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




#include <xmms/xmmsclient.h>
#include <xmms/xmmswatch.h>

#include <glib.h>

#include <stdio.h>

typedef struct xmms_gsource_St {
	GSource source;
	GMainContext *context;
	xmmsc_connection_t *conn;
	GList *w_list;
} xmmsc_gsource_t;

static int
watch_callback (xmmsc_connection_t *conn,
		xmmsc_watch_action_t action,
		void *data)
{
	GSource *source;
	xmmsc_gsource_t *x_source;

	source = xmmsc_watch_data_get (conn);
	x_source = (xmmsc_gsource_t*) source;

	switch (action) {
		case XMMSC_WATCH_ADD:
			{
				GPollFD *poll_fd;
				xmmsc_watch_t *watch = data;

				poll_fd = g_new0 (GPollFD, 1);

				poll_fd->fd = watch->fd;
			
				if (watch->flags & XMMSC_WATCH_IN)
					poll_fd->events |= G_IO_IN;
				if (watch->flags & XMMSC_WATCH_OUT)
					poll_fd->events |= G_IO_OUT;

				poll_fd->events |= G_IO_ERR | G_IO_HUP;

				g_source_add_poll ((GSource*) source, poll_fd);
				watch->data = poll_fd;
				x_source->w_list = g_list_append (x_source->w_list, watch);
			}
			
			break;
		case XMMSC_WATCH_REMOVE:
			{
				xmmsc_watch_t *watch = data;
				
				g_source_remove_poll ((GSource*) source, 
						      (GPollFD*)watch->data);

				g_free (watch->data);
				x_source->w_list = g_list_remove (x_source->w_list, watch);
			}
			break;
		case XMMSC_TIMEOUT_ADD:
			{
				GSource *s = NULL;
				xmmsc_timeout_t *timeout = data;
				
				s = g_timeout_source_new (timeout->interval);
				g_source_set_callback (s, timeout->cb, timeout, NULL);
				g_source_attach (s, x_source->context);
				timeout->data = s;
			}
			break;
		case XMMSC_TIMEOUT_REMOVE:
			{
				guint id;
				xmmsc_timeout_t *timeout = data;
				GSource *s = timeout->data;
				id = g_source_get_id (s);
				
				if (id != 0) {
					g_source_destroy (s);
					g_source_unref (s);
				}
			}
			break;
		case XMMSC_WATCH_WAKEUP:
			g_main_context_wakeup (x_source->context);
			break;
	}

	return TRUE;
}

static gboolean
gsource_connection_prepare (GSource *source, gint *timeout)
{
	xmmsc_gsource_t *x_source = (xmmsc_gsource_t *)source;

	*timeout = -1;

	return xmmsc_watch_more (x_source->conn);
}

static gboolean
gsource_connection_check (GSource *source)
{
	GPollFD *poll_fd;
	GList *n;
	xmmsc_gsource_t *x_source = (xmmsc_gsource_t *)source;


	for (n = x_source->w_list; n; n = g_list_next (n)) {
		xmmsc_watch_t *w = n->data;
		poll_fd = w->data;

		if (poll_fd->revents != 0)
			return TRUE;
	}

	return FALSE;
}

static gboolean
gsource_connection_dispatch (GSource *source, 
			     GSourceFunc callback, 
			     gpointer data)
{
	GPollFD *poll_fd;
	GList *n;
	xmmsc_gsource_t *x_source = (xmmsc_gsource_t *)source;
	guint flags = 0;

	for (n = x_source->w_list; n; n = g_list_next (n)) {
		xmmsc_watch_t *w = n->data;

		if (!w)
			continue;

		poll_fd = (GPollFD *) w->data;

		if (poll_fd->revents == 0)
			continue;

		if (poll_fd->revents & G_IO_IN)
			flags |= XMMSC_WATCH_IN;
		if (poll_fd->revents & G_IO_OUT)
			flags |= XMMSC_WATCH_OUT;
		if (poll_fd->revents & G_IO_ERR)
			flags |= XMMSC_WATCH_ERROR;
		if (poll_fd->revents & G_IO_HUP)
			flags |= XMMSC_WATCH_HANGUP;

		xmmsc_watch_dispatch (x_source->conn, w, flags);
	}

	return TRUE;
}

static GSource *
create_source (xmmsc_connection_t *conn, 
	       GSourceFuncs *funcs, 
	       GMainContext *context)
{
	GSource *source;
	xmmsc_gsource_t *x_source;

	source = g_source_new (funcs, sizeof (xmmsc_gsource_t));
	x_source = (xmmsc_gsource_t *)source;
	x_source->conn = conn;
	x_source->context = context;

	return source;
}

static GSourceFuncs xmmsc_connection_funcs = {
	gsource_connection_prepare,
	gsource_connection_check,
	gsource_connection_dispatch,
	NULL
};

/**
 * @defgroup XMMSWatchGLIB XMMSWatchGLIB
 * @brief GLIB bindnings to XMMSWatch.
 * 
 * These are the functions you want to use from a gmainloop client
 * If your client is written with QT, see XMMSWatchQT
 *
 * @ingroup XMMSWatch
 *
 * @{
 */


/**
 * Add a xmmsc_connection_t to a GMainLoop.
 * Call this after you called xmmsc_connect().
 *
 * @sa xmmsc_connect
 */

void
xmmsc_setup_with_gmain (xmmsc_connection_t *connection,
			GMainContext *context)
{

	GSource *source;

	source = create_source (connection, &xmmsc_connection_funcs, context);
	xmmsc_watch_callback_set (connection, watch_callback);
	xmmsc_watch_data_set (connection, source);

	xmmsc_watch_init (connection);

	g_source_attach (source, context);
}

/* @} */
