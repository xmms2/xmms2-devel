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
	ERR ("xmms2d disconnected us");
	g_main_loop_quit (ml);
}

static void
handle_file_add (xmonitor_t *mon, gchar *filename)
{
	if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
		monitor_add_dir (mon, filename);
		mon->dir_list = g_list_append (mon->dir_list, filename);
		DBG ("New directory: %s", filename);
	} else if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		gchar tmp[MON_FILENAME_MAX];
		g_snprintf (tmp, MON_FILENAME_MAX, "file://%s", filename);
		xmmsc_result_unref (xmmsc_medialib_add_entry (mon->conn, tmp));
		DBG ("Adding %s to medialib!", tmp);
	}
}

static int
handle_remove_from_mlib (xmmsv_t *v, void *userdata)
{
	xmonitor_t *mon = userdata;
	xmmsc_result_t *res2;
	xmmsv_list_iter_t *it;

	for (xmmsv_get_list_iter (v, &it);
	     xmmsv_list_iter_valid (it);
	     xmmsv_list_iter_next (it)) {
		gint32 id;
		xmmsv_t *elem;
		if (!xmmsv_list_iter_entry (it, &elem) || !xmmsv_get_int (elem, &id)) {
			ERR ("Failed to get entry id from hash!");
			continue;
		}

		if (id == 0) {
			DBG ("Entry not in db!");
			continue;
		}

		DBG ("Removing %d", id);

		res2 = xmmsc_medialib_remove_entry (mon->conn, id);
		xmmsc_result_unref (res2);
	}

	return FALSE;
}

static void
handle_file_del (xmonitor_t *mon, gchar *filename)
{
	xmmsc_result_t *res;
	xmmsv_coll_t *univ, *coll;
	gchar tmp[MON_FILENAME_MAX];

	g_snprintf (tmp, MON_FILENAME_MAX, "file://%s%%", filename);

	univ = xmmsv_coll_universe ();
	coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_MATCH);
	xmmsv_coll_add_operand (coll, univ);
	xmmsv_coll_attribute_set (coll, "field", "url");
	xmmsv_coll_attribute_set (coll, "value", tmp);

	res = xmmsc_coll_query_ids (mon->conn, coll, NULL, 0, 0);

	DBG ("remove '%s' from mlib", tmp);
	xmmsc_result_notifier_set (res, handle_remove_from_mlib, mon);
	xmmsc_result_unref (res);
	xmmsv_coll_unref (coll);
	xmmsv_coll_unref (univ);
}

static int
handle_mlib_update (xmmsv_t *v, void *userdata)
{
	xmonitor_t *mon = userdata;
	xmmsc_result_t *res2;
	gint32 id;

	if (!xmmsv_get_int (v, &id)) {
		ERR ("Failed to get id for entry!");
		return FALSE;
	}

	if (id == 0) {
		DBG ("Entry not in db!");
		return FALSE;
	}

	res2 = xmmsc_medialib_rehash (mon->conn, id);
	xmmsc_result_unref (res2);

	return FALSE;
}

static void
handle_file_changed (xmonitor_t *mon, gchar *filename)
{
	xmmsc_result_t *res;
	gchar tmp[MON_FILENAME_MAX];

	g_snprintf (tmp, MON_FILENAME_MAX, "file://%s", filename);

	res = xmmsc_medialib_get_id (mon->conn, tmp);
	xmmsc_result_notifier_set (res, handle_mlib_update, mon);
	xmmsc_result_unref (res);
	DBG ("update file in medialib");
}

gboolean
s_callback (GIOChannel *s, GIOCondition cond, gpointer data)
{
	xmonitor_t *mon = data;
	xevent_t event;

	while (monitor_process (mon, &event)) {
		switch (event.code) {
			case MON_DIR_CHANGED:
				DBG ("got changed signal for %s", event.filename);
				handle_file_changed (mon, event.filename);
				break;
			case MON_DIR_DELETED:
				handle_file_del (mon, event.filename);
				break;
			case MON_DIR_CREATED:
				handle_file_add (mon, event.filename);
				break;
			default:
				break;
		}
	}

	return TRUE;
}

static void
process_dir (xmonitor_t *mon, gchar *dirpath)
{
	GDir *dir;
	const gchar *tmp;
	gchar *path;

	dir = g_dir_open (dirpath, 0, NULL);
	if (!dir) {
		ERR ("Error when opening %s", dirpath);
		return;
	}

	while ((tmp = g_dir_read_name (dir))) {
		path = g_strdup_printf ("%s/%s", dirpath, tmp);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			if (!monitor_add_dir (mon, path)) {
				ERR ("Couldn't watch directory: %s!", path);
				g_free (path);
				continue;
			}
			DBG ("Now watching dir %s", path);
			mon->dir_list = g_list_append (mon->dir_list, path);
			process_dir (mon, path);
		} else {
			g_free (path);
		}
	}

	g_main_context_iteration (NULL, FALSE);
	
	g_dir_close (dir);
}

static int
handle_addpath (xmmsv_t *v, void *data)
{
	xmonitor_t *mon = data;

	if (!monitor_add_dir (mon, mon->watch_dir)) {
		ERR ("Couldn't watch directory!");
		return FALSE;
	}
	mon->dir_list = g_list_append (mon->dir_list, mon->watch_dir);

	process_dir (mon, mon->watch_dir);

	DBG ("Watching %d dirs", g_list_length (mon->dir_list));

	return FALSE;
}

static void
do_watch_dir (xmonitor_t *mon, const gchar *dirs)
{
	xmmsc_result_t *res;
	GList *n;

	DBG ("We are going to watch '%s'", dirs);

	for (n = mon->dir_list; n; n = g_list_next (n)) {
		monitor_del_dir (mon, n->data);
		g_free (n->data);
	}

	if (mon->dir_list) {
		g_list_free (mon->dir_list);
		mon->dir_list = NULL;
	}

	if (strlen (dirs) < 1) {
		mon->watch_dir = NULL;
		return;
	} else {
		mon->watch_dir = g_strdup (dirs);
	}

	/* Make sure that nothing changed while we where away! */
	res = xmmsc_medialib_path_import (mon->conn, mon->watch_dir);
	xmmsc_result_notifier_set (res, handle_addpath, mon);
	xmmsc_result_unref (res);
}

static int
handle_config_changed (xmmsv_t *v, void *data)
{
	xmonitor_t *mon = data;
	const gchar *val = NULL;
	int s;

	s = xmmsv_dict_entry_get_string (v,
	                                 "clients.mlibupdater.watch_dirs",
	                                 &val);
	if (s) {
		do_watch_dir (mon, val);
	}

	return TRUE; /* keep broadcast alive */
}

static int
handle_watch_dirs (xmmsv_t *v, void *data)
{
	xmonitor_t *mon = data;
	const gchar *dirs;

	if (!xmmsv_get_string (v, &dirs)) {
		ERR ("Couldn't get configvalue from server!");
		return FALSE;
	}

	do_watch_dir (mon, dirs);

	return FALSE;
}

static int
handle_configval (xmmsv_t *v, void *data)
{
	xmmsc_result_t *res2;
	xmonitor_t *mon = data;
	const gchar *val;

	if (!xmmsv_get_string (v, &val)) {
		ERR ("Couldn't register value in server!");
		return FALSE;
	}

	res2 = xmmsc_configval_get (mon->conn, val);
	xmmsc_result_notifier_set (res2, handle_watch_dirs, mon);
	xmmsc_result_unref (res2);

	return FALSE;
}

static void
message_handler (const gchar *log_domain, GLogLevelFlags log_level,
		 const gchar *message, gpointer user_data)
{

}

int 
main (int argc, char **argv)
{
	GIOChannel *gio;
	GMainLoop *ml;
	gchar *path;
	gchar *tmp;
	xmmsc_connection_t *conn;
	xmmsc_result_t *res;
	xmonitor_t *mon;
	gint fd;

	conn = xmmsc_init ("xmms-medialib-updater");
	path = getenv ("XMMS_PATH");
	if (!xmmsc_connect (conn, path)) {
		ERR ("Could not connect to xmms2d %s", xmmsc_get_last_error (conn));
		return EXIT_FAILURE;
	}

	ml = g_main_loop_new (NULL, FALSE);
	xmmsc_mainloop_gmain_init (conn);
	xmmsc_disconnect_callback_set (conn, quit, ml);

	mon = g_new0 (xmonitor_t, 1);
	fd = monitor_init (mon);
	mon->conn = conn;

	if (fd == -1) {
		ERR ("Couldn't initalize monitor");
		return EXIT_FAILURE;
	}


	tmp = getenv("XMMS_DEBUG");
	if (!tmp) {
		g_log_set_handler (NULL, G_LOG_LEVEL_MESSAGE | G_LOG_FLAG_RECURSION, 
				   message_handler, NULL);
	}

	gio = g_io_channel_unix_new (fd);
	g_io_add_watch (gio, G_IO_IN, s_callback, mon);

	res = xmmsc_configval_register (conn, "mlibupdater.watch_dirs", "");
	xmmsc_result_notifier_set (res, handle_configval, mon);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_configval_changed (conn);
	xmmsc_result_notifier_set (res, handle_config_changed, mon);
	xmmsc_result_unref (res);

	g_main_loop_run (ml);

	return EXIT_SUCCESS;
}

