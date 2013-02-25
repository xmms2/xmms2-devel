/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>

#include <xmmsc/xmmsc_ipc_transport.h>
#include <xmmsc/xmmsc_util.h>
#include <xmmsc/xmmsc_sockets.h>
#include <xmmsc/xmmsc_unistd.h>
#include "url.h"
#include "socket_tcp.h"

#include <xmmscpriv/xmmsc_util.h>

static void
xmms_ipc_tcp_destroy (xmms_ipc_transport_t *ipct)
{
	free (ipct->path);
	close (ipct->fd);
}

static int
xmms_ipc_tcp_read (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	xmms_socket_t fd;
	int ret;
	x_return_val_if_fail (ipct, -1);
	x_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	ret =  recv (fd, buffer, len, 0);

	return ret;
}

static int
xmms_ipc_tcp_write (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	xmms_socket_t fd;
	x_return_val_if_fail (ipct, -1);
	x_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	return send (fd, buffer, len, 0);

}

xmms_ipc_transport_t *
xmms_ipc_tcp_client_init (const xmms_url_t *url, int ipv6)
{
	xmms_socket_t fd = -1;
	xmms_ipc_transport_t *ipct;
	struct addrinfo hints;
	struct addrinfo *addrinfo;
	struct addrinfo *addrinfos;
	int gai_errno;

	if (!xmms_sockets_initialize ()) {
		return NULL;
	}

	memset (&hints, 0, sizeof (hints));
	hints.ai_flags = 0;
	hints.ai_family = url->host[0] ? (ipv6 ? PF_INET6 : PF_INET) : PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	if ((gai_errno = xmms_getaddrinfo (url->host[0] ? url->host : NULL, url->port[0] ? url->port : XMMS_STRINGIFY (XMMS_DEFAULT_TCP_PORT), &hints, &addrinfos))) {
		return NULL;
	}

	for (addrinfo = addrinfos; addrinfo; addrinfo = addrinfo->ai_next) {
		int _reuseaddr = 1;
		const char* reuseaddr = (const char*)&_reuseaddr;

		fd = socket (addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
		if (!xmms_socket_valid (fd)) {
			return NULL;
		}

		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, reuseaddr, sizeof (_reuseaddr));

		if (connect (fd, addrinfo->ai_addr, addrinfo->ai_addrlen) == 0) {
			break;
		}

		close (fd);
	}

	xmms_freeaddrinfo (addrinfos);

	if (!addrinfo) {
		return NULL;
	}

	assert (fd != -1);

	if (!xmms_socket_set_nonblock (fd)) {
		close (fd);
		return NULL;
	}

	ipct = x_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = strdup (url->host);
	ipct->read_func = xmms_ipc_tcp_read;
	ipct->write_func = xmms_ipc_tcp_write;
	ipct->destroy_func = xmms_ipc_tcp_destroy;

	return ipct;
}

static xmms_ipc_transport_t *
xmms_ipc_tcp_accept (xmms_ipc_transport_t *transport)
{
	xmms_socket_t fd;
	struct sockaddr sockaddr;
	socklen_t socklen;

	x_return_val_if_fail (transport, NULL);

	socklen = sizeof (sockaddr);

	fd = accept (transport->fd, &sockaddr, &socklen);
	if (xmms_socket_valid (fd)) {
		int _reuseaddr = 1;
		int _nodelay = 1;
		const char* reuseaddr = (const char*)&_reuseaddr;
		const char* nodelay = (const char*)&_nodelay;
		xmms_ipc_transport_t *ret;

		if (!xmms_socket_set_nonblock (fd)) {
			close (fd);
			return NULL;
		}

		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, reuseaddr, sizeof (_reuseaddr));
		setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, nodelay, sizeof (_nodelay));

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
xmms_ipc_tcp_server_init (const xmms_url_t *url, int ipv6)
{
	xmms_socket_t fd = -1;
	xmms_ipc_transport_t *ipct;
	struct addrinfo hints;
	struct addrinfo *addrinfo;
	struct addrinfo *addrinfos;
	int gai_errno;

	if (!xmms_sockets_initialize ()) {
		return NULL;
	}

	memset (&hints, 0, sizeof (hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = url->host[0] ? (ipv6 ? PF_INET6 : PF_INET) : PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	if ((gai_errno = xmms_getaddrinfo (url->host[0] ? url->host : NULL, url->port[0] ? url->port : XMMS_STRINGIFY (XMMS_DEFAULT_TCP_PORT), &hints, &addrinfos))) {
		return NULL;
	}

	for (addrinfo = addrinfos; addrinfo; addrinfo = addrinfo->ai_next) {
		int _reuseaddr = 1;
		int _nodelay = 1;
		const char* reuseaddr = (const char*)&_reuseaddr;
		const char* nodelay = (const char*)&_nodelay;

		fd = socket (addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
		if (!xmms_socket_valid (fd)) {
			return NULL;
		}

		setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, reuseaddr, sizeof (_reuseaddr));
		setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, nodelay, sizeof (_nodelay));

		if (bind (fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != SOCKET_ERROR &&
		    listen (fd, SOMAXCONN) != SOCKET_ERROR) {
			break;
		}
		close (fd);
	}

	xmms_freeaddrinfo (addrinfos);

	if (!addrinfo) {
		return NULL;
	}

	assert (fd != -1);

	if (!xmms_socket_set_nonblock (fd)) {
		close (fd);
		return NULL;
	}

	ipct = x_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = strdup (url->host);
	ipct->read_func = xmms_ipc_tcp_read;
	ipct->write_func = xmms_ipc_tcp_write;
	ipct->accept_func = xmms_ipc_tcp_accept;
	ipct->destroy_func = xmms_ipc_tcp_destroy;

	return ipct;
}

