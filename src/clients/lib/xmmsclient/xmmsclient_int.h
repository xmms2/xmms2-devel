#ifndef __XMMSCLIENT_INT_H__
#define __XMMSCLIENT_INT_H__

#define DBUS_API_SUBJECT_TO_CHANGE

#include <dbus/dbus.h>
#include <glib.h>
#include "xmmswatch.h"

/**
 * @typedef xmmsc_connection_t
 *
 * Holds all data about the current connection to
 * the XMMS server.
 */

struct xmmsc_connection_St {
	DBusConnection *conn;	
	GHashTable *callbacks;
	GHashTable *replies;
	gchar *error;
	gint timeout;
	xmmsc_watch_callback_t watch_callback;
	gpointer data;
};

#endif

