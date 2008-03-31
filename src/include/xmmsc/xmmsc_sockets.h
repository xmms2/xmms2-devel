#ifndef XMMSC_SOCKETS_H
#define XMMSC_SOCKETS_H

#include <xmmsc/xmmsc_stdbool.h>

/* Windows */
#ifdef HAVE_WINSOCK2
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET xmms_socket_t;
typedef int socklen_t;
#define XMMS_EINTR WSAEINTR
#define XMMS_EAGAIN WSAEWOULDBLOCK
#define XMMS_EINPROGRESS WSAEINPROGRESS
/* UNIX */
#else
#define SOCKET_ERROR (-1)
#define XMMS_EINTR EINTR
#define XMMS_EINPROGRESS EINPROGRESS
#ifdef __hpux
/* on HP-UX EAGAIN != EWOULDBLOCK */
#define XMMS_EAGAIN EAGAIN
#else
#define XMMS_EAGAIN EWOULDBLOCK
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
typedef int xmms_socket_t;
#endif

int xmms_sockets_initialize(void);
int xmms_socket_set_nonblock(xmms_socket_t socket);
int xmms_socket_valid(xmms_socket_t socket);
void xmms_socket_close(xmms_socket_t socket);
int xmms_socket_errno(void);
bool xmms_socket_error_recoverable(void);
int xmms_getaddrinfo (const char *node, const char *service,
                      const struct addrinfo *hints, struct addrinfo **res);
void xmms_freeaddrinfo (struct addrinfo *res);

#endif
