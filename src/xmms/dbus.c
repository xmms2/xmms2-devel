/** @file
 * D-BUS handler.
 *
 * This code is responsible of setting up a D-BUS server that clients
 * can connect to. It serializes xmms-signals into dbus-messages that
 * are passed to the clients. The clients can send dbus-messages that
 * causes xmms-signals to be emitted.
 *
 * @url http://www.freedesktop.org/software/dbus
 *
 */

#include "util.h"
#include "core.h"

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

static DBusServer *server;
static DBusConnection *connections; /** @todo should be (hash)table */

static const char *messages[]={"org.xmms.test"};
static const char *disconnectmsgs[]={"org.freedesktop.Local.Disconnect"};

static void
handle_playtime_changed (xmms_object_t *object, gconstpointer data, gpointer userdata) {

	/* connectionslock.down */

	/* foreach connection */
	if (connections) { 
		DBusMessage *msg;
		DBusMessageIter itr;
		int clientser;

		msg = dbus_message_new (NULL, "org.xmms.core.playtime-changed");
		dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT(data));
		dbus_connection_send (connections, msg, &clientser);

	}

	/* connectionslock.up */

}


static DBusHandlerResult
handle_next(DBusMessageHandler *handler, 
	    DBusConnection *conn, DBusMessage *msg, void *user_data){

	if (strcmp (dbus_message_get_name (msg), "org.xmms.test") == 0) {

		XMMS_DBG ("Got org.xmms.test message!");

		return DBUS_HANDLER_RESULT_REMOVE_MESSAGE;

	} else {
		XMMS_DBG (" kooonstigt meddelande?");
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_disconnect (DBusMessageHandler *handler,
		   DBusConnection     *connection,
		   DBusMessage        *message,
                   void               *user_data) {

	dbus_connection_unref (connection);
	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static void 
new_connect (DBusServer *server, DBusConnection *conn, void * data){
	DBusMessageHandler *hand;
	XMMS_DBG ("new connection");
	dbus_connection_ref (conn);

	hand = dbus_message_handler_new (handle_next, NULL, NULL);

	if (!hand) {
		XMMS_DBG ("couldn't alloc handler");
		dbus_connection_unref (conn);
		return;
	}

// to get all messages
//	dbus_connection_add_filter (conn, hand);

	dbus_connection_register_handler (conn, hand, messages, 1);

	hand = dbus_message_handler_new (handle_disconnect, NULL, NULL);

	if (!hand) {
		XMMS_DBG ("couldn't alloc handler\n");
		dbus_connection_unref (conn);
		return;
	}

	dbus_connection_register_handler (conn, hand, disconnectmsgs, 1);

	connections = conn; /* -> add to (hash)table... */

	dbus_connection_setup_with_g_main (conn);

}



/**
 * Initialize dbus.
 *
 * Creates a dbus server that waits for connections and registers
 * it with g_main.
 *
 * @returns TRUE if server successfully created, FALSE otherwise.
 *
 */
gboolean
xmms_dbus_init(){
        DBusError err;

	dbus_gthread_init ();

        dbus_error_init (&err);

        server = dbus_server_listen ("unix:path=/tmp/xmms-dbus", &err);
        if (!server) {
                XMMS_DBG ("couldn't create server!\n");
                return FALSE;
        }
        dbus_server_ref (server);
        dbus_server_set_new_connection_function (server, new_connect,
						 NULL, NULL);

        dbus_server_setup_with_g_main (server);

	xmms_object_connect (core, "playtime-changed", 
			     handle_playtime_changed, NULL);

	XMMS_DBG ("init done!");

        return TRUE;
}
