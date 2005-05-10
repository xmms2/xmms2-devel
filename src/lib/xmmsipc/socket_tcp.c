/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */


#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#include "xmmsc/xmmsc_ipc_transport.h"
#include "xmmsc/xmmsc_util.h"

void
xmms_ipc_tcp_destroy (xmms_ipc_transport_t *ipct)
{
	free (ipct->path);
	close (ipct->fd);
}

int
xmms_ipc_tcp_read (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	int fd;
	int ret;
	x_return_val_if_fail (ipct, -1);
	x_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	ret =  recv (fd, buffer, len, 0);

	return ret;
}

int
xmms_ipc_tcp_write (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	int fd;
	x_return_val_if_fail (ipct, -1);
	x_return_val_if_fail (buffer, -1);
	
	fd = ipct->fd;

	return send (fd, buffer, len, 0);

}

xmms_ipc_transport_t *
xmms_ipc_tcp_client_init (const char *path)
{
	int fd;
	int flags;
	char *host, *port;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_in saddr;
	struct hostent *hent;
	struct servent *sent;


	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	host = strdup (path);
	port = strchr (host, ':');
	if (port) {
		*port = '\0';
		port++;
	}

	hent = gethostbyname (host);
	if (!hent) {
		close (fd);
		free (host);
		return NULL;
	}

	memset (&saddr, '\0', sizeof (saddr));
	memcpy (&saddr.sin_addr, *hent->h_addr_list, sizeof (struct in_addr));

	saddr.sin_family = AF_INET;

	if (!port) {
		saddr.sin_port = htons (5555);
	} else {
		sent = getservbyname (port, "tcp");
		if (!sent) {
			saddr.sin_port = htons ((uint16_t) strtoul (port, NULL, 0));
		} else {
			saddr.sin_port = (uint16_t) sent->s_port;
		}
	}

	free (host);

	if (connect (fd, (struct sockaddr *) &saddr, sizeof (saddr)) == -1) {
		close (fd);
		return NULL;
	}

	flags = fcntl (fd, F_GETFL, 0);

	if (flags == -1) {
		close (fd);
		return NULL;
	}

	flags |= O_NONBLOCK;

	flags = fcntl (fd, F_SETFL, flags);
	if (flags == -1) {
		close (fd);
		return NULL;
	}

	ipct = x_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = strdup (path);
	ipct->read_func = xmms_ipc_tcp_read;
	ipct->write_func = xmms_ipc_tcp_write;
	ipct->destroy_func = xmms_ipc_tcp_destroy;

	return ipct;
}

xmms_ipc_transport_t *
xmms_ipc_tcp_accept (xmms_ipc_transport_t *transport)
{
	int fd;
	struct sockaddr_in sin;
	socklen_t sin_len;

	x_return_val_if_fail (transport, NULL);

	sin_len = sizeof (sin);

	fd = accept (transport->fd, (struct sockaddr *)&sin, &sin_len);
	if (fd >= 0) {
		int flags;
		int one = 1;
		xmms_ipc_transport_t *ret;

		flags = fcntl (fd, F_GETFL, 0);

		if (flags == -1) {
			close (fd);
			return NULL;
		}

		flags |= O_NONBLOCK;

		flags = fcntl (fd, F_SETFL, flags);
		if (flags == -1) {
			close (fd);
			return NULL;
		}

		setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, (char *) &one, sizeof (one));
		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (one));

		ret = x_new0 (xmms_ipc_transport_t, 1);
		ret->fd = fd;
		ret->read_func = xmms_ipc_tcp_read;
		ret->write_func = xmms_ipc_tcp_write;
		ret->destroy_func = xmms_ipc_tcp_destroy;

		return ret;
	}

	return NULL;
}

xmms_ipc_transport_t *
xmms_ipc_tcp_server_init (const char *path)
{
	int fd;
	int flags;
	char *host, *port;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_in saddr;
	struct hostent *hent;
	struct servent *sent;
	int off = 1;


	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	host = strdup (path);
	port = strchr (host, ':');
	if (port) {
		*port = '\0';
		port++;
	}

	hent = gethostbyname (host);
	if (!hent) {
		close (fd);
		free (host);
		return NULL;
	}

	memset (&saddr, '\0', sizeof (saddr));
	memcpy (&saddr.sin_addr, *hent->h_addr_list, sizeof (struct in_addr));

	saddr.sin_family = AF_INET;

	if (!port) {
		saddr.sin_port = htons (5555);
	} else {
		sent = getservbyname (port, "tcp");
		if (!sent) {
			saddr.sin_port = htons ((uint16_t) strtoul (port, NULL, 0));
		} else {
			saddr.sin_port = (uint16_t) sent->s_port;
		}
	}

	free (host);

	if (bind (fd, (struct sockaddr *) &saddr, sizeof (saddr)) == -1) {
		close (fd);
		return NULL;
	}

	listen (fd, 5);

	flags = fcntl (fd, F_GETFL, 0);

	if (flags == -1) {
		close (fd);
		return NULL;
	}

	flags |= O_NONBLOCK;

	flags = fcntl (fd, F_SETFL, flags);
	if (flags == -1) {
		close (fd);
		return NULL;
	}
		
	setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, (char *) &off, sizeof (off));

	/* Make sure that we can use this socket again... good thing */
	setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (char *) &off, sizeof (off));
		
	ipct = x_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = strdup (path);
	ipct->read_func = xmms_ipc_tcp_read;
	ipct->write_func = xmms_ipc_tcp_write;
	ipct->accept_func = xmms_ipc_tcp_accept;
	ipct->destroy_func = xmms_ipc_tcp_destroy;

	return ipct;
}

