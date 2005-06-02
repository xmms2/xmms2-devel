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

#define MY_PATH "/mnt/forsaken/music/Tagged/Dismantled"


static void
quit (void *data)
{
	GMainLoop *ml = data;
	DBG ("xmms2d disconnected us");
	g_main_loop_quit (ml);
}

static void
add_file_or_dir (xmonitor_t *mon, gchar *filename)
{
	if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		monitor_add_dir (mon, filename);
		DBG ("New directory, starting to watch!");
	} else if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		gchar *tmp = g_strdup_printf ("file://%s", filename);
		xmmsc_medialib_add_entry (mon->conn, tmp);
		DBG ("Adding %s to medialib!", filename);
		g_free (tmp);
	}
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
				add_file_or_dir (mon, event.filename);
				break;
			case MON_DIR_MOVED:
				DBG ("%s moved.. ", event.filename);
				break;
			default:
				break;
		}
	}

	return TRUE;
}

static void
handle_addpath (xmmsc_result_t *res, void *data)
{
	GDir *dir;
	const gchar *tmp;
	gchar *path;

	xmonitor_t *mon = data;

	dir = g_dir_open (MY_PATH, 0, NULL);
	if (!monitor_add_dir (mon, MY_PATH)) {
		ERR ("Couldn't watch directory!");
		return;
	}

	while ((tmp = g_dir_read_name (dir))) {
		path = g_strdup_printf ("%s/%s", MY_PATH, tmp);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			if (!monitor_add_dir (mon, path)) {
				ERR ("Couldn't watch directory: %s!", path);
				g_free (path);
				continue;
			}
			DBG ("Now watching dir %s", path);
		}
		g_free (path);
	}

	g_dir_close (dir);
}

int 
main (int argc, char **argv)
{
	GIOChannel *gio;
	GMainLoop *ml;
	gchar *path;
	xmmsc_connection_t *conn;
	xmmsc_result_t *res;
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
	mon->conn = conn;

	if (fd == -1) {
		ERR ("Couldn't initalize monitor");
		return EXIT_FAILURE;
	}


	gio = g_io_channel_unix_new (fd);
	g_io_add_watch (gio, G_IO_IN, s_callback, mon);

	/* Make sure that nothing changed while we where away! */
	res = xmmsc_medialib_path_import (conn, MY_PATH);
	xmmsc_result_notifier_set (res, handle_addpath, mon);

	g_main_loop_run (ml);

	return EXIT_SUCCESS;
}

