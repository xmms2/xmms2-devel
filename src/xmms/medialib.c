/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_plsplugins.h"
#include "xmms/xmms_defs.h"
#include "xmms/xmms_error.h"
#include "xmms/xmms_config.h"
#include "xmms/xmms_object.h"
#include "xmms/xmms_ipc.h"
#include "xmms/xmms_log.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <time.h>

#include <sqlite3.h>

/**
 * @file 
 * Medialib is a metainfo cache that is searchable.
 */


static void xmms_medialib_entry_remove_method (xmms_medialib_t *medialib, guint32 entry, xmms_error_t *error);
static int get_playlist_entries_cb (void *pArg, int argc, char **argv, char **cName);
static xmms_medialib_entry_t xmms_medialib_entry_new_unlocked (const char *url);
static int xmms_medialib_int_cb (void *pArg, int argc, char **argv, char **columnName);

static GHashTable *xmms_medialib_info (xmms_medialib_t *playlist, guint32 id, xmms_error_t *err);
static void xmms_medialib_select_and_add (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error);
void xmms_medialib_add_entry (xmms_medialib_t *, gchar *, xmms_error_t *);
static GList *xmms_medialib_select_method (xmms_medialib_t *, gchar *, xmms_error_t *);
GList *xmms_medialib_select (gchar *query, xmms_error_t *error);
static void xmms_medialib_playlist_save_current (xmms_medialib_t *, gchar *, xmms_error_t *);
static void xmms_medialib_playlist_load (xmms_medialib_t *, gchar *, xmms_error_t *);
static void xmms_medialib_playlist_import (xmms_medialib_t *medialib, gchar *playlistname, 
					   gchar *url, xmms_error_t *error);
static gchar *xmms_medialib_playlist_export (xmms_medialib_t *medialib, gchar *playlistname, 
					     gchar *mime, xmms_error_t *error);
static void xmms_medialib_playlist_remove (xmms_medialib_t *medialib, gchar *playlistname, xmms_error_t *);
static void xmms_medialib_path_import (xmms_medialib_t *medialib, gchar *path, xmms_error_t *error);
static void xmms_medialib_rehash (xmms_medialib_t *medialib, guint32 id, xmms_error_t *error);
static guint32 xmms_medialib_entry_get_id (xmms_medialib_t *medialib, gchar *url, xmms_error_t *error);

XMMS_CMD_DEFINE (info, xmms_medialib_info, xmms_medialib_t *, DICT, UINT32, NONE);
XMMS_CMD_DEFINE (select, xmms_medialib_select_method, xmms_medialib_t *, DICTLIST, STRING, NONE);
XMMS_CMD_DEFINE (mlib_add, xmms_medialib_add_entry, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (mlib_remove, xmms_medialib_entry_remove_method, xmms_medialib_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (playlist_save_current, xmms_medialib_playlist_save_current, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (playlist_load, xmms_medialib_playlist_load, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (addtopls, xmms_medialib_select_and_add, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (playlist_import, xmms_medialib_playlist_import, xmms_medialib_t *, NONE, STRING, STRING);
XMMS_CMD_DEFINE (playlist_export, xmms_medialib_playlist_export, xmms_medialib_t *, STRING, STRING, STRING);
XMMS_CMD_DEFINE (playlist_remove, xmms_medialib_playlist_remove, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (path_import, xmms_medialib_path_import, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (rehash, xmms_medialib_rehash, xmms_medialib_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (get_id, xmms_medialib_entry_get_id, xmms_medialib_t *, UINT32, STRING, NONE);

/**
 *
 * @defgroup Medialib Medialib
 * @ingroup XMMSServer
 * @brief Medialib caches metadata
 *
 * Controls metadata storage.
 * 
 * @{
 */

/**
 * Medialib structure
 */
struct xmms_medialib_St {
	xmms_object_t object;
	/** The current playlist */
	xmms_playlist_t *playlist;
	/** SQLite pointer */
	sqlite3 *sql;
	/** SQLite mutex */
	GMutex *mutex;
	/** Nextid in the mediatable */
	guint32 nextid;

	/** Statement to run when user is in "random mode" */
	const gchar *random_sql;
};


/** 
  * Ok, so the functions are written with reentrency in mind, but
  * we choose to have a global medialib object here. It will be
  * much easier, and I don't see the real use of multiple medialibs
  * right now. This could be changed by removing this global one
  * and altering the function callers...
  */

static xmms_medialib_t *medialib;


static void 
xmms_medialib_destroy (xmms_object_t *object)
{
	xmms_medialib_t *mlib = (xmms_medialib_t *)object;

	g_mutex_free (mlib->mutex);
	xmms_sqlite_close (mlib->sql);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_OUTPUT);
}

static void
xmms_medialib_path_changed (xmms_object_t *object, gconstpointer data,
			    gpointer userdata)
{
	gboolean c;
	xmms_medialib_t *mlib = userdata;
	g_mutex_lock (mlib->mutex);
	xmms_sqlite_close (mlib->sql);
	medialib->sql = xmms_sqlite_open (&medialib->nextid, &c);
	g_mutex_unlock (mlib->mutex);
}

static void
xmms_medialib_random_sql_changed (xmms_object_t *object, gconstpointer data,
				  gpointer userdata)
{
	xmms_medialib_t *mlib = userdata;

	g_mutex_lock (mlib->mutex);
	medialib->random_sql = (gchar*)data;
	g_mutex_unlock (mlib->mutex);
}

/**
 * Initialize the medialib and open the database file.
 * 
 * @param playlist the current playlist pointer
 * @returns TRUE if successful and FALSE if there was a problem
 */
 
gboolean
xmms_medialib_init (xmms_playlist_t *playlist)
{
	gchar path[XMMS_PATH_MAX+1];
	gboolean create;
	xmms_config_value_t *cv;

	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);
	medialib->mutex = g_mutex_new ();
	medialib->playlist = playlist;

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIALIB, XMMS_OBJECT (medialib));
	xmms_ipc_broadcast_register (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);

	xmms_object_cmd_add (XMMS_OBJECT (medialib), 
			     XMMS_IPC_CMD_INFO, 
			     XMMS_CMD_FUNC (info));
	xmms_object_cmd_add (XMMS_OBJECT (medialib), 
			     XMMS_IPC_CMD_SELECT, 
			     XMMS_CMD_FUNC (select));
	xmms_object_cmd_add (XMMS_OBJECT (medialib), 
			     XMMS_IPC_CMD_ADD, 
			     XMMS_CMD_FUNC (mlib_add));
	xmms_object_cmd_add (XMMS_OBJECT (medialib), 
			     XMMS_IPC_CMD_REMOVE, 
			     XMMS_CMD_FUNC (mlib_remove));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PLAYLIST_SAVE_CURRENT,
	                     XMMS_CMD_FUNC (playlist_save_current));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PLAYLIST_LOAD,
	                     XMMS_CMD_FUNC (playlist_load));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_ADD_TO_PLAYLIST,
	                     XMMS_CMD_FUNC (addtopls));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PLAYLIST_IMPORT,
	                     XMMS_CMD_FUNC (playlist_import));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PLAYLIST_EXPORT,
	                     XMMS_CMD_FUNC (playlist_export));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
			     XMMS_IPC_CMD_PLAYLIST_REMOVE,
			     XMMS_CMD_FUNC (playlist_remove));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PATH_IMPORT,
	                     XMMS_CMD_FUNC (path_import));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_REHASH,
	                     XMMS_CMD_FUNC (rehash));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_GET_ID,
	                     XMMS_CMD_FUNC (get_id));

	xmms_config_value_register ("medialib.dologging",
				    "1",
				    NULL, NULL);

	xmms_config_value_register ("medialib.playlist_load_on_import",
				    "0",
				    NULL, NULL);

	cv = xmms_config_value_register ("medialib.random_sql_statement",
					 "select id from Media where key='url' order by random() limit 1",
					 xmms_medialib_random_sql_changed, medialib);
	medialib->random_sql = xmms_config_value_string_get (cv);

	g_snprintf (path, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", g_get_home_dir());

	xmms_config_value_register ("medialib.path",
				    path,
				    xmms_medialib_path_changed, medialib);

	medialib->sql = xmms_sqlite_open (&medialib->nextid, &create);
	
	if (create) {
		xmms_medialib_entry_t entry;
		entry = xmms_medialib_entry_new ("file://" SHAREDDIR "/dismantled-the_swarm_clip.ogg");
		xmms_playlist_add (medialib->playlist, entry, NULL);
	}

	
	return TRUE;
}

/**
 * Return a random entry from the medialib.
 * The SQL statement used for retrival can be configured.
 */

guint32
xmms_medialib_get_random_entry (void)
{
	guint32 ret = 0;

	g_mutex_lock (medialib->mutex);
	xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &ret, 
			   medialib->random_sql);
	g_mutex_unlock (medialib->mutex);

	return ret;
}


/**
 * Called to start the logging of a entry.
 * When this is called you *have* to call logging_stop in order to
 * ensure database consistency.
 *
 * @param entry Entry to log.
 *
 * @sa xmms_medialib_logging_stop
 */

void
xmms_medialib_logging_start (xmms_medialib_entry_t entry)
{
	char tmp[16];
	time_t starttime;
	gboolean ret;
	xmms_config_value_t *cv;

	g_return_if_fail (entry);
	
	cv = xmms_config_lookup ("medialib.dologging");
	g_return_if_fail (cv);
	
	ret = xmms_config_value_int_get (cv);
	if (!ret)
		return;

	starttime = time (NULL);
	g_mutex_lock (medialib->mutex);
	ret = xmms_sqlite_query (medialib->sql, NULL, NULL, 
				 "INSERT INTO Log (id, starttime) VALUES (%u, %u)", 
				 entry, (guint) starttime);
	g_mutex_unlock (medialib->mutex);

	if (ret) {
		g_snprintf (tmp, 16, "%u", (guint) starttime);
		xmms_medialib_entry_property_set (entry, "laststarted", tmp);
	}
}


/**
 * Stops logging for the entry. Will also write the value to the database
 *
 * @param entry The entry.
 * @param playtime Playtime of the entry, this will be used to calculate how
 * many % of the song that was played.
 */

void
xmms_medialib_logging_stop (xmms_medialib_entry_t entry, guint playtime)
{
	const gchar *tmp, *sek;
	gint value = 0.0;
	gboolean ret;
	xmms_config_value_t *cv;

	cv = xmms_config_lookup ("medialib.dologging");
	g_return_if_fail (cv);

	ret = xmms_config_value_int_get (cv);
	if (!ret)
		return;

	tmp = xmms_medialib_entry_property_get (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION);
	if (tmp) {
		value = (gint) (100.0 * playtime / (gdouble)atoi (tmp));
	}
		
	sek = xmms_medialib_entry_property_get (entry, "laststarted");
	g_return_if_fail (sek);

	g_mutex_lock (medialib->mutex);
	ret = xmms_sqlite_query (medialib->sql, NULL, NULL, 
				 "UPDATE Log SET value=%d WHERE id=%u AND starttime=%s", 
				 value, entry, sek);
	g_mutex_unlock (medialib->mutex);
}


static int
xmms_medialib_int_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	guint *i = pArg;

	if (argv[0])
		*i = atoi (argv[0]);

	return 0;
}



static int
xmms_medialib_string_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	gchar **str = pArg;

	if (argv[0])
		*str = g_strdup (argv[0]);

	return 0;
}


/**
 * Retrieve a property from an entry.
 *
 * @param entry Entry to query.
 * @param property The property to extract. Strings passed should 
 * be defined in medialib.h 
 * 
 * @returns Newly allocated gchar that needs to be freed with g_free
 */

gchar *
xmms_medialib_entry_property_get (xmms_medialib_entry_t entry, const gchar *property)
{
	gchar *ret = NULL;

	g_return_val_if_fail (property, NULL);

	g_mutex_lock (medialib->mutex);
	xmms_sqlite_query (medialib->sql, xmms_medialib_string_cb, &ret,
			   "select value from Media where key=%Q and id=%d", property, entry);
	g_mutex_unlock (medialib->mutex);

	return ret;
}

/**
 * Retrieve a property as a int from a entry
 *
 * @param entry Entry to query.
 * @param property The property to extract. Strings passed should 
 * be defined in medialib.h 
 * 
 * @returns Property as integer, will not require you to free memory
 * if you know the property is a int. On failure 0 will be returned.
 */

guint
xmms_medialib_entry_property_get_int (xmms_medialib_entry_t entry, const gchar *property)
{
	gchar *tmp = NULL;
	guint ret;

	tmp = xmms_medialib_entry_property_get (entry, property);
	if (tmp) {
		ret = atoi (tmp);
		g_free (tmp);
	} else {
		ret = 0;
	}
	return ret;
}

/**
 * Set a entry property to a new value, overwriting the old value.
 *
 * @param entry Entry to alter.
 * @param property The property to extract. Strings passed should 
 * be defined in medialib.h 
 * @param value gchar with the new value, will be copied in to the medialib
 *
 * @returns TRUE on success and FALSE on failure.
 */

gboolean
xmms_medialib_entry_property_set (xmms_medialib_entry_t entry, const gchar *property, const gchar *value)
{
	gboolean ret;
	g_return_val_if_fail (property, FALSE);

	g_mutex_lock (medialib->mutex);
	ret = xmms_sqlite_query (medialib->sql, NULL, NULL,
				 "insert or replace into Media (id, value, key) values (%d, %Q, LOWER(%Q))", entry, value, property);
	g_mutex_unlock (medialib->mutex);

	return ret;

}

/**
 * A nice wrapper for @code xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_PROPERTY_RESOLVED); @endcode
 *
 * @return TRUE if the entry was resolved, else false.
 */

gboolean
xmms_medialib_entry_is_resolved (xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);
}

/**
 * A nice wrapper for @code xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_PROPERTY_ID); @endcode
 *
 * @return TRUE if the entry was resolved, else false.
 */

guint
xmms_medialib_entry_id_get (xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ID);
}

/**
 * Trigger a update signal to the client. This should be called
 * when important information in the entry has been changed and 
 * should be visible to the user.
 *
 * @param entry Entry to signal a update for.
 */

void
xmms_medialib_entry_send_update (xmms_medialib_entry_t entry)
{
	g_mutex_lock (medialib->mutex);
	xmms_object_emit_f (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE, 
			    XMMS_OBJECT_CMD_ARG_UINT32, entry);
	g_mutex_unlock (medialib->mutex);
}

static int
xmms_medialib_addtopls_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	gint i;

	xmms_playlist_t *playlist = pArg;

	for (i = 0; i < argc; i++) {
		if (g_strcasecmp (columnName[i], "id") == 0) {
			if (argv[i])
				xmms_playlist_add (playlist, atoi (argv[i]), NULL);
		}
	}
	
	return 0;
}

static void
xmms_medialib_select_and_add (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error)
{
	g_return_if_fail (medialib);
	g_return_if_fail (query);

	g_mutex_lock (medialib->mutex);

	if (!xmms_sqlite_query (medialib->sql, xmms_medialib_addtopls_cb, medialib->playlist,
				"%s", query)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Query failed!");
		g_mutex_unlock (medialib->mutex);
		return;
	}

	g_mutex_unlock (medialib->mutex);

}

static void
xmms_medialib_entry_remove_method (xmms_medialib_t *medialib, guint32 entry, xmms_error_t *error)
{
	xmms_medialib_entry_remove (entry);
}

/**
 * Remove a medialib entry from the database
 *
 * @param entry Entry to remove
 */

void
xmms_medialib_entry_remove (xmms_medialib_entry_t entry)
{
	g_mutex_lock (medialib->mutex);
	xmms_sqlite_query (medialib->sql, NULL, NULL, "delete from Media where id=%d", entry);
	xmms_sqlite_query (medialib->sql, NULL, NULL, "delete from Log where id=%d", entry);
	/** @todo remove from playlists too? or should we do refcounting on this? */
	g_mutex_unlock (medialib->mutex);
}



static gboolean
process_dir (const gchar *path, xmms_error_t *error)
{
	GDir *dir;
	const gchar *file;

	dir = g_dir_open (path, 0, NULL);
	if (!dir) {
		gchar *err = g_strdup_printf ("Failed to open dir %s", path);
		xmms_error_set (error, XMMS_ERROR_GENERIC, err);
		g_free (err);
		return FALSE;
	}

	while ((file = g_dir_read_name (dir))) {
		gchar realfile[XMMS_PATH_MAX+1];
		g_snprintf (realfile, XMMS_PATH_MAX, "%s/%s", path, file);
		if (g_file_test (realfile, G_FILE_TEST_IS_DIR)) {
			if (!process_dir (realfile, error))
				return FALSE;
		} else if (g_file_test (realfile, G_FILE_TEST_EXISTS)) {
			gchar *f = g_strdup_printf ("file://%s", realfile);
			xmms_medialib_entry_new_unlocked (f);
			g_free (f);
		}
	}
	g_dir_close (dir);
	return TRUE;
}

static void 
xmms_medialib_rehash (xmms_medialib_t *medialib, guint32 id, xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;

	if (id) {
		g_mutex_lock (medialib->mutex);
		xmms_sqlite_query (medialib->sql, NULL, NULL, "update Media set value = '0' where key='%s' and id=%d", 
				   XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, id);
		g_mutex_unlock (medialib->mutex);
	} else {
		g_mutex_lock (medialib->mutex);
		xmms_sqlite_query (medialib->sql, NULL, NULL, "update Media set value = '0' where key='%s'", 
				   XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);
		g_mutex_unlock (medialib->mutex);
	}

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);

	
}

static void 
xmms_medialib_path_import (xmms_medialib_t *medialib, gchar *path, xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;
	gchar *p;

	g_return_if_fail (medialib);
	g_return_if_fail (path);

	p = path+strlen(path);

	while (*(p-1) == '/')
		*p--;

	*p = '\0';

	g_mutex_lock (medialib->mutex);

	xmms_sqlite_query (medialib->sql, NULL, NULL, "BEGIN", NULL);

	if (process_dir (path, error)) {
		xmms_sqlite_query (medialib->sql, NULL, NULL, "COMMIT", NULL);
	} else {
		xmms_sqlite_query (medialib->sql, NULL, NULL, "ROLLBACK", NULL);
	}

	g_mutex_unlock (medialib->mutex);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);
	
}

static xmms_medialib_entry_t
xmms_medialib_entry_new_unlocked (const char *url)
{
	guint id = 0;
	guint ret;

	g_return_val_if_fail (url, 0);

	xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &id, 
			   "select id from Media where key='%s' and value=%Q", 
			   XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url);

	if (id) {
		ret = id;
	} else {
		ret = medialib->nextid++;
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert into Media (id, key, value) values (%d, '%s', %Q)",
				       ret, XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url)) {
			return 0;
		}
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert or replace into Media (id, key, value) values (%d, '%s', '0')",
				       ret, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED)) {
			return 0;
		}
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert or replace into Media (id, key, value) values (%d, '%s', '%d')",
				       ret, XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED, time(NULL))) {
			return 0;
		}

	}

	return ret;

}

static guint32
xmms_medialib_entry_get_id (xmms_medialib_t *medialib, gchar *url, xmms_error_t *error)
{
	guint32 id = 0;

	g_mutex_lock (medialib->mutex);
	xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &id, 
			   "select id from Media where key='%s' and value=%Q", 
			   XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url);
	g_mutex_unlock (medialib->mutex);

	return id;
}

/**
 * Welcome to a function that should be called something else.
 * Returns a entry for a URL, if the URL is already in the medialib
 * the current entry will be returned otherwise a new one will be
 * created and returned.
 *
 * @todo rename to something better?
 *
 * @param url URL to add/retrieve from the medialib
 * 
 * @returns Entry mapped to the URL
 */

xmms_medialib_entry_t
xmms_medialib_entry_new (const char *url)
{
	xmms_medialib_entry_t ret;

	g_mutex_lock (medialib->mutex);
	ret = xmms_medialib_entry_new_unlocked (url);
	g_mutex_unlock (medialib->mutex);

	return ret;
}

static int
xmms_medialib_hashtable_cb (void *pArg, int argc, char **argv, char **columnName) 
{
	GHashTable *hash = pArg;

	g_hash_table_insert (hash, g_strdup (argv[1]), g_strdup (argv[2]));

	return 0;
}

/**
 * Convert a entry and all properties to a hashtable that
 * could be feed to the client or somewhere else in the daemon.
 *
 * @param entry Entry to convert.
 *
 * @returns Newly allocated hashtable with newly allocated strings
 * make sure to free them all.
 */

GHashTable *
xmms_medialib_entry_to_hashtable (xmms_medialib_entry_t entry)
{
	GHashTable *ret;
	
	ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	g_mutex_lock (medialib->mutex);

	xmms_sqlite_query (medialib->sql, xmms_medialib_hashtable_cb, ret, 
			   "select * from Media where id=%d", entry);

	g_hash_table_insert (ret, g_strdup ("id"), g_strdup_printf ("%u", entry));

	g_mutex_unlock (medialib->mutex);

	return ret;
}


static GHashTable *
xmms_medialib_info (xmms_medialib_t *playlist, guint32 id, xmms_error_t *err)
{
	GHashTable *ret = xmms_medialib_entry_to_hashtable (id);
	if (!ret) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Could not retrive info for that entry!");
		return NULL;
	}
	return ret;
}

static int
select_callback (void *pArg, int argc, char **argv, char **cName)
{
	gint i=0;
	GList **l = (GList **) pArg;
	GHashTable *table;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	for (i = 0; i < argc; i++) {
		if (argv[i] && cName[i]) {
			g_hash_table_insert (table, g_strdup (cName[i]), g_strdup (argv[i]));

		}
	}

	*l = g_list_prepend (*l, table);

	return 0;
}

static GList *
xmms_medialib_select_method (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error)
{
	return xmms_medialib_select (query, error);
}

/**
 * Add a entry to the medialib. Calls #xmms_medialib_entry_new and then
 * wakes up the mediainfo_reader in order to resolve the metadata.
 *
 * @param medialib Medialib pointer
 * @param url URL to add
 * @param error In case of error this will be filled.
 */

void
xmms_medialib_add_entry (xmms_medialib_t *medialib, gchar *url, xmms_error_t *error)
{
	xmms_medialib_entry_t entry;
	xmms_mediainfo_reader_t *mr;

	g_return_if_fail (medialib);
	g_return_if_fail (url);

	entry = xmms_medialib_entry_new (url);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);
}

static guint
get_playlist_id (gchar *name)
{
	gint ret;
	guint id = 0;

	ret = xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &id,
	                         "select id from Playlist "
	                         "where name = '%s'", name);

	return ret ? id : 0;
}

static guint
prepare_playlist (guint id, gchar *name)
{
	gint ret;

	/* if the playlist doesn't exist yet, add it.
	 * if it does, delete the old entries
	 */
	if (id) {
		ret = xmms_sqlite_query (medialib->sql, NULL, NULL,
		                         "delete from PlaylistEntries "
		                         "where playlist_id = %u", id);
		return ret ? id : 0;
	}

	/* supplied id is zero, so we need to add a new playlist first */
	ret = xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &id,
	                         "select MAX (id) from Playlist");
	if (!ret) {
		return 0;
	}

	id++; /* we want MAX + 1 */

	ret = xmms_sqlite_query (medialib->sql, NULL, NULL,
	                         "insert into Playlist (id, name) "
	                         "values (%u, '%s')", id, name);
	return ret ? id : 0;
}

/**
 * Add a entry to a medialib playlist.
 *
 * @param playlist_id ID number of the playlist.
 * @param entry Entry to add to playlist
 * @returns TRUE upon success and FALSE if something went wrong
 */

gboolean
xmms_medialib_playlist_add (gint playlist_id, xmms_medialib_entry_t entry)
{
	gint ret;
	gchar mid[32];

	g_snprintf (mid, sizeof (mid), "mlib://%d", entry);

	g_mutex_lock (medialib->mutex);

	ret = xmms_sqlite_query (medialib->sql, NULL, NULL,
				 "insert into PlaylistEntries"
				 "(playlist_id, entry, pos) "
				 "values (%u, %Q, "

				 /* @todo use a real autoincrement column */
				 "ifnull((select MAX(ifnull(pos, 0)) from PlaylistEntries) + 1, 1))",
				 playlist_id, mid);

	if (!ret) {
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}

	g_mutex_unlock (medialib->mutex);

	return TRUE;

}


static gchar *
xmms_medialib_playlist_export (xmms_medialib_t *medialib, gchar *playlistname, 
			       gchar *mime, xmms_error_t *error)
{
	GString *str;
	GList *entries = NULL;
	guint *list;
	gint ret;
	gint i;
	gint plsid;
	
	g_mutex_lock (medialib->mutex);

	plsid = get_playlist_id (playlistname);
	if (!plsid) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "No such playlist!");
		g_mutex_unlock (medialib->mutex);
		return NULL;
	}

	ret = xmms_sqlite_query (medialib->sql, get_playlist_entries_cb, &entries,
	                         "select entry from PlaylistEntries "
	                         "where playlist_id = %u "
	                         "order by pos", plsid);

	if (!ret) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Failed to list entries!");
		g_mutex_unlock (medialib->mutex);
		return NULL;
	}
	g_mutex_unlock (medialib->mutex);

	entries = g_list_reverse (entries);

	list = g_malloc0 (sizeof (guint) * g_list_length (entries));
	i = 0;
	while (entries) {
		gchar *e = entries->data;

		if (g_strncasecmp (e, "mlib://", 7) == 0) {
			list[i] = atoi (e+7);
			i++;
		}

		g_free (e);
		entries = g_list_delete_link (entries, entries);
	}

	str = xmms_playlist_plugin_save (mime, list);

	if (!str) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Failed to generate playlist!");
		return NULL;
	}

	return str->str;
}

static void
xmms_medialib_playlist_remove (xmms_medialib_t *medialib, gchar *playlistname, xmms_error_t *error)
{
        gint playlist_id;

	g_mutex_lock (medialib->mutex);

	playlist_id = get_playlist_id (playlistname);
	if (!playlist_id) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "No such playlist!");
		g_mutex_unlock (medialib->mutex);
		return;
	}

	xmms_sqlite_query (medialib->sql, NULL, NULL, "delete from PlaylistEntries where playlist_id=%d", playlist_id);
	xmms_sqlite_query (medialib->sql, NULL, NULL, "delete from Playlist where id=%d", playlist_id);

	g_mutex_unlock (medialib->mutex);
}

static void
xmms_medialib_playlist_import (xmms_medialib_t *medialib, gchar *playlistname, 
			       gchar *url, xmms_error_t *error)
{
	gint playlist_id;
	xmms_medialib_entry_t entry = xmms_medialib_entry_new (url);
	xmms_config_value_t *cv;
	gint b;

	g_mutex_lock (medialib->mutex);
	playlist_id = get_playlist_id (playlistname);

	if (!(playlist_id = prepare_playlist (playlist_id, playlistname))) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Couldn't prepare playlist");
		g_mutex_unlock (medialib->mutex);

		return;
	}
	g_mutex_unlock (medialib->mutex);

	if (!xmms_playlist_plugin_import (playlist_id, entry)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Could not import playlist!");
		return;
	}

	cv = xmms_config_lookup ("medialib.playlist_load_on_import");

	b = xmms_config_value_int_get (cv);
	if (b) {
		xmms_medialib_playlist_load (medialib, playlistname, NULL);
	}

}


static void
xmms_medialib_playlist_save_current (xmms_medialib_t *medialib,
                                     gchar *name, xmms_error_t *error)
{
	GList *entries, *l;
	guint playlist_id;

	g_return_if_fail (medialib);
	g_return_if_fail (name);

	g_mutex_lock (medialib->mutex);

	playlist_id = get_playlist_id (name);

	if (!(playlist_id = prepare_playlist (playlist_id, name))) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Couldn't prepare playlist");

		g_mutex_unlock (medialib->mutex);
		return;
	}

	g_mutex_unlock (medialib->mutex);

	/* finally, add the playlist entries */
	entries = xmms_playlist_list (medialib->playlist, NULL);

	for (l = entries; l; l = g_list_next (l)) {
		xmms_medialib_entry_t entry = (xmms_medialib_entry_t) l->data;

		if (!entry) {
			return;
		}

		if (!xmms_medialib_playlist_add (playlist_id, entry)) {
			gchar buf[64];

			g_snprintf (buf, sizeof (buf),
				    "Couldn't add entry %u to playlist %u",
				    entry, playlist_id);

			xmms_error_set (error, XMMS_ERROR_GENERIC, buf);
			return;
		}

	}
}

static int
get_playlist_entries_cb (void *pArg, int argc, char **argv,
                         char **cName)
{
	GList **entries = pArg;

	/* valid prefixes for the playlist entries are:
	 * 'mlib://' and 'sql://', so any valid string is longer
	 * than 6 characters.
	 */
	if (argv[0] && strlen (argv[0]) > 6) {
		*entries = g_list_prepend (*entries, g_strdup (argv[0]));
	}

	return 0;
}

static int
playlist_load_sql_query_cb (void *pArg, int argc, char **argv, char **cName)
{
	xmms_medialib_t *medialib = pArg;

	xmms_playlist_addurl (medialib->playlist, argv[0], NULL);

	return 0;
}

static void
xmms_medialib_playlist_load (xmms_medialib_t *medialib, gchar *name,
                             xmms_error_t *error)
{
	GList *entries = NULL;
	gint ret;
	guint playlist_id;

	g_return_if_fail (medialib);
	g_return_if_fail (name);

	g_mutex_lock (medialib->mutex);

	if (!(playlist_id = get_playlist_id (name))) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "Playlist not found");
		g_mutex_unlock (medialib->mutex);

		return;
	}

	ret = xmms_sqlite_query (medialib->sql, get_playlist_entries_cb, &entries,
	                         "select entry from PlaylistEntries "
	                         "where playlist_id = %u "
	                         "order by pos", playlist_id);
	if (!ret) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Couldn't retrieve playlist entries");
		g_mutex_unlock (medialib->mutex);

		return;
	}

	/* we use g_list_prepend() in get_playlist_entries_cb(), so
	 * we need to reverse the list now
	 */
	entries = g_list_reverse (entries);

	while (entries) {
		gchar *entry = entries->data;

		if (!strncmp (entry, "mlib://", 7)) {
			xmms_medialib_entry_t e;
			e = atoi (entry+7);
			xmms_playlist_add (medialib->playlist, e, NULL);
		} else if (!strncmp (entry, "sql://", 6)) {
			xmms_sqlite_query (medialib->sql, playlist_load_sql_query_cb,
			                   medialib, "select url from Media where %q", entry);
		}

		g_free (entry);
		entries = g_list_delete_link (entries, entries);
	}

	g_mutex_unlock (medialib->mutex);
}

/**
 * Get a list of #GHashTables 's that matches the query.
 *
 * @param query SQL query that should be executed.
 * @param error In case of error this will be filled.
 * @returns GList containing GHashTables. Caller are responsible to 
 * free all memory.
 */
GList *
xmms_medialib_select (gchar *query, xmms_error_t *error)
{
	GList *res = NULL;
	gint ret;

	g_return_val_if_fail (query, 0);

	g_mutex_lock (medialib->mutex);

	ret = xmms_sqlite_query (medialib->sql, select_callback, (void *)&res, "%s", query);

	g_mutex_unlock (medialib->mutex);

	if (!ret)
		return NULL;

	res = g_list_reverse (res);
	return res;
}

/** @} */

/**
 * @internal
 */

void
xmms_medialib_playlist_save_autosaved ()
{
	xmms_error_t err;

	xmms_medialib_playlist_save_current (medialib, "autosaved", &err);
}

/**
 * @internal
 */

void
xmms_medialib_playlist_load_autosaved ()
{
	xmms_error_t err;

	xmms_medialib_playlist_load (medialib, "autosaved", &err);
}

/**
 * @internal
 * Get the next unresolved entry. Used by the mediainfo reader..
 */

xmms_medialib_entry_t
xmms_medialib_entry_not_resolved_get (void)
{
	xmms_medialib_entry_t ret = 0;

	g_mutex_lock (medialib->mutex);

	xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &ret,
			   "select id from Media where key='%s' and value='0' limit 1", 
			   XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);

	g_mutex_unlock (medialib->mutex);
	
	return ret;
}
