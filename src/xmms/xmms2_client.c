/**
 *
 */

#include <stdio.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

guint played_time=0;


const char *playtime_message[]={"org.xmms.core.playtime-changed"};
const char *mediainfo_message[]={"org.xmms.core.mediainfo-changed"};

static DBusHandlerResult
handle_playtime(DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
        DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_UINT32) {
		guint tme = dbus_message_iter_get_uint32 (&itr);
		if (tme != played_time) {
			played_time = tme;
			printf ("played time: %02d:%02d\r",
				tme/60000,
				(tme/1000)%60);
			fflush (stdout);
		}
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

static DBusHandlerResult
handle_mediainfo(DBusMessageHandler *handler, 
	   DBusConnection *conn, DBusMessage *msg, void *user_data)
{
        DBusMessageIter itr;

	dbus_message_iter_init (msg, &itr);
	if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
		gchar *uri = dbus_message_iter_get_string (&itr);
		printf ("playing uri: %s\n", uri);
		fflush (stdout);
	}

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}

int
main(int argc, char **argv)
{
	GMainLoop *mainloop;
	DBusConnection *conn;
	DBusMessage *msg, *res;
	DBusError err;
	DBusMessageHandler *hand;

	mainloop = g_main_loop_new (NULL, FALSE);

	dbus_error_init (&err);

	conn = dbus_connection_open ("unix:path=/tmp/xmms-dbus", &err);
	
	if (!conn) {
		printf ("error opening connection\n");
		return 1;
	}

	hand = dbus_message_handler_new (handle_playtime, NULL, NULL);
	dbus_connection_register_handler (conn, hand, playtime_message, 1);

	hand = dbus_message_handler_new (handle_mediainfo, NULL, NULL);
	dbus_connection_register_handler (conn, hand, mediainfo_message, 1);


	msg = dbus_message_new ("org.xmms.core.mediainfo", NULL);
	
	res = dbus_connection_send_with_reply_and_block (conn, msg, 2000, &err);

	if (res) {
		DBusMessageIter itr;
		
		dbus_message_iter_init (res, &itr);
		if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
			gchar *url = dbus_message_iter_get_string (&itr);
			printf ("Playing: %s\n",url);
			url = NULL;
		} else {
			printf ("bad response :(\n");
		}
	}

	dbus_connection_flush (conn);

	dbus_connection_setup_with_g_main (conn, NULL);

        g_main_loop_run (mainloop);

	return 0;

}
