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

#include "xmms/ipc_transport.h"
#include "socket_unix.h"

void
xmms_ipc_transport_destroy (xmms_ipc_transport_t *ipct)
{
	g_return_if_fail (ipct);

	ipct->destroy_func (ipct);

	g_free (ipct);
}

gint
xmms_ipc_transport_read (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	return ipct->read_func (ipct, buffer, len);
}

gint
xmms_ipc_transport_write (xmms_ipc_transport_t *ipct, gchar *buffer, gint len)
{
	return ipct->write_func (ipct, buffer, len);
}

gint
xmms_ipc_transport_fd_get (xmms_ipc_transport_t *ipct)
{
	g_return_val_if_fail (ipct, -1);
	return ipct->fd;
}

xmms_ipc_transport_t *
xmms_ipc_server_accept (xmms_ipc_transport_t *ipct)
{
	g_return_val_if_fail (ipct, NULL);

	if (!ipct->accept_func)
		return NULL;

	return ipct->accept_func (ipct);
}

xmms_ipc_transport_t *
xmms_ipc_client_init (const gchar *path)
{
	xmms_ipc_transport_t *transport = NULL;

	g_return_val_if_fail (path, NULL);

	if (g_strncasecmp (path, "unix://", 7) == 0) {
		transport = xmms_ipc_usocket_client_init (path+7);
	} else if (g_strncasecmp (path, "tcp://", 6) == 0) {
/*		transport = xmms_ipc_tcp_server_init (path+6);*/
	}

	return transport;
}

xmms_ipc_transport_t *
xmms_ipc_server_init (const gchar *path)
{
	xmms_ipc_transport_t *transport = NULL;

	g_return_val_if_fail (path, NULL);

	if (g_strncasecmp (path, "unix://", 7) == 0) {
		transport = xmms_ipc_usocket_server_init (path+7);
	} else if (g_strncasecmp (path, "tcp://", 6) == 0) {
/*		transport = xmms_ipc_tcp_server_init (path+6);*/
	}

	return transport;
}
