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

#include "xmmsclient/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient.h"
#include "xmmsclientpriv/xmmsclient_ipc.h"
#include "xmmsc/xmmsc_idnumbers.h"

typedef struct {
	GSource source;
	GPollFD pollfd;
	xmmsc_ipc_t *ipc;
} xmmsc_ipc_glib_watch_t;

gboolean
xmmsc_ipc_glib_prepare (GSource *source, gint *timeout)
{
	xmmsc_ipc_glib_watch_t *ipcwatch = (xmmsc_ipc_glib_watch_t *) source;

	*timeout = -1;

	ipcwatch->pollfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (xmmsc_ipc_io_out (ipcwatch->ipc))
		ipcwatch->pollfd.events |= G_IO_OUT;

	return FALSE;
}

gboolean
xmmsc_ipc_glib_check (GSource *source)
{
	xmmsc_ipc_glib_watch_t *ipcwatch = (xmmsc_ipc_glib_watch_t *) source;

	return !!ipcwatch->pollfd.revents;
}

gboolean
xmmsc_ipc_glib_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	xmmsc_ipc_glib_watch_t *ipcwatch = (xmmsc_ipc_glib_watch_t *) source;

	if (ipcwatch->pollfd.revents & G_IO_IN)
		xmmsc_ipc_io_in_callback (ipcwatch->ipc);

	if (ipcwatch->pollfd.revents & G_IO_OUT)
		xmmsc_ipc_io_out_callback (ipcwatch->ipc);

	if (ipcwatch->pollfd.revents & (G_IO_HUP | G_IO_ERR)) {
		xmmsc_ipc_error_set (ipcwatch->ipc, "Remote host disconnected, or something!");
		xmmsc_ipc_disconnect (ipcwatch->ipc);
	}

	return TRUE;
}


GSourceFuncs xmmsc_ipc_glib_funcs = {
	.prepare = xmmsc_ipc_glib_prepare,
	.check = xmmsc_ipc_glib_check,
	.dispatch = xmmsc_ipc_glib_dispatch,
};

gboolean
xmmsc_ipc_setup_with_gmain (xmmsc_connection_t *c)
{
	xmmsc_ipc_t *ipc = c->ipc;
	xmmsc_ipc_glib_watch_t *src;

	src = (xmmsc_ipc_glib_watch_t *)g_source_new (&xmmsc_ipc_glib_funcs,
						      sizeof (xmmsc_ipc_glib_watch_t));
	src->pollfd.fd = xmmsc_ipc_fd_get (ipc);
	src->pollfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	src->ipc = ipc;
	g_source_add_poll ((GSource *)src, &src->pollfd);

	g_source_attach ((GSource *)src, NULL);

	return TRUE;
}

