/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

typedef struct {
	GSource source;
	GPollFD pollfd;
	xmmsc_connection_t *conn;
} xmmsc_glib_watch_t;

static gboolean
xmmsc_glib_prepare (GSource *source, gint *timeout)
{
	xmmsc_glib_watch_t *watch = (xmmsc_glib_watch_t *) source;

	*timeout = -1;

	watch->pollfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	if (xmmsc_io_want_out (watch->conn))
		watch->pollfd.events |= G_IO_OUT;

	return FALSE;
}

static gboolean
xmmsc_glib_check (GSource *source)
{
	xmmsc_glib_watch_t *watch = (xmmsc_glib_watch_t *) source;

	return !!watch->pollfd.revents;
}

static gboolean
xmmsc_glib_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	xmmsc_glib_watch_t *watch = (xmmsc_glib_watch_t *) source;

	if (watch->pollfd.revents & G_IO_IN)
		xmmsc_io_in_handle (watch->conn);

	if (watch->pollfd.revents & G_IO_OUT)
		xmmsc_io_out_handle (watch->conn);

	if (watch->pollfd.revents & (G_IO_HUP | G_IO_ERR)) {
		xmmsc_io_disconnect (watch->conn);
		return FALSE;
	}

	return TRUE;
}

GSourceFuncs xmmsc_glib_funcs = {
	.prepare = xmmsc_glib_prepare,
	.check = xmmsc_glib_check,
	.dispatch = xmmsc_glib_dispatch,
};

void *
xmmsc_mainloop_gmain_init (xmmsc_connection_t *c)
{
	xmmsc_glib_watch_t *src;

	src = (xmmsc_glib_watch_t *)g_source_new (&xmmsc_glib_funcs,
	                                          sizeof (xmmsc_glib_watch_t));
	src->pollfd.fd = xmmsc_io_fd_get (c);
	src->pollfd.events = G_IO_IN | G_IO_HUP | G_IO_ERR;
	src->conn = c;
	g_source_add_poll ((GSource *)src, &src->pollfd);

	g_source_attach ((GSource *)src, NULL);

	return src;
}

void
xmmsc_mainloop_gmain_shutdown (xmmsc_connection_t *c, void *udata)
{
	g_source_destroy (udata);
}
