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




#include "xmms/xmmsclient.h"
#include "xmms/xmmswatch.h"

#include "internal/xmmsclient_int.h"

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <unistd.h>
#include <stdlib.h>


int
xmmsc_watch_more (xmmsc_connection_t *conn)
{
	return (dbus_connection_get_dispatch_status (conn->conn) ==
			DBUS_DISPATCH_DATA_REMAINS);
}

void
xmmsc_watch_dispatch (xmmsc_connection_t *conn, 
		      xmmsc_watch_t *watch, 
		      unsigned int event)
{
	unsigned int condition = 0;


	if (event & XMMSC_WATCH_ERROR) {
		condition |= DBUS_WATCH_ERROR;
	}
	if (event & XMMSC_WATCH_HANGUP) {
		condition |= DBUS_WATCH_HANGUP;
	}
	if (event & XMMSC_WATCH_IN) {
		condition |= DBUS_WATCH_READABLE;
	}
	if (event & XMMSC_WATCH_OUT) {
		condition |= DBUS_WATCH_WRITABLE;
	}
		
	dbus_connection_ref (conn->conn);

	dbus_watch_handle (watch->dbus_watch, condition);

	while (dbus_connection_dispatch (conn->conn) == DBUS_DISPATCH_DATA_REMAINS)
		;

	dbus_connection_unref (conn->conn);

}

static void
xmmsc_watch_free (xmmsc_watch_t *watch)
{
	free (watch);
}

/* Watch functions */
static dbus_bool_t
watch_add (DBusWatch *watch,
	    void * data)
{
	xmmsc_connection_t *conn = data;
	xmmsc_watch_t *x_watch;
	unsigned int flags = 0;
	
	if (!dbus_watch_get_enabled (watch))
		return TRUE;

	x_watch = calloc (1, sizeof (xmmsc_watch_t));
	x_watch->fd = dbus_watch_get_fd (watch);
	x_watch->dbus_watch = watch;

	flags = dbus_watch_get_flags (watch);
	if (flags & DBUS_WATCH_READABLE)
		x_watch->flags |= XMMSC_WATCH_IN;
	if (flags & DBUS_WATCH_WRITABLE)
		x_watch->flags |= XMMSC_WATCH_OUT;
	
	conn->watch_callback (conn, XMMSC_WATCH_ADD, x_watch);

	dbus_watch_set_data (watch, x_watch, (DBusFreeFunction) xmmsc_watch_free);
	
	return TRUE;
}

static void
watch_remove (DBusWatch *watch,
	      void * data)
{
	xmmsc_connection_t *conn = data;
	xmmsc_watch_t *x_watch;

	x_watch = dbus_watch_get_data (watch);
	if (!x_watch)
		return; /* not enabled when added */

	conn->watch_callback (conn, XMMSC_WATCH_REMOVE, x_watch);

	x_watch->dbus_watch = NULL;
	dbus_watch_set_data (watch, NULL, NULL); /* Seems strange to me */

}

static void
watch_toggled (DBusWatch *watch,
	       void * data)
{
	if (dbus_watch_get_enabled (watch))
		watch_add (watch, data);
	else
		watch_remove (watch, data);
}

/* Timeout functions */
static void
timeout_free (xmmsc_connection_t *conn)
{
}

static int
timeout_handler (void * data)
{
	xmmsc_timeout_t *time = data;
	dbus_timeout_handle (time->dbus_timeout);
	return TRUE;
}

static dbus_bool_t
timeout_add (DBusTimeout *timeout,
	     void * data)
{

	xmmsc_connection_t *conn = data;
	xmmsc_timeout_t *time;
	
	if (!dbus_timeout_get_enabled (timeout))
		return TRUE;
	
	time = calloc (1, sizeof (xmmsc_timeout_t));
	time->dbus_timeout = timeout;
	time->interval = dbus_timeout_get_interval (timeout);
	time->cb = timeout_handler;

	dbus_timeout_set_data (timeout, time, NULL);

	conn->watch_callback (conn, XMMSC_TIMEOUT_ADD, time);

	return TRUE;
}

static void
timeout_remove (DBusTimeout *timeout,
		void * data)
{
	xmmsc_timeout_t *time;
	xmmsc_connection_t *conn = data;

	time = dbus_timeout_get_data (timeout);
	if (!time)
		return;
	
	conn->watch_callback (conn, XMMSC_TIMEOUT_REMOVE, time);

	time->dbus_timeout = NULL;
	dbus_timeout_set_data (timeout, NULL, NULL); /* Seems strange to me */
}

static void
timeout_toggled (DBusTimeout *timeout,
		 void * data)
{
	if (dbus_timeout_get_enabled (timeout))
		timeout_add (timeout, data);
	else
		timeout_remove (timeout, data);

}

/* wakeup function */
static void
wakeup_main (void *data)
{
	xmmsc_connection_t *conn = data;

	conn->watch_callback (conn, XMMSC_WATCH_WAKEUP, NULL);
}

/* xmmsc_watch functions */

/**
 * @defgroup XMMSWatch XMMSWatch
 *
 * These functions is used by xmmsclient-qt and xmmsclient-glib.
 *
 * @ingroup XMMSClient
 *
 * @{
 */


/**
 * Set the watch callback on this connection.
 */
void
xmmsc_watch_callback_set (xmmsc_connection_t *conn, xmmsc_watch_callback_t cb)
{
	conn->watch_callback = cb;
}

/**
 * Set private data for this watch.
 */

void
xmmsc_watch_data_set (xmmsc_connection_t *connection, void * data)
{
	connection->data = data;
}

/**
 * Get private data from watch.
 */

void *
xmmsc_watch_data_get (xmmsc_connection_t *connection)
{
	return connection->data;
}

/**
 * Init a xmms watch on a DBusConnection.
 */

void
xmmsc_watch_init (xmmsc_connection_t *connection)
{

	dbus_connection_set_watch_functions (connection->conn,
					     watch_add,
					     watch_remove,
					     watch_toggled,
					     connection, NULL);

	dbus_connection_set_timeout_functions (connection->conn,
					       timeout_add,
					       timeout_remove,
					       timeout_toggled,
					       connection, NULL);

	dbus_connection_set_wakeup_main_function (connection->conn,
						  wakeup_main,
						  connection, NULL);

}

/**
 * @}
 */
