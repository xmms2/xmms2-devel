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
#include "playlist.h"
#include "core.h"

#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

static DBusServer *server;
static DBusConnection *connections; /** @todo should be (hash)table */

static const char *messages[]={"org.xmms.core.mediainfo"};
static const char *disconnectmsgs[]={"org.freedesktop.Local.Disconnect"};

static void
handle_playtime_changed (xmms_object_t *object, gconstpointer data, gpointer userdata) {

	/* connectionslock.down */

	/* foreach connection */
	if (connections) { 
		DBusMessage *msg;
		DBusMessageIter itr;
		int clientser;

		msg = dbus_message_new ("org.xmms.core.playtime-changed", NULL);
		dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_uint32 (&itr, GPOINTER_TO_UINT(data));
		dbus_connection_send (connections, msg, &clientser);
		dbus_message_unref (msg);
	}

	/* connectionslock.up */

}

static void
handle_mediainfo_changed (xmms_object_t *object, gconstpointer data, gpointer userdata)
{

	/* connectionslock.down */

	/* foreach connection */
	if (connections) { 
		DBusMessage *msg;
		DBusMessageIter itr;
		int clientser;

		msg = dbus_message_new ("org.xmms.core.mediainfo-changed", NULL);
		dbus_message_append_iter_init (msg, &itr);
		dbus_message_iter_append_string (&itr, xmms_core_get_uri ());
		dbus_connection_send (connections, msg, &clientser);
		dbus_message_unref (msg);
	}

	/* connectionslock.up */

}


static DBusHandlerResult
handle_mediainfo(DBusMessageHandler *handler, 
		 DBusConnection *conn, DBusMessage *msg, void *user_data)
{
	DBusMessage *rpy;
	DBusMessageIter itr;
	int clientser;
	char *uri;
	
	rpy = dbus_message_new_reply (msg);
	dbus_message_append_iter_init (rpy, &itr);
	uri = xmms_core_get_uri ();
	dbus_message_iter_append_string (&itr, uri);
	dbus_connection_send (connections, rpy, &clientser);
	dbus_message_unref (rpy);
	dbus_message_unref (msg);

	g_free (uri);

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_disconnect (DBusMessageHandler *handler,
		   DBusConnection     *connection,
		   DBusMessage        *message,
                   void               *user_data) {

	XMMS_DBG ("disconnect");
	connections = NULL;
//	dbus_message_unref (message);
//	dbus_connection_unref (connection);
	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


static void 
new_connect (DBusServer *server, DBusConnection *conn, void * data){
	DBusMessageHandler *hand;

	XMMS_DBG ("new connection");

	g_assert(connections==NULL);

	dbus_connection_ref (conn);

	hand = dbus_message_handler_new (handle_mediainfo, NULL, NULL);

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

	dbus_connection_setup_with_g_main (conn, NULL);

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

	//dbus_gthread_init ();  dbus_enable_deadlocks ();

        dbus_error_init (&err);

        server = dbus_server_listen ("unix:path=/tmp/xmms-dbus", &err);
        if (!server) {
                XMMS_DBG ("couldn't create server!\n");
                return FALSE;
        }
        dbus_server_ref (server);
        dbus_server_set_new_connection_function (server, new_connect,
						 NULL, NULL);
	
	dbus_server_setup_with_g_main (server, NULL);
	
	xmms_object_connect (XMMS_OBJECT (core), "playtime-changed",
			     handle_playtime_changed, NULL);
	
	xmms_object_connect (XMMS_OBJECT (core), "mediainfo-changed",
			     handle_mediainfo_changed, NULL);



	XMMS_DBG ("init done!");

        return TRUE;
}
