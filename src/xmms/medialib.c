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
static gboolean get_playlist_entries_cb (xmms_object_cmd_value_t **row, gpointer udata);
static gboolean xmms_medialib_int_cb (xmms_object_cmd_value_t **row, gpointer udata);

static GHashTable *xmms_medialib_info (xmms_medialib_t *playlist, guint32 id, xmms_error_t *err);
static void xmms_medialib_select_and_add (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error);
void xmms_medialib_add_entry (xmms_medialib_t *, gchar *, xmms_error_t *);
static GList *xmms_medialib_select_method (xmms_medialib_t *, gchar *, xmms_error_t *);
GList *xmms_medialib_select (xmms_medialib_session_t *, gchar *query, xmms_error_t *error);
static void xmms_medialib_playlist_save_current (xmms_medialib_t *, gchar *, xmms_error_t *);
static void xmms_medialib_playlist_load (xmms_medialib_t *, gchar *, xmms_error_t *);
static GList *xmms_medialib_playlist_list (xmms_medialib_t *, gchar *, xmms_error_t *);
static GList *xmms_medialib_playlists_list (xmms_medialib_t *, xmms_error_t *);
static void xmms_medialib_playlist_import (xmms_medialib_t *medialib, gchar *playlistname, 
					   gchar *url, xmms_error_t *error);
static gchar *xmms_medialib_playlist_export (xmms_medialib_t *medialib, gchar *playlistname, 
					     gchar *mime, xmms_error_t *error);
static void xmms_medialib_playlist_remove (xmms_medialib_t *medialib, gchar *playlistname, xmms_error_t *);
static void xmms_medialib_path_import (xmms_medialib_t *medialib, gchar *path, xmms_error_t *error);
static void xmms_medialib_rehash (xmms_medialib_t *medialib, guint32 id, xmms_error_t *error);
static guint32 xmms_medialib_entry_get_id (xmms_medialib_t *medialib, gchar *url, xmms_error_t *error);

XMMS_CMD_DEFINE (info, xmms_medialib_info, xmms_medialib_t *, DICT, UINT32, NONE);
XMMS_CMD_DEFINE (select, xmms_medialib_select_method, xmms_medialib_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE (mlib_add, xmms_medialib_add_entry, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (mlib_remove, xmms_medialib_entry_remove_method, xmms_medialib_t *, NONE, UINT32, NONE);
XMMS_CMD_DEFINE (playlist_save_current, xmms_medialib_playlist_save_current, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (playlist_load, xmms_medialib_playlist_load, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (addtopls, xmms_medialib_select_and_add, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (playlist_list, xmms_medialib_playlist_list, xmms_medialib_t *, LIST, STRING, NONE);
XMMS_CMD_DEFINE (playlists_list, xmms_medialib_playlists_list, xmms_medialib_t *, LIST, NONE, NONE);
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

	/** Statement to run when user is in "random mode" */
	const gchar *random_sql;
};

/**
 * This is handed out by xmms_medialib_begin()
 */
struct xmms_medialib_session_St {
	sqlite3 *sql;
	xmms_medialib_t *medialib;
	const char *file;
	int line;
};


/** 
  * Ok, so the functions are written with reentrency in mind, but
  * we choose to have a global medialib object here. It will be
  * much easier, and I don't see the real use of multiple medialibs
  * right now. This could be changed by removing this global one
  * and altering the function callers...
  */

static xmms_medialib_t *medialib;
static xmms_medialib_session_t *global_medialib_session;
static GMutex *global_medialib_session_mutex;

#define destroy_array(a) { gint i = 0; while (a[i]) { xmms_object_cmd_value_free (a[i]); i++; }; g_free (a); }


static void 
xmms_medialib_destroy (xmms_object_t *object)
{
	if (global_medialib_session) {
		xmms_sqlite_close (global_medialib_session->sql);
		g_free (global_medialib_session);
	}
	g_mutex_free (global_medialib_session_mutex);
	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_OUTPUT);
}

static void
xmms_medialib_path_changed (xmms_object_t *object, gconstpointer data,
							gpointer userdata)
{
	/*gboolean c;
	xmms_medialib_t *mlib = userdata;
	g_mutex_lock (mlib->mutex);
	xmms_sqlite_close (mlib->sql);
	medialib->sql = xmms_sqlite_open (&medialib->nextid, &c);
	g_mutex_unlock (mlib->mutex);*/
}

static void
xmms_medialib_random_sql_changed (xmms_object_t *object, gconstpointer data,
								  gpointer userdata)
{
	medialib->random_sql = (gchar*)data;
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
	xmms_medialib_session_t *session;
	xmms_config_value_t *cv;
	gint c = 0;

	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);
	medialib->playlist = playlist;

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIALIB, XMMS_OBJECT (medialib));
	xmms_ipc_broadcast_register (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
	xmms_ipc_broadcast_register (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_PLAYLIST_LOADED);

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
			     XMMS_IPC_CMD_PLAYLIST_LIST,
			     XMMS_CMD_FUNC (playlist_list));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
			     XMMS_IPC_CMD_PLAYLISTS_LIST,
			     XMMS_CMD_FUNC (playlists_list));
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

	cv = xmms_config_value_register ("medialib.random_sql_statement",
					 "select id as value from Media where key='url' order by random() limit 1",
					 xmms_medialib_random_sql_changed, medialib);
	medialib->random_sql = xmms_config_value_string_get (cv);

	g_snprintf (path, XMMS_PATH_MAX, "%s/.xmms2/medialib.db", g_get_home_dir());

	xmms_config_value_register ("medialib.path",
				    path,
				    xmms_medialib_path_changed, medialib);

	global_medialib_session = NULL;

#ifdef OLD_SQLITE_VERSION
	global_medialib_session = g_new0 (xmms_medialib_session_t, 1);
	global_medialib_session->medialib = medialib;
	global_medialib_session->file = "global";
	global_medialib_session->line = 0;
	global_medialib_session->sql = xmms_sqlite_open (&c);
#endif

	global_medialib_session_mutex = g_mutex_new ();

	session = xmms_medialib_begin ();
	xmms_medialib_end (session);
	/*
	medialib->sql = xmms_sqlite_open (&medialib->nextid, &create);
	*/
	
	return TRUE;
}

/** Session handling */

xmms_medialib_session_t *
_xmms_medialib_begin (const char *file, int line)
{
	gboolean create;
	xmms_medialib_session_t *session;

	if (global_medialib_session) {
		g_mutex_lock (global_medialib_session_mutex);
		return global_medialib_session;
	}

	session = g_new0 (xmms_medialib_session_t, 1);
	session->medialib = medialib;
	session->file = file;
	session->line = line;
	xmms_object_ref (XMMS_OBJECT (medialib));

	session->sql = xmms_sqlite_open (&create);
	/*
	if (!xmms_sqlite_exec (session->sql, "BEGIN")) {
		XMMS_DBG ("could not start new session!");
	}
	*/

	if (create) {
		xmms_medialib_entry_t entry;
		entry = xmms_medialib_entry_new (session, "file://" SHAREDDIR "/dismantled-the_swarm_clip.ogg");
		xmms_playlist_add (medialib->playlist, entry, NULL);
	}

	return session;
}

void
xmms_medialib_commit (xmms_medialib_session_t *session)
{
	g_return_if_fail (session);
	XMMS_DBG ("Commiting and begining session %s:%d", session->file, session->line);
	/*
	if (!xmms_sqlite_exec (session->sql, "COMMIT")) {
		XMMS_DBG ("error when commiting this session!");
	}
	if (!xmms_sqlite_exec (session->sql, "BEGIN")) {
		XMMS_DBG ("error when beginning a new session!");
	}
	*/

}

void
xmms_medialib_rollback (xmms_medialib_session_t *session)
{
	g_return_if_fail (session);
	XMMS_DBG ("Rolling back session %s:%d", session->file, session->line);
	/*
	xmms_sqlite_exec (session->sql, "ROLLBACK");
	xmms_sqlite_exec (session->sql, "BEGIN");
	*/
}


void
xmms_medialib_end (xmms_medialib_session_t *session)
{
	g_return_if_fail (session);

	if (session == global_medialib_session) {
		g_mutex_unlock (global_medialib_session_mutex);
		return;
	}

	xmms_sqlite_close (session->sql);
	xmms_object_unref (XMMS_OBJECT (session->medialib));
	g_free (session);
}

/**
 * Return a random entry from the medialib.
 * The SQL statement used for retrival can be configured.
 */

guint32
xmms_medialib_get_random_entry (xmms_medialib_session_t *session)
{
	guint32 ret = 0;

	g_return_val_if_fail (session, 0);
	xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &ret, 
							 medialib->random_sql);
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
xmms_medialib_logging_start (xmms_medialib_session_t *session,
							 xmms_medialib_entry_t entry)
{
	time_t starttime;
	gboolean ret;
	xmms_config_value_t *cv;

	g_return_if_fail (entry);
	g_return_if_fail (session);
	
	cv = xmms_config_lookup ("medialib.dologging");
	g_return_if_fail (cv);
	
	ret = xmms_config_value_int_get (cv);
	if (!ret)
		return;

	starttime = time (NULL);
	ret = xmms_sqlite_exec (session->sql, "INSERT INTO Log (id, starttime) VALUES (%u, %u)", 
							entry, (guint) starttime);

	if (ret) {
		xmms_medialib_entry_property_set_int (session, entry, "laststarted", starttime);
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
xmms_medialib_logging_stop (xmms_medialib_session_t *session,
							xmms_medialib_entry_t entry, 
							guint playtime)
{
	gint sek;
	gint value = 0.0;
	gboolean ret;
	xmms_config_value_t *cv;

	g_return_if_fail (session);

	cv = xmms_config_lookup ("medialib.dologging");
	g_return_if_fail (cv);

	ret = xmms_config_value_int_get (cv);
	if (!ret)
		return;

	sek = xmms_medialib_entry_property_get_int (session, 
												entry, 
												XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION);
	value = (gint) (100.0 * playtime / (gdouble)sek);
		
	sek = xmms_medialib_entry_property_get_int (session, entry, "laststarted");
	g_return_if_fail (sek);

	ret = xmms_sqlite_exec (session->sql, 
							"UPDATE Log SET value=%d WHERE id=%u AND starttime=%d", 
							value, entry, sek);
}


static gboolean
xmms_medialib_int_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	guint *i = udata;

	if (row && row[0] && row[0]->type == XMMS_OBJECT_CMD_ARG_INT32)
		*i = row[0]->value.int32;
	else
		XMMS_DBG ("Expected int32 but got something else!");

	destroy_array (row);

	return TRUE;
}

static int
xmms_medialib_string_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	gchar **str = udata;

	if (row && row[0] && row[0]->type == XMMS_OBJECT_CMD_ARG_STRING)
		*str = g_strdup (row[0]->value.string);
	else
		XMMS_DBG ("Expected string but got something else!");

	destroy_array (row);

	return 0;
}

static int
xmms_medialib_cmd_value_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	xmms_object_cmd_value_t **ret = udata;

	*ret = xmms_object_cmd_value_copy (row[0]);

	destroy_array (row);

	return 0;
}

/**
 * Retrieve a property from an entry
 *
 * @see xmms_medialib_entry_property_get_str
 */

xmms_object_cmd_value_t *
xmms_medialib_entry_property_get_cmd_value (xmms_medialib_session_t *session,
											xmms_medialib_entry_t entry, 
											const gchar *property)
{
	xmms_object_cmd_value_t *ret = NULL;

	g_return_val_if_fail (property, NULL);
	g_return_val_if_fail (session, NULL);
	
	xmms_sqlite_query_array (session->sql, xmms_medialib_cmd_value_cb, 
							 &ret, "select value from Media where key=%Q and id=%d", 
							 property, entry);

	return ret;
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
xmms_medialib_entry_property_get_str (xmms_medialib_session_t *session,
									  xmms_medialib_entry_t entry, 
									  const gchar *property)
{
	gchar *ret = NULL;

	g_return_val_if_fail (property, NULL);
	g_return_val_if_fail (session, NULL);

	xmms_sqlite_query_array (session->sql, xmms_medialib_string_cb, &ret,
							 "select value from Media where key=%Q and id=%d", 
							 property, entry);

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
xmms_medialib_entry_property_get_int (xmms_medialib_session_t *session,
									  xmms_medialib_entry_t entry, 
									  const gchar *property)
{
	guint ret = 0;

	g_return_val_if_fail (property, 0);
	g_return_val_if_fail (session, 0);

	xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &ret,
							 "select value from Media where key=%Q and id=%d", 
							 property, entry);

	return ret;
}

/**
 * Set a entry property to a new value, overwriting the old value.
 *
 * @param entry Entry to alter.
 * @param property The property to extract. Strings passed should 
 * be defined in medialib.h 
 * @param value gint with the new value, will be copied in to the medialib
 *
 * @returns TRUE on success and FALSE on failure.
 */

gboolean
xmms_medialib_entry_property_set_int (xmms_medialib_session_t *session,
									  xmms_medialib_entry_t entry, 
									  const gchar *property, gint value)
{
	gboolean ret;

	g_return_val_if_fail (property, FALSE);
	g_return_val_if_fail (session, FALSE);

	ret = xmms_sqlite_exec (session->sql,
							"insert or replace into Media (id, value, key) values (%d, %d, LOWER(%Q))", 
							entry, value, property);

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
xmms_medialib_entry_property_set_str (xmms_medialib_session_t *session,
									  xmms_medialib_entry_t entry, 
									  const gchar *property, const gchar *value)
{
	gboolean ret;

	g_return_val_if_fail (property, FALSE);
	g_return_val_if_fail (session, FALSE);

	if (value && !g_utf8_validate (value, -1, NULL)) {
		XMMS_DBG ("OOOOOPS! Trying to set property %s to a NON UTF-8 string (%s) I will deny that!", property, value);
		return FALSE;
	}

	ret = xmms_sqlite_exec (session->sql,
							"insert or replace into Media (id, value, key) values (%d, %Q, LOWER(%Q))", 
							entry, value, property);

	return ret;

}

/**
 * A nice wrapper for @code xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_PROPERTY_RESOLVED); @endcode
 *
 * @return TRUE if the entry was resolved, else false.
 */

gboolean
xmms_medialib_entry_is_resolved (xmms_medialib_session_t *session,
								 xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (session,
												 entry, 
												 XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);
}

/**
 * A nice wrapper for @code xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_PROPERTY_ID); @endcode
 *
 * @return TRUE if the entry was resolved, else false.
 */

guint
xmms_medialib_entry_id_get (xmms_medialib_session_t *session, 
							xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (session, 
												 entry, 
												 XMMS_MEDIALIB_ENTRY_PROPERTY_ID);
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
	xmms_object_emit_f (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE, 
						XMMS_OBJECT_CMD_ARG_UINT32, entry);
}

static gboolean
xmms_medialib_addtopls_cb (GHashTable *row, gpointer udata)
{
	xmms_playlist_t *playlist = udata;
	xmms_object_cmd_value_t *val;

	val = g_hash_table_lookup (row, "id");
	if (val && val->type == XMMS_OBJECT_CMD_ARG_INT32) {
		xmms_playlist_add (playlist, val->value.int32, NULL);
	}

	g_hash_table_destroy (row);

	return TRUE;
}

static void
xmms_medialib_select_and_add (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error)
{
	xmms_medialib_session_t *session;
	g_return_if_fail (medialib);
	g_return_if_fail (query);

	session = xmms_medialib_begin ();

	if (!xmms_sqlite_query_table (session->sql, xmms_medialib_addtopls_cb, 
								  session->medialib->playlist,
								  "%s", query)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Query failed!");
		xmms_medialib_end (session);
		return;
	}

	xmms_medialib_end (session);

}

static void
xmms_medialib_entry_remove_method (xmms_medialib_t *medialib, guint32 entry, xmms_error_t *error)
{
	xmms_medialib_session_t *session;
	session = xmms_medialib_begin ();
	xmms_medialib_entry_remove (session, entry);
	xmms_medialib_end (session);
}

/**
 * Remove a medialib entry from the database
 *
 * @param entry Entry to remove
 */

void
xmms_medialib_entry_remove (xmms_medialib_session_t *session,
							xmms_medialib_entry_t entry)
{
	g_return_if_fail (session);
	xmms_sqlite_exec (session->sql, "delete from Media where id=%d", entry);
	xmms_sqlite_exec (session->sql, "delete from Log where id=%d", entry);
	/** @todo remove from playlists too? or should we do refcounting on this? */
}

static gboolean
process_dir (xmms_medialib_session_t *session, const gchar *path, xmms_error_t *error)
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
			if (!process_dir (session, realfile, error))
				return FALSE;
		} else if (g_file_test (realfile, G_FILE_TEST_EXISTS)) {
			gchar *f = g_strdup_printf ("file://%s", realfile);
			xmms_medialib_entry_new (session, f);
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
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();

	if (id) {
		xmms_sqlite_exec (session->sql, "update Media set value = 0 where key='%s' and id=%d", 
						  XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED, id);
	} else {
		xmms_sqlite_exec (session->sql, "update Media set value = 0 where key='%s'", 
						  XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);
	}

	xmms_medialib_end (session);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);
	
}

static void 
xmms_medialib_path_import (xmms_medialib_t *medialib, gchar *path, xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;
	xmms_medialib_session_t *session;
	gchar *p;

	g_return_if_fail (medialib);
	g_return_if_fail (path);

	session = xmms_medialib_begin ();

	p = path+strlen(path);

	while (*(p-1) == '/')
		*p--;

	*p = '\0';

	if (process_dir (session, path, error)) {
		xmms_medialib_commit (session);
	} else {
		xmms_medialib_rollback (session);
	}

	xmms_medialib_end (session);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);
	
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
xmms_medialib_entry_new (xmms_medialib_session_t *session, const char *url)
{
	guint id = 0;
	guint ret = 0;

	g_return_val_if_fail (url, 0);
	g_return_val_if_fail (session, 0);

	xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &id, 
							 "select id as value from Media where key='%s' and value=%Q", 
							 XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url);

	if (id) {
		ret = id;
	} else {
		if (!xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &ret,
									  "select MAX (id) from Media")) {
			return 0;
		}

		ret++; /* next id */
		
		if (!xmms_sqlite_exec (session->sql,
							   "insert into Media (id, key, value) values (%d, '%s', %Q)",
							   ret, XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url)) {
			return 0;
		}
		if (!xmms_sqlite_exec (session->sql,
							   "insert or replace into Media (id, key, value) values (%d, '%s', 0)",
							   ret, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED)) {
			return 0;
		}
		if (!xmms_sqlite_exec (session->sql,
							   "insert or replace into Media (id, key, value) values (%d, '%s', %d)",
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
	xmms_medialib_session_t *session = xmms_medialib_begin ();

	xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &id, 
							 "select id as value from Media where key='%s' and value=%Q", 
							 XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url);
	xmms_medialib_end (session);

	return id;
}

static gboolean
xmms_medialib_hashtable_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	GHashTable *hash = udata;

	g_hash_table_insert (hash, g_strdup (row[1]->value.string), 
			     xmms_object_cmd_value_copy (row[2]));

	destroy_array (row);

	return TRUE;
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
xmms_medialib_entry_to_hashtable (xmms_medialib_session_t *session, xmms_medialib_entry_t entry)
{
	GHashTable *ret;

	g_return_val_if_fail (session, NULL);
	
	ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, xmms_object_cmd_value_free);

	xmms_sqlite_query_array (session->sql, xmms_medialib_hashtable_cb, ret, 
							 "select * from Media where id=%d", entry);

	g_hash_table_insert (ret, g_strdup ("id"), xmms_object_cmd_value_int_new (entry));

	return ret;
}


static GHashTable *
xmms_medialib_info (xmms_medialib_t *medialib, guint32 id, xmms_error_t *err)
{
	xmms_medialib_session_t *session;
	GHashTable *ret;

	session = xmms_medialib_begin ();
	ret = xmms_medialib_entry_to_hashtable (session, id);
	xmms_medialib_end (session);

	if (!ret) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Could not retrive info for that entry!");
		return NULL;
	}
	return ret;
}

static gboolean
select_callback (GHashTable *row, gpointer udata)
{
	GList **l = (GList **) udata;

	*l = g_list_prepend (*l, xmms_object_cmd_value_dict_new (row));
	return TRUE;
}

static GList *
xmms_medialib_select_method (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error)
{
	GList *ret;
	xmms_medialib_session_t *session = xmms_medialib_begin ();
	ret = xmms_medialib_select (session, query, error);
	return ret;
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
	xmms_medialib_session_t *session;

	g_return_if_fail (medialib);
	g_return_if_fail (url);

	session = xmms_medialib_begin ();

	entry = xmms_medialib_entry_new (session, url);

	xmms_medialib_end (session);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);
}

static guint
get_playlist_id (xmms_medialib_session_t *session, gchar *name)
{
	gint ret;
	guint id = 0;

	g_return_val_if_fail (session, 0);

	ret = xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &id,
								   "select id as value from Playlist "
								   "where name = '%s'", name);

	return ret ? id : 0;
}

static guint
prepare_playlist (xmms_medialib_session_t *session,
				  guint id, 
				  gchar *name)
{
	gint ret;

	g_return_val_if_fail (session, 0);

	/* if the playlist doesn't exist yet, add it.
	 * if it does, delete the old entries
	 */
	if (id) {
		ret = xmms_sqlite_exec (session->sql,
								"delete from PlaylistEntries "
								"where playlist_id = %u", id);
		return ret ? id : 0;
	}

	/* supplied id is zero, so we need to add a new playlist first */
	ret = xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &id,
								   "select MAX (id) as value from Playlist");
	if (!ret) {
		return 0;
	}

	id++; /* we want MAX + 1 */

	ret = xmms_sqlite_exec (session->sql,
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
xmms_medialib_playlist_add (xmms_medialib_session_t *session, 
							gint playlist_id, 
							xmms_medialib_entry_t entry)
{
	gint ret;
	gchar mid[32];

	g_return_val_if_fail (session, FALSE);

	g_snprintf (mid, sizeof (mid), "mlib://%d", entry);

	ret = xmms_sqlite_exec (session->sql,
							"insert into PlaylistEntries"
							"(playlist_id, entry, pos) "
							"values (%u, %Q, "

							/* @todo use a real autoincrement column */
							"ifnull((select MAX(ifnull(pos, 0)) from PlaylistEntries) + 1, 1))",
							playlist_id, mid);
	if (!ret) {
		return FALSE;
	}

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
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();
	
	plsid = get_playlist_id (session, playlistname);
	if (!plsid) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "No such playlist!");
		xmms_medialib_end (session);
		return NULL;
	}

	ret = xmms_sqlite_query_array (session->sql, get_playlist_entries_cb, &entries,
								   "select entry from PlaylistEntries "
								   "where playlist_id = %u "
								   "order by pos", plsid);

	xmms_medialib_end (session);

	if (!ret) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Failed to list entries!");
		return NULL;
	}

	entries = g_list_reverse (entries);

	list = g_malloc0 (sizeof (guint) * g_list_length (entries) + 1);
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
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();

	playlist_id = get_playlist_id (session, playlistname);
	if (!playlist_id) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "No such playlist!");
		xmms_medialib_end (session);
		return;
	}

	xmms_sqlite_exec (session->sql, "delete from PlaylistEntries where playlist_id=%d", playlist_id);
	xmms_sqlite_exec (session->sql, "delete from Playlist where id=%d", playlist_id);

	xmms_medialib_end (session);
}

static gboolean
xmms_medialib_playlist_list_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	GList **n = udata;
	/* strip mlib:// */
	if (g_strncasecmp (row[0]->value.string, "mlib", 4) == 0) {
		char *p = row[0]->value.string + 7;
		*n = g_list_prepend (*n,
				xmms_object_cmd_value_uint_new((atoi(p))));
	}

	destroy_array (row);

	return TRUE;
}

static GList *
xmms_medialib_playlist_list (xmms_medialib_t *medialib, gchar *playlistname, xmms_error_t *error)
{
	GList *ret = NULL;
	gint playlist_id;
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();
	
	playlist_id = get_playlist_id (session, playlistname);
	if (!playlist_id) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "No such playlist!");
		xmms_medialib_end (session);
		return NULL;
	}
	
	/* sorted by pos? */
	xmms_sqlite_query_array (session->sql, xmms_medialib_playlist_list_cb, &ret, "select entry from Playlistentries where playlist_id=%d order by pos", playlist_id);
	
	ret = g_list_reverse (ret);
	xmms_medialib_end (session);
		
	return ret;
}

static gboolean
xmms_medialib_playlists_list_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	GList **n = udata;
	*n = g_list_prepend (*n, xmms_object_cmd_value_str_new(row[0]->value.string));

	destroy_array (row);

	return TRUE;
}

static GList *
xmms_medialib_playlists_list (xmms_medialib_t *medialib, xmms_error_t *error)
{
	GList *ret = NULL;
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();
	
	/* order by (smth)? */
	xmms_sqlite_query_array (session->sql, xmms_medialib_playlists_list_cb, &ret, "select name from Playlist");
	
	ret = g_list_reverse (ret);
	xmms_medialib_end (session);
	
	return ret;
}

static void
xmms_medialib_playlist_import (xmms_medialib_t *medialib, gchar *playlistname, 
							   gchar *url, xmms_error_t *error)
{
	gint playlist_id;
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin ();
	entry = xmms_medialib_entry_new (session, url);

	playlist_id = get_playlist_id (session, playlistname);

	if (!(playlist_id = prepare_playlist (session, playlist_id, playlistname))) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
						"Couldn't prepare playlist");
		xmms_medialib_end (session);

		return;
	}

	xmms_medialib_end (session);

	if (!xmms_playlist_plugin_import (playlist_id, entry)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Could not import playlist!");
		return;
	}

	xmms_mediainfo_reader_wakeup (xmms_playlist_mediainfo_reader_get (medialib->playlist));

}


static void
xmms_medialib_playlist_save_current (xmms_medialib_t *medialib,
                                     gchar *name, xmms_error_t *error)
{
	GList *entries, *l;
	guint playlist_id;
	xmms_medialib_session_t *session;

	g_return_if_fail (medialib);
	g_return_if_fail (name);

	session = xmms_medialib_begin ();

	playlist_id = get_playlist_id (session, name);

	if (!(playlist_id = prepare_playlist (session, playlist_id, name))) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Couldn't prepare playlist");

		xmms_medialib_end (session);
		return;
	}
		
	/* finally, add the playlist entries */
	entries = xmms_playlist_list (medialib->playlist, NULL);

	for (l = entries; l; l = g_list_next (l)) {
		xmms_object_cmd_value_t *val = l->data;
		xmms_medialib_entry_t entry = (xmms_medialib_entry_t) val->value.uint32;

		if (!entry) {
			xmms_medialib_end (session);
			return;
		}

		if (!xmms_medialib_playlist_add (session, playlist_id, entry)) {
			gchar buf[64];

			g_snprintf (buf, sizeof (buf),
				    "Couldn't add entry %u to playlist %u",
				    entry, playlist_id);

			xmms_error_set (error, XMMS_ERROR_GENERIC, buf);
			xmms_medialib_end (session);
			return;
		}

	}

	xmms_medialib_end (session);
}

static gboolean
get_playlist_entries_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	GList **entries = udata;

	/* 
	 * valid prefixes for the playlist entries are:
	 * 'mlib://' and 'sql://', so any valid string is longer
	 * than 6 characters.
	 */
	if (row && row[0] && row[0]->type == XMMS_OBJECT_CMD_ARG_STRING) {
		*entries = g_list_prepend (*entries, g_strdup (row[0]->value.string));
	}

	destroy_array (row);
	
	return TRUE;
}

static gboolean
playlist_load_sql_query_cb (xmms_object_cmd_value_t **row, gpointer udata)
{
	xmms_medialib_t *medialib = udata;

	xmms_playlist_addurl (medialib->playlist, row[0]->value.string, NULL);

	destroy_array (row);

	return 0;
}

static void
xmms_medialib_playlist_load (xmms_medialib_t *medialib, gchar *name,
                             xmms_error_t *error)
{
	GList *entries = NULL;
	gint ret;
	guint playlist_id;
	xmms_medialib_session_t *session;

	g_return_if_fail (medialib);
	g_return_if_fail (name);

	session = xmms_medialib_begin ();

	if (!(playlist_id = get_playlist_id (session, name))) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "Playlist not found");
		xmms_medialib_end (session);
		return;
	}

	ret = xmms_sqlite_query_array (session->sql, get_playlist_entries_cb, &entries,
				       "select entry from PlaylistEntries "
				       "where playlist_id = %u "
				       "order by pos", playlist_id);
	if (!ret) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Couldn't retrieve playlist entries");
		xmms_medialib_end (session);

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
			xmms_sqlite_query_array (session->sql, playlist_load_sql_query_cb,
						 medialib, "select url from Media where %q", entry);
		}

		g_free (entry);
		entries = g_list_delete_link (entries, entries);
	}

	xmms_object_emit_f (XMMS_OBJECT (medialib), 
			    XMMS_IPC_SIGNAL_MEDIALIB_PLAYLIST_LOADED, 
			    XMMS_OBJECT_CMD_ARG_STRING,
			    name);
	xmms_medialib_end (session);
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
xmms_medialib_select (xmms_medialib_session_t *session,
					  gchar *query, xmms_error_t *error)
{
	GList *res = NULL;
	gint ret;

	g_return_val_if_fail (query, 0);
	g_return_val_if_fail (session, 0);

	ret = xmms_sqlite_query_table (session->sql, select_callback, (void *)&res, "%s", query);

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
xmms_medialib_entry_not_resolved_get (xmms_medialib_session_t *session)
{
	xmms_medialib_entry_t ret = 0;

	g_return_val_if_fail (session, 0);

	xmms_sqlite_query_array (session->sql, xmms_medialib_int_cb, &ret,
				 "select id as value from Media where key='%s' and value=0 limit 1", 
				 XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);

	return ret;
}
