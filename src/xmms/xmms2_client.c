/**
 *
 */

#include <stdio.h>
#include <time.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

static DBusHandlerResult
handle_msg(DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data){

        DBusMessageIter itr;

	printf("got message: %s\n", dbus_message_get_name (msg));

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint tme;
		tme = dbus_message_iter_get_uint32 (&itr);

		printf(" played time: %d -- %lu\n",tme/1000, time(NULL));

	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

int
main(int argc, char **argv) {
	GMainLoop *mainloop;
	DBusConnection *conn;
	DBusMessage *msg;
	DBusError err;
	dbus_uint32_t clientser;
	DBusMessageHandler *hand;

	mainloop = g_main_loop_new (NULL, FALSE);

	dbus_error_init (&err);

	conn = dbus_connection_open ("unix:path=/tmp/xmms-dbus", &err);
	
	if (!conn) {
		printf ("error opening connection\n");
		return 1;
	}

	hand = dbus_message_handler_new (handle_msg, NULL, NULL);
	dbus_connection_add_filter (conn, hand);


	msg = dbus_message_new (NULL, "org.xmms.test");
	
	dbus_connection_send (conn, msg, &clientser);

	dbus_connection_flush (conn);

	dbus_connection_setup_with_g_main (conn);

        g_main_loop_run (mainloop);

	return 0;

}
