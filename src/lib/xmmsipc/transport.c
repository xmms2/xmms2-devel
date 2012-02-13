/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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
#include <string.h>

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

xmms_socket_t
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

char *
xmms_ipc_hostname (const char *path)
{
	xmms_url_t *url;
	char* ret = NULL;

	url = parse_url (path);
	if (!strcasecmp (url->protocol, "tcp")) {
		if (strlen (url->host)) {
			ret = strdup (url->host);
		}
	}
	free_url (url);

	return ret;
}

