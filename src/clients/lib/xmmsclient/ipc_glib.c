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

struct xmmsc_ipc_glib_St {
	GSource *source;
	GPollFD *pollfd;
};

typedef struct xmmsc_ipc_glib_St xmmsc_ipc_glib_t;
typedef gboolean (*xmmsc_ipc_callback_t) (GSource *, xmmsc_ipc_t *);


static void xmmsc_ipc_glib_destroy (xmmsc_ipc_glib_t *gipc);

static gboolean
xmmsc_ipc_source_prepare (GSource *source, gint *timeout_)
{
	/* No timeout here */
	return FALSE;
}

static gboolean
xmmsc_ipc_source_check (GSource *source)
{
	/* Maybe check for errors here? */
	return TRUE;
}

static gboolean
xmmsc_ipc_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	xmmsc_ipc_t *ipc = user_data;
	xmmsc_ipc_glib_t *gipc;
	gboolean ret;

	gipc = xmmsc_ipc_private_data_get (ipc);

	if (gipc->pollfd->revents & G_IO_ERR || gipc->pollfd->revents & G_IO_HUP) {
		xmmsc_ipc_glib_destroy (gipc);
		xmmsc_ipc_error_set (ipc, "Remote host disconnected, or something!");
		xmmsc_ipc_disconnect (ipc);
		return FALSE;
	} else if (gipc->pollfd->revents & G_IO_IN) {
		ret = xmmsc_ipc_io_in_callback (ipc);
	}

	return ret;
}


static GSourceFuncs xmmsc_ipc_callback_funcs = {
	xmmsc_ipc_source_prepare,
	xmmsc_ipc_source_check,
	xmmsc_ipc_source_dispatch,
	NULL
};

static void
xmmsc_ipc_glib_destroy (xmmsc_ipc_glib_t *gipc)
{
	if (gipc->pollfd) {
		g_source_remove_poll (gipc->source, gipc->pollfd);
		g_free (gipc->pollfd);
	}
	if (gipc->source) {
		g_source_remove (g_source_get_id (gipc->source));
		g_source_destroy (gipc->source);
	}

	g_free (gipc);
}

gboolean
xmmsc_ipc_setup_with_gmain (xmmsc_connection_t *c, xmmsc_ipc_callback_t callback)
{
	xmmsc_ipc_glib_t *gipc;
	xmmsc_ipc_t *ipc = c->ipc;

	gipc = g_new0 (xmmsc_ipc_glib_t, 1);

	gipc->pollfd = g_new0 (GPollFD, 1);
	gipc->pollfd->fd = xmmsc_ipc_fd_get (ipc);
	gipc->pollfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;

	gipc->source = g_source_new (&xmmsc_ipc_callback_funcs, sizeof (GSource));

	xmmsc_ipc_private_data_set (ipc, gipc);

	g_source_set_callback (gipc->source, 
			       (GSourceFunc)callback,
			       (gpointer) ipc, NULL);

	g_source_add_poll (gipc->source, gipc->pollfd);
	g_source_attach (gipc->source, NULL);

	return TRUE;
}
