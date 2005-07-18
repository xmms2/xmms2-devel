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

#include <stdlib.h>
#include <strings.h>

#include "xmmsc/xmmsc_util.h"
#include "xmmsc/xmmsc_ipc_transport.h"
#include "socket_unix.h"
#include "socket_tcp.h"
#include "url.h"

void
xmms_ipc_transport_destroy (xmms_ipc_transport_t *ipct)
{
	x_return_if_fail (ipct);

	ipct->destroy_func (ipct);

	free (ipct);
}

int
xmms_ipc_transport_read (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	return ipct->read_func (ipct, buffer, len);
}

int
xmms_ipc_transport_write (xmms_ipc_transport_t *ipct, char *buffer, int len)
{
	return ipct->write_func (ipct, buffer, len);
}

int
xmms_ipc_transport_fd_get (xmms_ipc_transport_t *ipct)
{
	x_return_val_if_fail (ipct, -1);
	return ipct->fd;
}

xmms_ipc_transport_t *
xmms_ipc_server_accept (xmms_ipc_transport_t *ipct)
{
	x_return_val_if_fail (ipct, NULL);

	if (!ipct->accept_func)
		return NULL;

	return ipct->accept_func (ipct);
}

xmms_ipc_transport_t *
xmms_ipc_client_init (const char *path)
{
	xmms_ipc_transport_t *transport = NULL;
	xmms_url_t *url;

	x_return_val_if_fail (path, NULL);

	url = parse_url (path);
	x_return_val_if_fail (url, NULL);

	if (!strcasecmp (url->protocol, "") || !strcasecmp (url->protocol, "unix")) {
		transport = xmms_ipc_usocket_client_init (url);
	} else if (!strcasecmp (url->protocol, "tcp")) {
		transport = xmms_ipc_tcp_client_init (url, url->ipv6_host);
	}

	free_url (url);
	return transport;
}

xmms_ipc_transport_t *
xmms_ipc_server_init (const char *path)
{
	xmms_ipc_transport_t *transport = NULL;
	xmms_url_t *url;

	x_return_val_if_fail (path, NULL);

	url = parse_url (path);
	x_return_val_if_fail (url, NULL);

	if (!strcasecmp (url->protocol, "") || !strcasecmp (url->protocol, "unix")) {
		transport = xmms_ipc_usocket_server_init (url);
	} else if (!strcasecmp (url->protocol, "tcp")) {
		transport = xmms_ipc_tcp_server_init (url, url->ipv6_host);
	}

	free_url (url);
	return transport;
}
