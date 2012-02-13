/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient-glib.h>

#include <xmms_configuration.h>

typedef struct updater_St {
	xmmsc_connection_t *conn;
	GHashTable *watchers;
	GFile *root;
} updater_t;

typedef struct updater_quit_St {
	GMainLoop *ml;
	updater_t *updater;
	void *source;
} updater_quit_t;

static gboolean updater_add_watcher (updater_t *updater, GFile *root);
static void on_directory_event (GFileMonitor *monitor, GFile *dir,
                                GFile *other, GFileMonitorEvent event,
                                gpointer udata);

/* TODO: Remove once we depend on GLib >= 2.18 */
#ifndef HAVE_G_FILE_QUERY_FILE_TYPE
static GFileType
g_file_query_file_type (GFile *file, GFileQueryInfoFlags flags,
                        GCancellable *cancellable);

GFileType
g_file_query_file_type (GFile *file,
                        GFileQueryInfoFlags   flags,
                        GCancellable *cancellable)
{
  GFileInfo *info;
  GFileType file_type;

  g_return_val_if_fail (G_IS_FILE(file), G_FILE_TYPE_UNKNOWN);
  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, flags,
			    cancellable, NULL);
  if (info != NULL)
    {
      file_type = g_file_info_get_file_type (info);
      g_object_unref (info);
    }
  else
    file_type = G_FILE_TYPE_UNKNOWN;

  return file_type;
}
#endif

static void
unregister_monitor (gpointer ptr)
{
	GFileMonitor *monitor = (GFileMonitor *) ptr;

	g_file_monitor_cancel (monitor);
	g_object_unref (monitor);
}

static updater_t *
updater_new (void)
{
	updater_t *updater = g_new0 (updater_t, 1);

	updater->conn = xmmsc_init ("XMMS2-Medialib-Updater");
	updater->watchers = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                           g_free, unregister_monitor);

	return updater;
}

static void
updater_destroy (updater_t *updater)
{
	g_return_if_fail (updater);
	g_return_if_fail (updater->watchers);
	g_return_if_fail (updater->conn);

	g_hash_table_destroy (updater->watchers);
	xmmsc_unref (updater->conn);
	g_free (updater);
}

static void
updater_quit (void *data)
{
	updater_quit_t *quit = (updater_quit_t *) data;

	g_debug ("Shutting down!");

	xmmsc_mainloop_gmain_shutdown (quit->updater->conn,
	                               quit->source);

	updater_destroy (quit->updater);
	g_main_loop_quit (quit->ml);
	g_free (quit);
}

static updater_quit_t *
updater_connect (updater_t *updater, GMainLoop *ml)
{
	updater_quit_t *quit;
	gchar *path;

	g_return_val_if_fail (updater, NULL);
	g_return_val_if_fail (updater->conn, NULL);
	g_return_val_if_fail (ml, NULL);

	path = getenv ("XMMS_PATH");

	if (!xmmsc_connect (updater->conn, path)) {
		g_warning ("Unable to connect to XMMS2");
		return NULL;
	}

	quit = g_new0 (updater_quit_t, 1);
	quit->updater = updater;
	quit->ml = ml;
	quit->source = xmmsc_mainloop_gmain_init (updater->conn);

	xmmsc_disconnect_callback_set (updater->conn, updater_quit, quit);

	return quit;
}

static void
updater_clear_watchers (updater_t *updater)
{
	g_return_if_fail (updater);
	g_return_if_fail (updater->watchers);

	g_hash_table_remove_all (updater->watchers);
}


static void
updater_find_sub_directories (updater_t *updater, GFile *file)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GError *err = NULL;

	g_return_if_fail (updater);
	g_return_if_fail (file);

	enumerator = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                        G_FILE_QUERY_INFO_NONE, NULL, &err);

	g_clear_error (&err);

	while ((info = g_file_enumerator_next_file (enumerator, NULL, &err)) != NULL) {
		const gchar *name;
		GFile *child;

		name = g_file_info_get_name (info);
		child = g_file_get_child (file, name);
		g_object_unref (info);

		updater_add_watcher (updater, child);
		g_object_unref (child);
	}

	g_object_unref (enumerator);
}


static gboolean
updater_is_dir (GFile *file)
{
	GFileType type;

	g_return_val_if_fail (file, FALSE);

	type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);

	if (type != G_FILE_TYPE_DIRECTORY) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
updater_add_watcher (updater_t *updater, GFile *file)
{
	GFileMonitor *monitor;
	GError *err = NULL;
	gchar *path;

	g_return_val_if_fail (updater, FALSE);
	g_return_val_if_fail (file, FALSE);

	if (!updater_is_dir (file)) {
		return FALSE;
	}

	path = g_file_get_path (file);

	monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &err);
	if (err) {
		g_printerr ("Unable to monitor '%s', %s\n", path, err->message);
		g_error_free (err);
		g_free (monitor);
		g_free (path);
		return FALSE;
	}

	g_signal_connect (monitor, "changed", (gpointer) on_directory_event, updater);

	/* path ownership transfered to the hash */
	g_hash_table_insert (updater->watchers, path, monitor);

	updater_find_sub_directories (updater, file);

	return TRUE;
}

static gboolean
updater_add_watcher_and_import (updater_t *updater, GFile *file)
{
	xmmsc_result_t *res;
	gchar *url, *path;

	g_return_val_if_fail (updater, FALSE);
	g_return_val_if_fail (file, FALSE);

	if (!updater_add_watcher (updater, file)) {
		return FALSE;
	}

	path = g_file_get_path (file);
	url = g_strdup_printf ("file://%s", path);
	g_free (path);

	res = xmmsc_medialib_import_path (updater->conn, url);
	xmmsc_result_unref (res);

	g_free (url);

	return TRUE;
}

static gboolean
updater_remove_watcher (updater_t *updater, GFile *file)
{
	gboolean ret = TRUE;
	gchar *path;

	g_return_val_if_fail (updater, FALSE);
	g_return_val_if_fail (file, FALSE);

	path = g_file_get_path (file);

	if (!g_hash_table_remove (updater->watchers, path)) {
		ret = FALSE;
	}

	g_free (path);

	return ret;
}

/**
 * TODO: Maybe this should support colon separated dirs in the future
 */
static gboolean
updater_switch_directory (updater_t *updater, const gchar *path)
{
	GFile *file;

	g_return_val_if_fail (updater, FALSE);
	g_return_val_if_fail (updater->conn, FALSE);
	g_return_val_if_fail (path, FALSE);

	file = g_file_new_for_path (path);

	g_debug ("switching directory to: %s", path);

	if (!updater_is_dir (file)) {
		g_object_unref (file);
		return FALSE;
	}

	updater_clear_watchers (updater);
	updater_add_watcher_and_import (updater, file);
	g_object_unref (file);

	return TRUE;
}

static int
updater_config_changed (xmmsv_t *value, void *udata)
{
	updater_t *updater = (updater_t *) udata;
	const gchar *path;

	g_return_val_if_fail (updater, FALSE);

	if (xmmsv_dict_entry_get_string (value, "clients.mlibupdater.watch_dirs", &path)) {
		if (*path) {
			updater_switch_directory (updater, path);
		}
	}

	return TRUE;
}

static int
updater_config_get (xmmsv_t *value, void *udata)
{
	updater_t *updater;
	const gchar *path;

	updater = (updater_t *) udata;

	g_return_val_if_fail (updater, FALSE);

	if (!xmmsv_get_string (value, &path)) {
		g_error ("Failed to retrieve config value\n");
		return FALSE;
	}

	if (*path) {
		updater_switch_directory (updater, path);
	} else {
		g_message ("Please register a directory with the command:");
		g_message ("\nxmms2 server config "
		           "clients.mlibupdater.watch_dirs /path/to/directory");
	}

	return FALSE;
}

static int
updater_config_register (xmmsv_t *value, void *udata)
{
	xmmsc_result_t *res;
	const gchar *conf;
	updater_t *updater;

	updater = (updater_t *) udata;

	g_return_val_if_fail (updater, FALSE);

	if (!xmmsv_get_string (value, &conf)) {
		g_error ("Failed to register config value\n");
		return FALSE;
	}

	res = xmmsc_config_get_value (updater->conn, conf);
	xmmsc_result_notifier_set (res, updater_config_get, updater);
	xmmsc_result_unref (res);

	return FALSE;
}

static void
updater_subscribe_config (updater_t *updater)
{
	xmmsc_result_t *res;
	const gchar *default_directory;

	g_return_if_fail (updater);
	g_return_if_fail (updater->conn);

	default_directory = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);

	if (!default_directory) {
		default_directory = "";
	}

	res = xmmsc_config_register_value (updater->conn,
	                                   "mlibupdater.watch_dirs",
	                                   default_directory);

	xmmsc_result_notifier_set (res, updater_config_register, updater);
	xmmsc_result_unref (res);

	res = xmmsc_broadcast_config_value_changed (updater->conn);
	xmmsc_result_notifier_set (res, updater_config_changed, updater);
	xmmsc_result_unref (res);
}

static void
updater_add_file (updater_t *updater, GFile *file)
{
	xmmsc_result_t *res;
	gchar *path, *url;

	g_return_if_fail (updater);
	g_return_if_fail (updater->conn);
	g_return_if_fail (file);

	path = g_file_get_path (file);
	url = g_strdup_printf ("file://%s", path);
	g_free (path);

	res = xmmsc_medialib_add_entry (updater->conn, url);
	xmmsc_result_unref (res);

	g_free (url);
}

static int
updater_remove_file_by_id (xmmsv_t *value, void *udata)
{
	xmmsc_result_t *res;
	updater_t *updater;
	int mid;

	updater = (updater_t *) udata;

	g_return_val_if_fail (updater, FALSE);

	if (!xmmsv_get_int (value, &mid)) {
		g_error ("couldn't find this one!");
		return FALSE;
	}

	if (!mid) {
		g_debug ("entry not in medialib");
		return FALSE;
	}

	res = xmmsc_medialib_remove_entry (updater->conn, mid);
	xmmsc_result_unref (res);

	return FALSE;
}

static void
updater_remove_file (updater_t *updater, GFile *file)
{
	xmmsc_result_t *res;
	gchar *path, *url;

	g_return_if_fail (updater);
	g_return_if_fail (updater->conn);
	g_return_if_fail (file);

	path = g_file_get_path (file);
	url = g_strdup_printf ("file://%s", path);
	g_free (path);

	g_debug ("removing file '%s'", url);

	res = xmmsc_medialib_get_id (updater->conn, url);
	xmmsc_result_notifier_set (res, updater_remove_file_by_id, updater);
	xmmsc_result_unref (res);

	g_free (url);
}

static int
updater_remove_directory_by_id (xmmsv_t *value, void *udata)
{
	xmmsv_list_iter_t *it;

	xmmsv_get_list_iter (value, &it);
	while (xmmsv_list_iter_valid (it)) {
		xmmsv_t *item;

		if (xmmsv_list_iter_entry (it, &item)) {
			updater_remove_file_by_id (item, udata);
		}

		xmmsv_list_iter_next (it);
	}
	return TRUE;
}

static void
updater_remove_directory (updater_t *updater, GFile *file)
{
	xmmsc_result_t *res;
	xmmsv_coll_t *univ, *coll;
	gchar *path, *pattern, *encoded;

	path = g_file_get_path (file);
	encoded = xmmsc_medialib_encode_url (path);
	g_free (path);

	pattern = g_strdup_printf ("file://%s/*", encoded);
	g_free (encoded);

	univ = xmmsv_coll_universe ();
	coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_MATCH);

	xmmsv_coll_add_operand (coll, univ);
	xmmsv_coll_attribute_set (coll, "field", "url");
	xmmsv_coll_attribute_set (coll, "value", pattern);
	xmmsv_coll_attribute_set (coll, "case-sensitive", "true");

	g_debug ("remove '%s' from mlib", pattern);

	res = xmmsc_coll_query_ids (updater->conn, coll, NULL, 0, 0);
	xmmsc_result_notifier_set (res, updater_remove_directory_by_id, updater);
	xmmsc_result_unref (res);

	xmmsv_coll_unref (coll);
	xmmsv_coll_unref (univ);

	g_free (pattern);
}

static int
updater_rehash_file_by_id (xmmsv_t *value, void *udata)
{
	xmmsc_result_t *res;
	updater_t *updater;
	int mid;

	updater = (updater_t *) udata;

	g_return_val_if_fail (updater, FALSE);

	if (!xmmsv_get_int (value, &mid)) {
		return FALSE;
	}

	if (!mid) {
		g_warning ("Couldn't find requested medialib entry.");
		return FALSE;
	}

	res = xmmsc_medialib_rehash (updater->conn, mid);
	xmmsc_result_unref (res);

	return FALSE;
}

static void
updater_rehash_file (updater_t *updater, GFile *file)
{
	xmmsc_result_t *res;
	gchar *path, *url;

	g_return_if_fail (updater);
	g_return_if_fail (updater->conn);
	g_return_if_fail (file);

	path = g_file_get_path (file);
	url = g_strdup_printf ("file://%s", path);
	g_free (path);

	g_debug ("resolving entry '%s'", url);

	res = xmmsc_medialib_get_id (updater->conn, url);
	xmmsc_result_notifier_set (res, updater_rehash_file_by_id, updater);
	xmmsc_result_unref (res);

	g_free (url);
}

static void
on_entity_created (updater_t *updater, GFile *entity)
{
	GFileType type;

	g_return_if_fail (updater);
	g_return_if_fail (entity);

	type = g_file_query_file_type (entity, G_FILE_QUERY_INFO_NONE, NULL);
	switch (type) {
	case G_FILE_TYPE_DIRECTORY:
		g_debug ("directory created");
		updater_add_watcher_and_import (updater, entity);
		break;
	case G_FILE_TYPE_REGULAR:
		g_debug ("file created");
		updater_add_file (updater, entity);
		break;
	default:
		g_debug ("something else created: %d", (int) type);
		break;
	}
}

static void
on_entity_changed (updater_t *updater, GFile *entity)
{
	GFileType type;

	g_return_if_fail (updater);
	g_return_if_fail (entity);

	type = g_file_query_file_type (entity, G_FILE_QUERY_INFO_NONE, NULL);
	switch (type) {
	case G_FILE_TYPE_REGULAR:
		updater_rehash_file (updater, entity);
		break;
	default:
		g_debug ("something else changed: %d", (int) type);
		break;
	}
}

static void
on_entity_deleted (updater_t *updater, GFile *entity)
{
	g_debug ("entity deleted");

	g_return_if_fail (updater);
	g_return_if_fail (entity);

	/* There is no way of knowing if the deleted event
	 * came from a file or a directory so lets give it
	 * a go with both actions and see what happens.
	 */
	if (updater_remove_watcher (updater, entity)) {
		updater_remove_directory (updater, entity);
	} else {
		updater_remove_file (updater, entity);
	}
}

static void
on_directory_event (GFileMonitor *monitor, GFile *entity, GFile *other,
                    GFileMonitorEvent event, gpointer udata)
{
	updater_t *updater = (updater_t *) udata;

	g_return_if_fail (updater);

	switch (event) {
	case G_FILE_MONITOR_EVENT_CREATED:
		on_entity_created (updater, entity);
		break;
	case G_FILE_MONITOR_EVENT_CHANGED:
		on_entity_changed (updater, entity);
		break;
	case G_FILE_MONITOR_EVENT_DELETED:
		on_entity_deleted (updater, entity);
		break;
	default:
		break;
	}
}

int
main (int argc, char **argv)
{
	updater_t *updater;
	GMainLoop *ml;

	g_type_init ();
	g_thread_init (NULL);

	ml = g_main_loop_new (NULL, FALSE);

	updater = updater_new ();

	if (!updater_connect (updater, ml)) {
		updater_destroy (updater);
		g_main_loop_unref (ml);
		return EXIT_FAILURE;
	}

	updater_subscribe_config (updater);

	g_main_loop_run (ml);
	g_main_loop_unref (ml);

	return EXIT_SUCCESS;
}
