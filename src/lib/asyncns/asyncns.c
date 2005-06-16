/* 
 * Note! This file was "borrowed" by libasyncns but fits perfectly
 * in xmms2. The contents of this file may be modifed by xmms2-team
 */

/***
  This file is part of libasyncns.
 
  libasyncns is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.
 
  libasyncns is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with libasyncns; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*#undef HAVE_PTHREAD */

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pwd.h>

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#if HAVE_PTHREAD
#include <pthread.h>
#endif

#include "xmmsc/xmmsc_asyncns.h"

#define MAX_WORKERS 16
#define MAX_QUERIES 256
#define BUFSIZE (10240)

typedef enum {
    REQUEST_ADDRINFO,
    RESPONSE_ADDRINFO,
    REQUEST_NAMEINFO,
    RESPONSE_NAMEINFO,
    REQUEST_TERMINATE
} query_type_t;

enum {
    REQUEST_RECV_FD = 0,
    REQUEST_SEND_FD = 1,
    RESPONSE_RECV_FD = 2,
    RESPONSE_SEND_FD = 3
};

struct asyncns {
    int fds[4];

#ifndef HAVE_PTHREAD    
    pid_t workers[MAX_WORKERS];
#else
    pthread_t workers[MAX_WORKERS];
#endif
    unsigned valid_workers;

    unsigned current_id, current_index;
    asyncns_query_t* queries[MAX_QUERIES];

    asyncns_query_t *done_head, *done_tail;

    int n_queries;
};

struct asyncns_query {
    asyncns_t *asyncns;
    int done;
    unsigned id;
    query_type_t type;
    asyncns_query_t *done_next, *done_prev;
    int ret;
    struct addrinfo *addrinfo;
    char *serv, *host;
    void *userdata;
};

typedef struct rheader {
    query_type_t type;
    unsigned id;
    size_t length;
} rheader_t;

typedef struct addrinfo_request {
    struct rheader header;
    int hints_is_null;
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t node_len, service_len;
} addrinfo_request_t;

typedef struct addrinfo_response {
    struct rheader header;
    int ret;
} addrinfo_response_t;

typedef struct addrinfo_serialization {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    size_t canonname_len;
} addrinfo_serialization_t;

typedef struct nameinfo_request {
    struct rheader header;
    int flags;
    socklen_t sockaddr_len;
    int gethost, getserv;
} nameinfo_request_t;

typedef struct nameinfo_response {
    struct rheader header;
    size_t hostlen, servlen;
    int ret;
} nameinfo_response_t;

#ifndef HAVE_STRNDUP

static char *strndup(const char *s, size_t l) {
    size_t a;
    char *n;

    a = strlen(s);
    if (a > l)
        a = l;

    if (!(n = malloc(a+1)))
        return NULL;

    strncpy(n, s, a);
    n[a] = 0;

    return n;
}

#endif

static int fd_nonblock(int fd) {
    int i;
    assert(fd >= 0);

    if ((i = fcntl(fd, F_GETFL, 0)) < 0)
        return -1;

    if (i & O_NONBLOCK)
        return 0;

    return fcntl(fd, F_SETFL, i | O_NONBLOCK);
}

static int fd_cloexec(int fd) {
    int v;
    assert(fd >= 0);

    if ((v = fcntl(fd, F_GETFD, 0)) < 0)
        return -1;

    if (v & FD_CLOEXEC)
        return 0;
    
    return fcntl(fd, F_SETFD, v | FD_CLOEXEC);
}


static void *serialize_addrinfo(void *p, const struct addrinfo *ai, size_t *length, size_t maxlength) {
    addrinfo_serialization_t *s = p;
    size_t cnl, l;
    assert(p);
    assert(ai);
    assert(length);
    assert(*length <= maxlength);

    cnl = (ai->ai_canonname ? strlen(ai->ai_canonname)+1 : 0);
    l = sizeof(addrinfo_serialization_t) + ai->ai_addrlen + cnl;

    if (*length + l > maxlength)
        return NULL;

    s->ai_flags = ai->ai_flags;
    s->ai_family = ai->ai_family;
    s->ai_socktype = ai->ai_socktype;
    s->ai_protocol = ai->ai_protocol;
    s->ai_addrlen = ai->ai_addrlen;
    s->canonname_len = cnl;

    memcpy((uint8_t*) p + sizeof(addrinfo_serialization_t), ai->ai_addr, ai->ai_addrlen);

    if (ai->ai_canonname)
        strcpy((char*) p + sizeof(addrinfo_serialization_t) + ai->ai_addrlen, ai->ai_canonname);

    *length += l;
    return (uint8_t*) p + l;
}

static int send_addrinfo_reply(int out_fd, unsigned id, int ret, struct addrinfo *ai) {
    uint8_t data[BUFSIZE];
    addrinfo_response_t *resp = (addrinfo_response_t*) data;
    assert(out_fd >= 0);

    resp->header.type = RESPONSE_ADDRINFO;
    resp->header.id = id;
    resp->header.length = sizeof(addrinfo_response_t);
    resp->ret = ret;

    if (ret == 0 && ai) {
        void *p = data + sizeof(addrinfo_response_t);

        while (ai && p) {
            p = serialize_addrinfo(p, ai, &resp->header.length, BUFSIZE);
            ai = ai->ai_next;
        }
    }

    if (ai)
        freeaddrinfo(ai);

    return send(out_fd, resp, resp->header.length, 0);
}

static int send_nameinfo_reply(int out_fd, unsigned id, int ret, const char *host, const char *serv) {
    uint8_t data[BUFSIZE];
    size_t hl, sl;
    nameinfo_response_t *resp = (nameinfo_response_t*) data;

    assert(out_fd >= 0);
    
    sl = serv ? strlen(serv)+1 : 0;
    hl = host ? strlen(host)+1 : 0;

    resp->header.type = RESPONSE_NAMEINFO;
    resp->header.id = id;
    resp->header.length = sizeof(nameinfo_response_t) + hl + sl;
    resp->ret = ret;
    resp->hostlen = hl;
    resp->servlen = sl;

    assert(sizeof(data) >= resp->header.length);
    
    if (host)
        memcpy(data + sizeof(nameinfo_response_t), host, hl);

    if (serv)
        memcpy(data + sizeof(nameinfo_response_t) + hl, serv, sl);
    
    return send(out_fd, resp, resp->header.length, 0);
}

static int handle_request(int out_fd, const rheader_t *req, size_t length) {
    assert(out_fd >= 0);
    assert(req);
    assert(length >= sizeof(rheader_t));
    assert(length == req->length);

    switch (req->type) {
        case REQUEST_ADDRINFO: {
            struct addrinfo ai, *result = NULL;
            const addrinfo_request_t *ai_req = (addrinfo_request_t*) req;
            const char *node, *service;
            int ret;

            assert(length >= sizeof(addrinfo_request_t));
            assert(length == sizeof(addrinfo_request_t) + ai_req->node_len + ai_req->service_len);

            memset(&ai, 0, sizeof(ai));
            ai.ai_flags = ai_req->ai_flags;
            ai.ai_family = ai_req->ai_family;
            ai.ai_socktype = ai_req->ai_socktype;
            ai.ai_protocol = ai_req->ai_protocol;

            node = ai_req->node_len ? (const char*) req + sizeof(addrinfo_request_t) : NULL;
            service = ai_req->service_len ? (const char*) req + sizeof(addrinfo_request_t) + ai_req->node_len : NULL;

            ret = getaddrinfo(node, service,
                              ai_req->hints_is_null ? NULL : &ai,
                              &result);

            /* send_addrinfo_reply() frees result */
            return send_addrinfo_reply(out_fd, req->id, ret, result);
        }

        case REQUEST_NAMEINFO: {
            int ret;
            const nameinfo_request_t *ni_req = (nameinfo_request_t*) req;
            char hostbuf[NI_MAXHOST], servbuf[NI_MAXSERV];
            const struct sockaddr *sa;
            
            assert(length >= sizeof(nameinfo_request_t));
            assert(length == sizeof(nameinfo_request_t) + ni_req->sockaddr_len);

            sa = (const struct sockaddr*) ((const char*) req + sizeof(nameinfo_request_t));
            
            ret = getnameinfo(sa, ni_req->sockaddr_len,
                              ni_req->gethost ? hostbuf : NULL, ni_req->gethost ? sizeof(hostbuf) : 0,
                              ni_req->getserv ? servbuf : NULL, ni_req->getserv ? sizeof(servbuf) : 0,
                              ni_req->flags);

            return send_nameinfo_reply(out_fd, req->id, ret,
                                       ret == 0 && ni_req->gethost ? hostbuf : NULL,
                                       ret == 0 && ni_req->getserv ? servbuf : NULL);
        }

        case REQUEST_TERMINATE: {
            /* Quit */
            return -1;
        }

        default:
            ;
    }

    return 0;
}

#ifndef HAVE_PTHREAD

static int process_worker(int in_fd, int out_fd) {
    int have_death_sig = 0;
    assert(in_fd > 2);
    assert(out_fd > 2);
    
    close(0);
    close(1);
    close(2);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);

    chdir("/");

    if (geteuid() == 0) {
        struct passwd *pw;

        if ((pw = getpwnam("nobody"))) {
#ifdef HAVE_SETRESUID            
            setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid);
#elif HAVE_SETREUID
            setreuid(pw->pw_uid, pw->pw_uid);
#else
            setuid(pw->pw_uid);
            seteuid(pw->pw_uid);
#endif
        }
    }

    signal(SIGTERM, SIG_DFL);

    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);

#ifdef PR_SET_PDEATHSIG
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) >= 0)
        have_death_sig = 1;
#endif

    if (!have_death_sig)
        fd_nonblock(in_fd);
    
    while (getppid() > 1) { /* if the parent PID is 1 our parent process died. */
        char buf[BUFSIZE];
        ssize_t length;

        if (!have_death_sig) {
            fd_set fds;
            struct timeval tv = { 0, 500000 } ;
            
            FD_ZERO(&fds);
            FD_SET(in_fd, &fds);
            
            if (select(in_fd+1, &fds, NULL, NULL, &tv) < 0)
                break;
            
            if (getppid() == 1)
                break;
        }
        
        if ((length = recv(in_fd, buf, sizeof(buf), 0)) <= 0) {

            if (length < 0 && errno == EAGAIN)
                continue;

            break;
        }

        if (handle_request(out_fd, (rheader_t*) buf, (size_t) length) < 0)
            break;
    }

    close(in_fd);
    close(out_fd);
    
    return 0;
}

#else

static void* thread_worker(void *p) {
    sigset_t fullset;
    int *fds = p;
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    /* No signals in this thread please */
    sigfillset(&fullset);
    pthread_sigmask(SIG_BLOCK, &fullset, NULL);
    
    for (;;) {
        char buf[BUFSIZE];
        ssize_t length;

        if ((length = recv(fds[REQUEST_RECV_FD], buf, sizeof(buf), 0)) <= 0)
            break;

        if (handle_request(fds[RESPONSE_SEND_FD], (rheader_t*) buf, (size_t) length) < 0)
            break;
        
    }

    return NULL;
}

#endif

asyncns_t* asyncns_new(unsigned n_proc) {
    asyncns_t *asyncns = NULL;
    int i;
    unsigned p;
    assert(n_proc >= 1);

    if (n_proc > MAX_WORKERS)
        n_proc = MAX_WORKERS;

    if (!(asyncns = malloc(sizeof(asyncns_t))))
        goto fail;

    asyncns->valid_workers = 0;

    for (i = 0; i < 4; i++) 
        asyncns->fds[i] = -1;
        
    for (p = 0; p < MAX_QUERIES; p++)
        asyncns->queries[p] = NULL;
    
    if (socketpair(PF_UNIX, SOCK_DGRAM, 0, asyncns->fds) < 0 ||
        socketpair(PF_UNIX, SOCK_DGRAM, 0, asyncns->fds+2) < 0)
        goto fail;
    
    for (i = 0; i < 4; i++) 
        fd_cloexec(asyncns->fds[i]);

    for (asyncns->valid_workers = 0; asyncns->valid_workers < n_proc; asyncns->valid_workers++) {

#ifndef HAVE_PTHREAD
        if ((asyncns->workers[asyncns->valid_workers] = fork()) < 0)
            goto fail;
        else if (asyncns->workers[asyncns->valid_workers] == 0) {
            close(asyncns->fds[REQUEST_SEND_FD]);
            close(asyncns->fds[RESPONSE_RECV_FD]);
            _exit(process_worker(asyncns->fds[REQUEST_RECV_FD], asyncns->fds[RESPONSE_SEND_FD]));
        }
#else
        if (pthread_create(&asyncns->workers[asyncns->valid_workers], NULL, thread_worker, asyncns->fds) != 0)
            goto fail;
#endif
    }

#ifndef HAVE_PTHREAD
    close(asyncns->fds[REQUEST_RECV_FD]);
    close(asyncns->fds[RESPONSE_SEND_FD]);
    asyncns->fds[REQUEST_RECV_FD] = asyncns->fds[RESPONSE_SEND_FD] = -1;
#endif    
    
    asyncns->current_index = asyncns->current_id = 0;
    asyncns->done_head = asyncns->done_tail = NULL;
    asyncns->n_queries = 0;

    fd_nonblock(asyncns->fds[RESPONSE_RECV_FD]);

    return asyncns;

fail:
    if (asyncns) 
        asyncns_free(asyncns);

    return NULL;
}

void asyncns_free(asyncns_t *asyncns) {
    unsigned p;
    int i;
    rheader_t req;
    assert(asyncns);

    req.type = REQUEST_TERMINATE;
    req.length = sizeof(req);
    req.id = 0;
    
    /* Send one termiantion packet for each worker */
    for (p = 0; p < asyncns->valid_workers; p++)
        send(asyncns->fds[REQUEST_SEND_FD], &req, req.length, 0);

    /* No terminate them forcibly*/
    for (p = 0; p < asyncns->valid_workers; p++) {
#ifndef HAVE_PTHREAD
        kill(asyncns->workers[p], SIGTERM);
        waitpid(asyncns->workers[p], NULL, 0);
#else
        pthread_cancel(asyncns->workers[p]);
        pthread_join(asyncns->workers[p], NULL);
#endif        
    }

    /* Due to Solaris' broken thread cancelation we first send an
     * termination request and then cancel th thread. */

    
    for (i = 0; i < 4; i++)
        if (asyncns->fds[i] >= 0)
            close(asyncns->fds[i]);
    
    for (p = 0; p < MAX_QUERIES; p++)
        if (asyncns->queries[p])
            asyncns_cancel(asyncns, asyncns->queries[p]);
    
    free(asyncns);
}

int asyncns_fd(asyncns_t *asyncns) {
    assert(asyncns);

    return asyncns->fds[RESPONSE_RECV_FD];
}

static asyncns_query_t *lookup_query(asyncns_t *asyncns, unsigned id) {
    asyncns_query_t *q;
    assert(asyncns);

    if ((q = asyncns->queries[id % MAX_QUERIES]))
        if (q->id == id)
            return q;

    return NULL;
}

static void complete_query(asyncns_t *asyncns, asyncns_query_t *q) {
    assert(asyncns);
    assert(q);
    assert(!q->done);

    q->done = 1;
    
    if ((q->done_prev = asyncns->done_tail))
        asyncns->done_tail->done_next = q;
    else
        asyncns->done_head = q;

    asyncns->done_tail = q;
    q->done_next = NULL;
}

static void *unserialize_addrinfo(void *p, struct addrinfo **ret_ai, size_t *length) {
    addrinfo_serialization_t *s = p;
    size_t l;
    struct addrinfo *ai;
    assert(p);
    assert(ret_ai);
    assert(length);

    if (*length < sizeof(addrinfo_serialization_t))
        return NULL;

    l = sizeof(addrinfo_serialization_t) + s->ai_addrlen + s->canonname_len;
    if (*length < l)
        return NULL;

    if (!(ai = malloc(sizeof(struct addrinfo))))
        goto fail;
    
    ai->ai_addr = NULL;
    ai->ai_canonname = NULL;
    ai->ai_next = NULL;

    if (s->ai_addrlen && !(ai->ai_addr = malloc(s->ai_addrlen)))
        goto fail;
    
    if (s->canonname_len && !(ai->ai_canonname = malloc(s->canonname_len)))
        goto fail;

    ai->ai_flags = s->ai_flags;
    ai->ai_family = s->ai_family;
    ai->ai_socktype = s->ai_socktype;
    ai->ai_protocol = s->ai_protocol;
    ai->ai_addrlen = s->ai_addrlen;

    if (ai->ai_addr)
        memcpy(ai->ai_addr, (uint8_t*) p + sizeof(addrinfo_serialization_t), s->ai_addrlen);

    if (ai->ai_canonname)
        memcpy(ai->ai_canonname, (uint8_t*) p + sizeof(addrinfo_serialization_t) + s->ai_addrlen, s->canonname_len);

    *length -= l;
    *ret_ai = ai;
    
    return (uint8_t*) p + l;


fail:
    if (ai)
        asyncns_freeaddrinfo(ai);

    return NULL;
}

static int handle_response(asyncns_t *asyncns, rheader_t *resp, size_t length) {
    asyncns_query_t *q;
    assert(asyncns);
    assert(resp);
    assert(length >= sizeof(rheader_t));
    assert(length == resp->length);

    if (!(q = lookup_query(asyncns, resp->id)))
        return 0;
    
    switch (resp->type) {
        case RESPONSE_ADDRINFO: {
            const addrinfo_response_t *ai_resp = (addrinfo_response_t*) resp;
            void *p;
            size_t l;
            struct addrinfo *prev = NULL;

            assert(length >= sizeof(addrinfo_response_t));
            assert(q->type == REQUEST_ADDRINFO);

            q->ret = ai_resp->ret;
            l = length - sizeof(addrinfo_response_t);
            p = (uint8_t*) resp + sizeof(addrinfo_response_t);

            while (l > 0 && p) {
                struct addrinfo *ai = NULL;
                p = unserialize_addrinfo(p, &ai, &l);

                if (!ai)
                    break;

                if (prev)
                    prev->ai_next = ai;
                else
                    q->addrinfo = ai;

                prev = ai;
            }

            complete_query(asyncns, q);
            break;
        }

        case RESPONSE_NAMEINFO: {
            const nameinfo_response_t *ni_resp = (nameinfo_response_t*) resp;

            assert(length >= sizeof(nameinfo_response_t));
            assert(q->type = REQUEST_NAMEINFO);

            q->ret = ni_resp->ret;

            if (ni_resp->hostlen)
                q->host = strndup((char*) ni_resp + sizeof(nameinfo_response_t), ni_resp->hostlen-1);

            if (ni_resp->servlen)
                q->serv = strndup((char*) ni_resp + sizeof(nameinfo_response_t) + ni_resp->hostlen, ni_resp->servlen-1);
                    

            complete_query(asyncns, q);
            break;
        }
            
        default:
            ;
    }
    
    return 0;
}

int asyncns_wait(asyncns_t *asyncns, int block) {
    int handled = 0;
    assert(asyncns);

    for (;;) {
        char buf[BUFSIZE];
        ssize_t l;

        if (((l = recv(asyncns->fds[RESPONSE_RECV_FD], buf, sizeof(buf), 0)) < 0)) {
            fd_set fds;

            if (errno != EAGAIN)
                return -1;

            if (!block || handled)
                return 0;
                
            FD_ZERO(&fds);
            FD_SET(asyncns->fds[RESPONSE_RECV_FD], &fds);

            if (select(asyncns->fds[RESPONSE_RECV_FD]+1, &fds, NULL, NULL, NULL) < 0)
                return -1;
                
            continue;
        }


        if (handle_response(asyncns, (rheader_t*) buf, (size_t) l) < 0)
            return -1;

        handled = 1;
    }
}

static asyncns_query_t *alloc_query(asyncns_t *asyncns) {
    asyncns_query_t *q;
    assert(asyncns);

    if (asyncns->n_queries >= MAX_QUERIES)
        return NULL;

    while (asyncns->queries[asyncns->current_index]) {

        asyncns->current_index++;
        asyncns->current_id++;

        while (asyncns->current_index >= MAX_QUERIES)
            asyncns->current_index -= MAX_QUERIES;
    }
        
    if (!(q = asyncns->queries[asyncns->current_index] = malloc(sizeof(asyncns_query_t))))
        return NULL;

    asyncns->n_queries++;

    q->asyncns = asyncns;
    q->done = 0;
    q->id = asyncns->current_id;
    q->done_next = q->done_prev = NULL;
    q->ret = 0;
    q->addrinfo = NULL;
    q->userdata = NULL;
    q->host = q->serv = NULL;

    return q;
}

asyncns_query_t* asyncns_getaddrinfo(asyncns_t *asyncns, const char *node, const char *service, const struct addrinfo *hints) {
    uint8_t data[BUFSIZE];
    addrinfo_request_t *req = (addrinfo_request_t*) data;
    asyncns_query_t *q;
    assert(asyncns);
    assert(node || service);

    if (!(q = alloc_query(asyncns)))
        return NULL;

    memset(req, 0, sizeof(addrinfo_request_t));
    
    req->node_len = node ? strlen(node)+1 : 0;
    req->service_len = service ? strlen(service)+1 : 0;
    
    req->header.id = q->id;
    req->header.type = q->type = REQUEST_ADDRINFO;
    req->header.length = sizeof(addrinfo_request_t) + req->node_len + req->service_len;

    if (req->header.length > BUFSIZE)
        goto fail;

    if (!(req->hints_is_null = !hints)) {
        req->ai_flags = hints->ai_flags; 
        req->ai_family = hints->ai_family;
        req->ai_socktype = hints->ai_socktype;
        req->ai_protocol = hints->ai_protocol;
    }

    if (node)
        strcpy((char*) req + sizeof(addrinfo_request_t), node);

    if (service)
        strcpy((char*) req + sizeof(addrinfo_request_t) + req->node_len, service);

    if (send(asyncns->fds[REQUEST_SEND_FD], req, req->header.length, 0) < 0)
        goto fail;
    
    return q;

fail:
    if (q)
        asyncns_cancel(asyncns, q);

    return NULL;
}

int asyncns_getaddrinfo_done(asyncns_t *asyncns, asyncns_query_t* q, struct addrinfo **ret_res) {
    int ret;
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(q->type == REQUEST_ADDRINFO);

    if (!q->done)
        return EAI_AGAIN;

    *ret_res = q->addrinfo;
    q->addrinfo = NULL;
    ret = q->ret;
    asyncns_cancel(asyncns, q);

    return ret;
}

asyncns_query_t* asyncns_getnameinfo(asyncns_t *asyncns, const struct sockaddr *sa, socklen_t salen, int flags, int gethost, int getserv) {
    uint8_t data[BUFSIZE];
    nameinfo_request_t *req = (nameinfo_request_t*) data;
    asyncns_query_t *q;

    assert(asyncns);
    assert(sa);
    assert(salen > 0);

    if (!(q = alloc_query(asyncns)))
        return NULL;

    memset(req, 0, sizeof(nameinfo_request_t));
    
    req->header.id = q->id;
    req->header.type = q->type = REQUEST_NAMEINFO;
    req->header.length = sizeof(nameinfo_request_t) + salen;

    if (req->header.length > BUFSIZE)
        goto fail;

    req->flags = flags;
    req->sockaddr_len = salen;
    req->gethost = gethost;
    req->getserv = getserv;

    memcpy((uint8_t*) req + sizeof(nameinfo_request_t), sa, salen);

    if (send(asyncns->fds[REQUEST_SEND_FD], req, req->header.length, 0) < 0)
        goto fail;
    
    return q;

fail:
    if (q)
        asyncns_cancel(asyncns, q);

    return NULL;
}

int asyncns_getnameinfo_done(asyncns_t *asyncns, asyncns_query_t* q, char *ret_host, size_t hostlen, char *ret_serv, size_t servlen) {
    int ret;
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(q->type == REQUEST_NAMEINFO);
    assert(!ret_host || hostlen);
    assert(!ret_serv || servlen);

    if (!q->done)
        return EAI_AGAIN;

    if (ret_host && q->host) {
        strncpy(ret_host, q->host, hostlen);
        ret_host[hostlen-1] = 0;
    }

    if (ret_serv && q->serv) {
        strncpy(ret_serv, q->serv, servlen);
        ret_serv[servlen-1] = 0;
    }
    
    ret = q->ret;
    asyncns_cancel(asyncns, q);

    return ret;
}

asyncns_query_t* asyncns_getnext(asyncns_t *asyncns) {
    assert(asyncns);
    return asyncns->done_head;
}

int asyncns_getnqueries(asyncns_t *asyncns) {
    assert(asyncns);
    return asyncns->n_queries;
}

void asyncns_cancel(asyncns_t *asyncns, asyncns_query_t* q) {
    int i;
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);
    assert(asyncns->n_queries > 0);

    if (q->done) {

        if (q->done_prev)
            q->done_prev->done_next = q->done_next;
        else 
            asyncns->done_head = q->done_next;

        if (q->done_next)
            q->done_next->done_prev = q->done_prev;
        else
            asyncns->done_tail = q->done_prev;
    }


    i = q->id % MAX_QUERIES;
    assert(asyncns->queries[i] == q);
    asyncns->queries[i] = NULL;

    asyncns_freeaddrinfo(q->addrinfo);
    free(q->addrinfo);
    free(q->host);
    free(q->serv);

    asyncns->n_queries--;
    free(q);
}

void asyncns_freeaddrinfo(struct addrinfo *ai) {
    if (!ai)
        return;

    while (ai) {
        struct addrinfo *next = ai->ai_next;

        free(ai->ai_addr);
        free(ai->ai_canonname);
        free(ai);

        ai = next;
    }
}

int asyncns_isdone(asyncns_t *asyncns, asyncns_query_t*q) {
    assert(asyncns);
    assert(q);
    assert(q->asyncns == asyncns);

    return q->done;
}

void asyncns_setuserdata(asyncns_t *asyncns, asyncns_query_t *q, void *userdata) {
    assert(q);
    assert(asyncns);
    assert(q->asyncns = asyncns);
    
    q->userdata = userdata;
}

void* asyncns_getuserdata(asyncns_t *asyncns, asyncns_query_t *q) {
    assert(q);
    assert(asyncns);
    assert(q->asyncns = asyncns);

    return q->userdata;
}


