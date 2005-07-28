#ifndef XMMSC_SOCKETS_H
#define XMMSC_SOCKETS_H

/* Windows */

#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
typedef SOCKET xmms_socket_t;
typedef int socklen_t;
#define XMMS_EINTR WSAEINTR
#define XMMS_EAGAIN WSAEWOULDBLOCK

/* UNIX */
#else
#define SOCKET_ERROR (-1)
#define XMMS_EINTR EINTR
#define XMMS_EAGAIN EWOULDBLOCK
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
typedef int xmms_socket_t;
#endif

/**
 * Tries to set socket to non-blocking mode.
 * @param socket Socket to make non-blocking.
 * On success, returns 1.
 * On failure, closes socket and returns 0.
 */
int xmms_sockets_initialize();
int xmms_socket_set_nonblock(xmms_socket_t socket);
int xmms_socket_valid(xmms_socket_t socket);
void xmms_socket_close(xmms_socket_t socket);
int xmms_socket_errno();


#endif
