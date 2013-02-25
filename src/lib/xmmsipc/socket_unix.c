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
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>

#include <xmmsc/xmmsc_ipc_transport.h>
#include <xmmscpriv/xmmsc_util.h>
#include "url.h"
#include "socket_unix.h"

static void
xmms_ipc_usocket_destroy (xmms_ipc_transport_t *ipct)
{
	free (ipct->path);
	close (ipct->fd);
}

static int
xmms_ipc_usocket_read (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	int fd;
	int ret;
	x_return_val_if_fail (ipct, -1);
	x_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	ret =  recv (fd, buffer, len, 0);

	return ret;
}

static int
xmms_ipc_usocket_write (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	int fd;
	x_return_val_if_fail (ipct, -1);
	x_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	return send (fd, buffer, len, 0);

}

xmms_ipc_transport_t *
xmms_ipc_usocket_client_init (const xmms_url_t *url)
{
	int fd;
	int flags;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_un saddr;


	fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	saddr.sun_family = AF_UNIX;
	snprintf (saddr.sun_path, sizeof(saddr.sun_path), "/%s", url->path);

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
	ipct->path = strdup (url->path);
	ipct->read_func = xmms_ipc_usocket_read;
	ipct->write_func = xmms_ipc_usocket_write;
	ipct->destroy_func = xmms_ipc_usocket_destroy;

	return ipct;
}

static xmms_ipc_transport_t *
xmms_ipc_usocket_accept (xmms_ipc_transport_t *transport)
{
	int fd;
	struct sockaddr_un sin;
	socklen_t sin_len;

	x_return_val_if_fail (transport, NULL);

	sin_len = sizeof (sin);

	fd = accept (transport->fd, (struct sockaddr *)&sin, &sin_len);
	if (fd >= 0) {
		int flags;
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


		ret = x_new0 (xmms_ipc_transport_t, 1);
		ret->fd = fd;
		ret->read_func = xmms_ipc_usocket_read;
		ret->write_func = xmms_ipc_usocket_write;
		ret->destroy_func = xmms_ipc_usocket_destroy;

		return ret;
	}

	return NULL;
}

xmms_ipc_transport_t *
xmms_ipc_usocket_server_init (const xmms_url_t *url)
{
	int fd;
	int flags;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_un saddr;


	fd = socket (AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	saddr.sun_family = AF_UNIX;
	snprintf (saddr.sun_path, sizeof (saddr.sun_path), "/%s", url->path);

	if (access (saddr.sun_path, F_OK) == 0) {
		if (connect (fd, (struct sockaddr *) &saddr, sizeof (saddr)) != -1) {
			/* active socket already exists! */
			close (fd);
			return NULL;
		}
		/* remove stale socket */
		unlink (saddr.sun_path);
	}

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

	ipct = x_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = strdup (url->path);
	ipct->read_func = xmms_ipc_usocket_read;
	ipct->write_func = xmms_ipc_usocket_write;
	ipct->accept_func = xmms_ipc_usocket_accept;
	ipct->destroy_func = xmms_ipc_usocket_destroy;

	return ipct;
}

