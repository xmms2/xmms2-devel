/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003, 2004 Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include <Ecore.h>
#include <stdio.h>
#include <assert.h>

static int
on_fd_data (void *udata, Ecore_Fd_Handler *handler)
{
	xmmsc_watch_t *watch = udata;
	void **data = watch->data;
	xmmsc_connection_t *conn = data[0];
	int flags = 0;

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_READ))
		flags |= XMMSC_WATCH_IN;

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_WRITE))
		flags |= XMMSC_WATCH_OUT;

	if (ecore_main_fd_handler_active_get (handler, ECORE_FD_ERROR))
		flags |= XMMSC_WATCH_ERROR;

	xmmsc_watch_dispatch (conn, watch, flags);

	return 1;
}

static int
watch_callback (xmmsc_connection_t *conn, xmmsc_watch_action_t action,
                void *data)
{
	xmmsc_watch_t *watch = data;
	void **watch_data;
	int flags = ECORE_FD_ERROR;

	switch (action) {
		case XMMSC_WATCH_ADD:
			if (watch->flags & XMMSC_WATCH_IN)
				flags |= ECORE_FD_READ;
			if (watch->flags & XMMSC_WATCH_OUT)
				flags |= ECORE_FD_WRITE;


			watch_data = malloc (2 * sizeof (void *));
			watch->data = watch_data;

			watch_data[0] = conn;
			watch_data[1] = ecore_main_fd_handler_add (watch->fd,
			                                           flags,
			                                           on_fd_data,
			                                           watch, NULL,
			                                           NULL);

			break;
		case XMMSC_WATCH_REMOVE:
			watch_data = watch->data;

			ecore_main_fd_handler_del (watch_data[1]);

			free (watch->data);
			break;
		default:
			break;
	}

	return TRUE;
}

/**
 * @defgroup XMMSWatchEcore XMMSWatchEcore
 * @brief Ecore bindings to XMMSWatch.
 *
 * These are the functions you want to use from a Ecore client
 * If your client is written with Glib, see XMMSWatchGLIB
 * If your client is written with QT, see XMMSWatchQT
 *
 * @ingroup XMMSWatch
 *
 * @{
 */


/**
 * Add a xmmsc_connection_t to the Ecore main loop.
 * Call this after you called xmmsc_connect() and ecore_init().
 *
 * @sa xmmsc_connect
 */

void
xmmsc_setup_with_ecore (xmmsc_connection_t *connection)
{
	xmmsc_watch_callback_set (connection, watch_callback);
	xmmsc_watch_init (connection);
}

/* @} */
