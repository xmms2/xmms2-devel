/**
 *
 */

/* YES! I know that this api may change under my feet */
#define DBUS_API_SUBJECT_TO_CHANGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

guint played_time=0;

#define XMMS_MAX_URI_LEN 1024

static GMainLoop *mainloop;

static const char *playtime_message[]={"org.xmms.core.playtime-changed"};
static const char *mediainfo_message[]={"org.xmms.core.mediainfo-changed"};
static const char *disconnectmsgs[]={"org.freedesktop.Local.Disconnect"};

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


static DBusHandlerResult
handle_disconnect (DBusMessageHandler *handler,
		   DBusConnection     *connection,
		   DBusMessage        *message,
                   void               *user_data)
{

	printf("Someone set us up the bomb...\n");

	g_main_loop_quit (mainloop);

	dbus_connection_unref (connection);

	return DBUS_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
}


int
status_main(DBusConnection *conn)
{
	DBusMessage *msg, *res;
	DBusError err;
	DBusMessageHandler *hand;

	mainloop = g_main_loop_new (NULL, FALSE);

	dbus_error_init (&err);

	hand = dbus_message_handler_new (handle_playtime, NULL, NULL);
	dbus_connection_register_handler (conn, hand, playtime_message, 1);

	hand = dbus_message_handler_new (handle_mediainfo, NULL, NULL);
	dbus_connection_register_handler (conn, hand, mediainfo_message, 1);

	hand = dbus_message_handler_new (handle_disconnect, NULL, NULL);
	dbus_connection_register_handler (conn, hand, disconnectmsgs, 1);


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

	dbus_message_unref (msg);
	dbus_connection_flush (conn);

	dbus_connection_setup_with_g_main (conn, NULL);


	return 0;

}

#define streq(a,b) (strcmp(a,b)==0)

int
main(int argc, char **argv)
{
	DBusConnection *conn;
	DBusError err;

	dbus_error_init (&err);

	conn = dbus_connection_open ("unix:path=/tmp/xmms-dbus", &err);
	
	if (!conn) {
		int ret;
		
		printf ("Couldn't connect to xmms2d, starning it...\n");
		ret = system ("xmms2d -d");

		if (ret != 0) {
			perror ("Error starting xmms2d");
			exit (1);
		}
		printf ("started!\n");

		dbus_error_init (&err);
		conn = dbus_connection_open ("unix:path=/tmp/xmms-dbus", &err);
		if (!conn) {
			printf ("Couldn't connect to xmms2d even tough I started it... Bad, very bad.\n");
			exit (1);
		}
	}

	if (argc<2 || streq (argv[1], "status")) {
		status_main (conn);
	} else {

		if ( streq (argv[1], "next") ) {
			DBusMessage *msg;
			int cserial;

			msg = dbus_message_new ("org.xmms.core.play-next", NULL);
			dbus_connection_send (conn, msg, &cserial);
			dbus_message_unref (msg);
			dbus_connection_flush (conn);
			dbus_connection_disconnect (conn);
			dbus_connection_unref (conn);
			exit(0);
		} else if ( streq (argv[1], "quit") ) {
			DBusMessage *msg;
			int cserial;

			msg = dbus_message_new ("org.xmms.core.quit", NULL);
			dbus_connection_send (conn, msg, &cserial);
			dbus_message_unref (msg);
			dbus_connection_flush (conn);
			dbus_connection_disconnect (conn);
			dbus_connection_unref (conn);
			exit(0);
		} else if ( streq (argv[1], "list") ) {
			DBusMessage *msg,*res;
			int cserial;

			msg = dbus_message_new ("org.xmms.playlist.list", NULL);
			res = dbus_connection_send_with_reply_and_block (conn, msg, 2000, &err);
			if (res) {
				DBusMessageIter itr;
				
				dbus_message_iter_init (res, &itr);
				while (42) {
					if (dbus_message_iter_get_arg_type (&itr) == DBUS_TYPE_STRING) {
						gchar *url = dbus_message_iter_get_string (&itr);
						printf ("list: %s\n",url);
					}
					if (!dbus_message_iter_has_next (&itr)){
						break;
					}
					dbus_message_iter_next (&itr);
				}
				dbus_message_unref (res);
			} else {
				printf ("no response?\n");
			}
			
			dbus_message_unref (msg);
			dbus_connection_flush (conn);
			dbus_connection_disconnect (conn);
			dbus_connection_unref (conn);
			exit(0);
		} else if ( streq (argv[1], "add") ) {
			int i;
			if ( argc < 3 ) {
				printf ("usage: add url [url...]\n");
				return 1;
			}
			for (i=2;i<argc;i++) {
				DBusMessage *msg;
				DBusMessageIter itr;
				int cserial;
				gchar nuri[XMMS_MAX_URI_LEN];
				if (!strchr (argv[i], ':')) {
					if (argv[i][0] == '/') {
						g_snprintf (nuri, XMMS_MAX_URI_LEN, "file://%s", argv[i]);
					} else {
						gchar *cwd = g_get_current_dir ();
						g_snprintf (nuri, XMMS_MAX_URI_LEN, "file://%s/%s", cwd, argv[i]);
						g_free (cwd);
					}
				} else {
					g_snprintf (nuri, XMMS_MAX_URI_LEN, "%s", argv[i]);
				}
				
				msg = dbus_message_new ("org.xmms.playlist.add", NULL);
				dbus_message_append_iter_init (msg, &itr);
				dbus_message_iter_append_string (&itr, nuri);
				dbus_connection_send (conn, msg, &cserial);
				dbus_connection_flush (conn);
				dbus_message_unref (msg);
			}
			dbus_connection_disconnect (conn);
			dbus_connection_unref (conn);
			exit(0);
		} else {
			printf ("Unknown command '%s'\n", argv[1]);
			return 1;
		}
	}
	g_main_loop_run (mainloop);
	printf ("...BOOM!\n");
	return 0;

}
