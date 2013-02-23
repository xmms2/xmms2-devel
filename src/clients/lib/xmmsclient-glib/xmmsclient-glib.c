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

#include <glib.h>

#include "xmmsclient/xmmsclient.h"
#include "xmmsclient/xmmsclient-glib.h"

typedef struct {
	xmmsc_connection_t *conn;
	GIOChannel *iochan;
	gboolean write_pending;
	guint source_id;
} xmmsc_glib_watch_t;

static gboolean
xmmsc_glib_read_cb (GIOChannel *iochan, GIOCondition cond, gpointer data)
{
	xmmsc_glib_watch_t *watch = data;
	gboolean ret = FALSE;

	g_return_val_if_fail (watch, FALSE);
	if (!(cond & G_IO_IN)) {
		xmmsc_io_disconnect (watch->conn);
	} else {
		ret = xmmsc_io_in_handle (watch->conn);
	}

	return ret;
}

static gboolean
xmmsc_glib_write_cb (GIOChannel *iochan, GIOCondition cond, gpointer data)
{
	xmmsc_glib_watch_t *watch = data;

	g_return_val_if_fail (watch, FALSE);
	xmmsc_io_out_handle (watch->conn);

	return !!xmmsc_io_want_out (watch->conn);
}

static void
xmmsc_mainloop_need_out_cb (gboolean need_out, void *data)
{
	xmmsc_glib_watch_t *watch = data;

	g_return_if_fail (watch);
	if (need_out && !watch->write_pending) {
		g_io_add_watch (watch->iochan,
		                G_IO_OUT,
		                xmmsc_glib_write_cb,
		                watch);
		watch->write_pending = TRUE;
	} else if (!need_out) {
		/* our write_cb has finished writing */
		watch->write_pending = FALSE;
	}
}

void *
xmmsc_mainloop_gmain_init (xmmsc_connection_t *c)
{
	xmmsc_glib_watch_t *watch;
	gint fd;

	g_return_val_if_fail (c, NULL);
	fd = xmmsc_io_fd_get (c);

	watch = g_new0 (xmmsc_glib_watch_t, 1);
	watch->conn = xmmsc_ref (c);
	watch->iochan = g_io_channel_unix_new (fd);

	xmmsc_io_need_out_callback_set (c, xmmsc_mainloop_need_out_cb, watch);
	watch->source_id = g_io_add_watch (watch->iochan,
	                                   G_IO_IN | G_IO_ERR | G_IO_HUP,
	                                   xmmsc_glib_read_cb,
	                                   watch);
	g_io_channel_unref (watch->iochan);

	if (xmmsc_io_want_out (c)) {
		xmmsc_mainloop_need_out_cb (TRUE, watch);
	}

	return watch;
}

void
xmmsc_mainloop_gmain_shutdown (xmmsc_connection_t *c, void *data)
{
	g_return_if_fail (data != NULL);

	xmmsc_glib_watch_t *watch = (xmmsc_glib_watch_t *) data;

	g_source_remove (watch->source_id);
	xmmsc_unref (watch->conn);

	g_free (watch);
}
