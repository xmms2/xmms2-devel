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


#include <glib.h>

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


#include "xmms/util.h"
#include "xmms/ipc_transport.h"

void
xmms_ipc_tcp_destroy (xmms_ipc_transport_t *ipct)
{
	g_free (ipct->path);
	close (ipct->fd);
}

gint
xmms_ipc_tcp_read (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	gint fd;
	gint ret;
	g_return_val_if_fail (ipct, -1);
	g_return_val_if_fail (buffer, -1);

	fd = ipct->fd;

	ret =  recv (fd, buffer, len, 0);

	return ret;
}

gint
xmms_ipc_tcp_write (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	gint fd;
	g_return_val_if_fail (ipct, -1);
	g_return_val_if_fail (buffer, -1);
	
	fd = ipct->fd;

	return send (fd, buffer, len, 0);

}

xmms_ipc_transport_t *
xmms_ipc_tcp_client_init (const gchar *path)
{
	gint fd;
	gint flags;
	gchar **tokens;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_in saddr;
	struct hostent *hent;
	struct servent *sent;

	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	tokens = g_strsplit (path, ":", 0);
	if (!tokens) {
		close (fd);
		return NULL;
	}

	hent = gethostbyname (tokens[0]);
	if (!hent) {
		close (fd);
		g_strfreev (tokens);
		return NULL;
	}

	memset (&saddr, '\0', sizeof (saddr));
	memcpy (&saddr.sin_addr, *hent->h_addr_list, sizeof (struct in_addr));

	saddr.sin_family = AF_INET;

	if (!tokens[1]) {
		saddr.sin_port = htons (5555);
	} else {
		sent = getservbyname (tokens[1], "tcp");
		if (!sent) {
			saddr.sin_port = htons ((guint16) strtoul (tokens[1], NULL, 0));
		} else {
			saddr.sin_port = (guint16) sent->s_port;
		}
	}

	g_strfreev (tokens);

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

	ipct = g_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = g_strdup (path);
	ipct->read_func = xmms_ipc_tcp_read;
	ipct->write_func = xmms_ipc_tcp_write;
	ipct->destroy_func = xmms_ipc_tcp_destroy;

	return ipct;
}

xmms_ipc_transport_t *
xmms_ipc_tcp_accept (xmms_ipc_transport_t *transport)
{
	gint fd;
	struct sockaddr_in sin;
	socklen_t sin_len;

	g_return_val_if_fail (transport, NULL);

	sin_len = sizeof (sin);

	fd = accept (transport->fd, (struct sockaddr *)&sin, &sin_len);
	if (fd >= 0) {
		gint flags;
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

		ret = g_new0 (xmms_ipc_transport_t, 1);
		ret->fd = fd;
		ret->read_func = xmms_ipc_tcp_read;
		ret->write_func = xmms_ipc_tcp_write;
		ret->destroy_func = xmms_ipc_tcp_destroy;

		return ret;
	} else {
		XMMS_DBG ("Accept error %s (%d)", strerror (errno), errno);
	}

	return NULL;
}

xmms_ipc_transport_t *
xmms_ipc_tcp_server_init (const gchar *path)
{
	gint fd;
	gint flags;
	gchar **tokens;
	xmms_ipc_transport_t *ipct;
	struct sockaddr_in saddr;
	struct hostent *hent;
	struct servent *sent;
	int off = 1;

	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		return NULL;
	}

	tokens = g_strsplit (path, ":", 0);
	if (!tokens) {
		close (fd);
		return NULL;
	}

	hent = gethostbyname (tokens[0]);
	if (!hent) {
		close (fd);
		g_strfreev (tokens);
		return NULL;
	}

	memset (&saddr, '\0', sizeof (saddr));
	memcpy (&saddr.sin_addr, *hent->h_addr_list, sizeof (struct in_addr));
	XMMS_DBG ("Binding to IP: %s", inet_ntoa (saddr.sin_addr));

	saddr.sin_family = AF_INET;

	if (!tokens[1]) {
		saddr.sin_port = htons (5555);
	} else {
		sent = getservbyname (tokens[1], "tcp");
		if (!sent) {
			saddr.sin_port = htons ((guint16) strtoul (tokens[1], NULL, 0));
		} else {
			saddr.sin_port = (guint16) sent->s_port;
		}
	}

	XMMS_DBG ("Binding to port: %u", (unsigned) ntohs (saddr.sin_port));
	g_strfreev (tokens);
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
		
	ipct = g_new0 (xmms_ipc_transport_t, 1);
	ipct->fd = fd;
	ipct->path = g_strdup (path);
	ipct->read_func = xmms_ipc_tcp_read;
	ipct->write_func = xmms_ipc_tcp_write;
	ipct->accept_func = xmms_ipc_tcp_accept;
	ipct->destroy_func = xmms_ipc_tcp_destroy;

	return ipct;
}

