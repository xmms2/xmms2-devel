#include "xmmsclient.h"
#include "xmmsclient_int.h"
#include "xmmswatch.h"

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <glib.h>


gboolean
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

	switch (event) {
		case XMMSC_WATCH_IN:
			condition = DBUS_WATCH_READABLE;
			break;
		case XMMSC_WATCH_OUT:
			condition = DBUS_WATCH_WRITABLE;
			break;
		case XMMSC_WATCH_ERROR:
			condition = DBUS_WATCH_ERROR;
			break;
		case XMMSC_WATCH_HANGUP:
			condition = DBUS_WATCH_HANGUP;
			break;
	}

	dbus_watch_handle (watch->dbus_watch, condition);

	dbus_connection_ref (conn->conn);

	while (dbus_connection_dispatch (conn->conn) == DBUS_DISPATCH_DATA_REMAINS)
		;

	dbus_connection_unref (conn->conn);

	return TRUE;
}

static void
xmmsc_watch_free (xmmsc_watch_t *watch)
{
	g_free (watch);
}

/* Watch functions */
static dbus_bool_t
watch_add (DBusWatch *watch,
	    gpointer data)
{
	xmmsc_connection_t *conn = data;
	xmmsc_watch_t *x_watch;
	guint flags = 0;
	
	g_return_val_if_fail (conn, FALSE);

	if (!dbus_watch_get_enabled (watch))
		return TRUE;

	x_watch = g_new0 (xmmsc_watch_t, 1);
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
	      gpointer data)
{
	xmmsc_connection_t *conn = data;
	xmmsc_watch_t *x_watch;

	g_return_if_fail (conn);

	x_watch = dbus_watch_get_data (watch);
	if (!x_watch)
		return; /* not enabled when added */

	conn->watch_callback (conn, XMMSC_WATCH_REMOVE, x_watch);

	x_watch->dbus_watch = NULL;
	dbus_watch_set_data (watch, NULL, NULL); /* Seems strange to me */

}

static void
watch_toggled (DBusWatch *watch,
	       gpointer data)
{
	if (dbus_watch_get_enabled (watch))
		watch_add (watch, data);
	else
		watch_remove (watch, data);
}

/* Timeout functions */
static void
timeout_free (xmmsc_timeout_t *time)
{
	if (time)
		g_free (time);
}

static gboolean
timeout_handler (gpointer data)
{
	xmmsc_timeout_t *time = data;
	dbus_timeout_handle (time->dbus_timeout);
	return TRUE;
}

static dbus_bool_t
timeout_add (DBusTimeout *timeout,
	     gpointer data)
{

	xmmsc_connection_t *conn = data;
	xmmsc_timeout_t *time;
	
	g_return_val_if_fail (conn, FALSE);

	if (!dbus_timeout_get_enabled (timeout))
		return TRUE;
	
	time = g_new0 (xmmsc_timeout_t, 1);
	time->dbus_timeout = timeout;
	time->interval = dbus_timeout_get_interval (timeout);
	time->cb = timeout_handler;

	dbus_timeout_set_data (timeout, time, (DBusFreeFunction)timeout_free);

	conn->watch_callback (conn, XMMSC_TIMEOUT_ADD, time);

	return FALSE;
}

static void
timeout_remove (DBusTimeout *timeout,
		gpointer data)
{
	xmmsc_timeout_t *time;
	xmmsc_connection_t *conn = data;

	g_return_if_fail (conn);

	time = dbus_timeout_get_data (timeout);
	if (!time)
		return;

	conn->watch_callback (conn, XMMSC_TIMEOUT_REMOVE, time);

	time->dbus_timeout = NULL;
	dbus_timeout_set_data (timeout, NULL, NULL); /* Seems strange to me */
}

static void
timeout_toggled (DBusTimeout *timeout,
		 gpointer data)
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
void
xmmsc_watch_callback_set (xmmsc_connection_t *conn, xmmsc_watch_callback_t cb)
{
	g_return_if_fail (conn);
	g_return_if_fail (cb);

	conn->watch_callback = cb;
}

void
xmmsc_watch_data_set (xmmsc_connection_t *connection, gpointer data)
{
	g_return_if_fail (connection);
	connection->data = data;
}

gpointer
xmmsc_watch_data_get (xmmsc_connection_t *connection)
{
	g_return_val_if_fail (connection, NULL);
	
	return connection->data;
}

void
xmmsc_watch_init (xmmsc_connection_t *connection)
{

	g_return_if_fail (connection);
	g_return_if_fail (connection->watch_callback);

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
