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

#include "internal/client_ipc.h"
#include "xmms/xmmsclient.h"
#include "internal/xmmsclient_int.h"

static gboolean
xmmsc_ipc_io_dispatch (GIOChannel *source, GIOCondition condition,
                       gpointer data)
{
	gboolean ret;
	xmmsc_ipc_t *ipc = (xmmsc_ipc_t *) data;

	if (condition & G_IO_ERR || condition & G_IO_HUP) {
		xmmsc_ipc_error_set (ipc, "Remote host disconnected, or something!");
		xmmsc_ipc_disconnect (ipc);
		ret = FALSE;
	} else if (condition & G_IO_IN) {
		ret = xmmsc_ipc_io_in_callback (ipc);
	}

	return ret;
}

gboolean
xmmsc_ipc_setup_with_gmain (xmmsc_connection_t *c)
{
	xmmsc_ipc_t *ipc = c->ipc;
	gint fd;
	GIOChannel *channel;

	fd = xmmsc_ipc_fd_get (ipc);
	channel = g_io_channel_unix_new (fd);

	g_io_add_watch (channel, G_IO_IN | G_IO_HUP | G_IO_ERR,
	                xmmsc_ipc_io_dispatch, ipc);

	return TRUE;
}
