#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>

#include "mlibupdater.h"


static void
quit (void *data)
{
	GMainLoop *ml = data;
	DBG ("xmms2d disconnected us");
	g_main_loop_quit (ml);
}

gboolean
s_callback (GIOChannel *s, GIOCondition cond, gpointer data)
{
	xmonitor_t *mon = data;
	xevent_t event;

	while (monitor_process (mon, &event)) {
		switch (event.code) {
			case MON_DIR_CHANGED:
				DBG ("%s changed.. ", event.filename);
				break;
			case MON_DIR_DELETED:
				DBG ("%s deleted.. ", event.filename);
				break;
			case MON_DIR_CREATED:
				DBG ("%s created.. ", event.filename);
				break;
			case MON_DIR_MOVED:
				DBG ("%s moved.. ", event.filename);
				break;
			default:
				DBG ("Unknown event...");
		}
	}

	return TRUE;
}

int 
main (int argc, char **argv)
{
	GIOChannel *gio;
	GMainLoop *ml;
	gchar *path;
	xmmsc_connection_t *conn;
	xmonitor_t *mon;
	gint fd;

	conn = xmmsc_init ("XMMS MLib Updater " VERSION);
	path = getenv ("XMMS_PATH");
	if (!xmmsc_connect (conn, path)) {
		ERR ("Could not connect to xmms2d %s", xmmsc_get_last_error (conn));
		return EXIT_FAILURE;
	}

	ml = g_main_loop_new (NULL, FALSE);
	xmmsc_setup_with_gmain (conn);
	xmmsc_disconnect_callback_set (conn, quit, ml);

	mon = g_new0 (xmonitor_t, 1);
	fd = monitor_init (mon);

	if (fd == -1) {
		ERR ("Couldn't initalize monitor");
		return EXIT_FAILURE;
	}

	if (!monitor_add_dir (mon, "/tmp/test", FALSE)) {
		ERR ("Couldn't watch directory!");
		return EXIT_FAILURE;
	}

	gio = g_io_channel_unix_new (fd);
	g_io_add_watch (gio, G_IO_IN, s_callback, mon);

	DBG ("All done, waiting for events");
	g_main_loop_run (ml);

	return EXIT_SUCCESS;
}

