/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#include "xmms_configuration.h"
#include "xmmspriv/xmms_medialib.h"
#include "xmmspriv/xmms_xform.h"
#include "xmmspriv/xmms_utils.h"
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
gchar *xmms_medialib_url_encode (const gchar *path);
static gboolean xmms_medialib_check_id_in_session (xmms_medialib_entry_t entry, xmms_medialib_session_t *session);

static void xmms_medialib_add_entry (xmms_medialib_t *, const gchar *, xmms_error_t *);
static void xmms_medialib_move_entry (xmms_medialib_t *, guint32 entry, const gchar *, xmms_error_t *);
static void xmms_medialib_path_import (xmms_medialib_t *medialib, const gchar *path, xmms_error_t *error);
static void xmms_medialib_rehash (xmms_medialib_t *medialib, guint32 id, xmms_error_t *error);
static void xmms_medialib_property_set_str_method (xmms_medialib_t *medialib, guint32 entry, const gchar *source, const gchar *key, const gchar *value, xmms_error_t *error);
static void xmms_medialib_property_set_int_method (xmms_medialib_t *medialib, guint32 entry, const gchar *source, const gchar *key, gint32 value, xmms_error_t *error);
static void xmms_medialib_property_remove_method (xmms_medialib_t *medialib, guint32 entry, const gchar *source, const gchar *key, xmms_error_t *error);
static gint32 xmms_medialib_entry_get_id (xmms_medialib_t *medialib, const gchar *url, xmms_error_t *error);


XMMS_CMD_DEFINE (info, xmms_medialib_info, xmms_medialib_t *, DICT, INT32, NONE);
XMMS_CMD_DEFINE (mlib_add, xmms_medialib_add_entry, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (mlib_remove, xmms_medialib_entry_remove_method, xmms_medialib_t *, NONE, INT32, NONE);
XMMS_CMD_DEFINE (mlib_move, xmms_medialib_move_entry, xmms_medialib_t *, NONE, INT32, STRING);
XMMS_CMD_DEFINE (path_import, xmms_medialib_path_import, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (rehash, xmms_medialib_rehash, xmms_medialib_t *, NONE, INT32, NONE);
XMMS_CMD_DEFINE (get_id, xmms_medialib_entry_get_id, xmms_medialib_t *, INT32, STRING, NONE);

XMMS_CMD_DEFINE4 (set_property_str, xmms_medialib_property_set_str_method, xmms_medialib_t *, NONE, INT32, STRING, STRING, STRING);
XMMS_CMD_DEFINE4 (set_property_int, xmms_medialib_property_set_int_method, xmms_medialib_t *, NONE, INT32, STRING, STRING, INT32);

XMMS_CMD_DEFINE3 (remove_property, xmms_medialib_property_remove_method, xmms_medialib_t *, NONE, INT32, STRING, STRING);

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

	GMutex *source_lock;
	GHashTable *sources;
};

/**
 * This is handed out by xmms_medialib_begin()
 */
struct xmms_medialib_session_St {
	xmms_medialib_t *medialib;

	/** The SQLite handler */
	sqlite3 *sql;

	/** debug file */
	const gchar *file;
	/** debug line number */
	gint line;

	/* Write or read lock, true if write */
	gboolean write;

	gint next_id;
};


/**
  * Ok, so the functions are written with reentrency in mind, but
  * we choose to have a global medialib object here. It will be
  * much easier, and I don't see the real use of multiple medialibs
  * right now. This could be changed by removing this global one
  * and altering the function callers...
  */
static xmms_medialib_t *medialib;

static const char source_pref[] = "server:client/*:plugin/id3v2:plugin/*";

/**
  * This is only used if we are using a older version of sqlite.
  * The reason for this is that we must have a global session, due to some
  * strange limitiations in older sqlite libraries.
  */
static xmms_medialib_session_t *global_medialib_session;

/** This protects the above global session */
static GMutex *global_medialib_session_mutex;

static GMutex *xmms_medialib_debug_mutex;
static GHashTable *xmms_medialib_debug_hash;

static void
xmms_medialib_destroy (xmms_object_t *object)
{
	xmms_medialib_t *mlib = (xmms_medialib_t *)object;
	if (global_medialib_session) {
		xmms_sqlite_close (global_medialib_session->sql);
		g_free (global_medialib_session);
	}
	g_mutex_free (mlib->source_lock);
	g_hash_table_destroy (mlib->sources);
	g_mutex_free (global_medialib_session_mutex);
	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_OUTPUT);
}

#define XMMS_MEDIALIB_SOURCE_SERVER "server"
#define XMMS_MEDIALIB_SOURCE_SERVER_ID 1

static gint
source_match_pattern (const gchar *source, const gchar *pattern,
                      gint pattern_len)
{
	/* check whether we need to keep a wildcard in mind when matching
	 * the strings.
	 */
	if (pattern_len > 0 && pattern[pattern_len - 1] == '*') {
		/* if the asterisk is the first character of the pattern,
		 * it obviously accepts anything.
		 */
		if (pattern_len == 1) {
			return TRUE;
		}

		/* otherwise we have to compare the characters just up to the
		 * asterisk.
		 */
		return !g_ascii_strncasecmp (source, pattern, pattern_len - 1);
	}

	/* there's no wildcards, so just compare all of the characters. */
	return !g_ascii_strncasecmp (pattern, source, pattern_len);
}

static void
xmms_sqlite_source_pref (sqlite3_context *context, int args, sqlite3_value **val)
{
	gint source;
	const gchar *pref;
	xmms_medialib_t *mlib;
	gchar *source_name, *colon;
	gint i;

	mlib = sqlite3_user_data (context);

	if (sqlite3_value_type (val[0]) != SQLITE_INTEGER) {
		sqlite3_result_error (context, "First argument to xmms_source_pref should be a integer", -1);
		return;
	}
	if (sqlite3_value_type (val[1]) != SQLITE3_TEXT) {
		sqlite3_result_error (context, "Second argument to xmms_source_pref should be a string", -1);
		return;
	}

	source = sqlite3_value_int (val[0]);
	pref = (const gchar *) sqlite3_value_text (val[1]);

	g_mutex_lock (mlib->source_lock);
	source_name = g_hash_table_lookup (mlib->sources, GINT_TO_POINTER (source));
	g_mutex_unlock (mlib->source_lock);

	do {
		gsize len;

		colon = strchr (pref, ':');

		/* get the length of this substring */
		len = colon ? colon - pref : strlen (pref);

		/* check whether the substring matches */
		if (source_match_pattern (source_name, pref, len)) {
			sqlite3_result_int (context, i);
			return;
		}

		if (colon) {
			pref = colon + 1;
		}

		/* if we just processed the final substring, then we're done */
	} while (colon);

	sqlite3_result_int (context, -1);
}

int
add_to_source (void *hash, int columns, char **vals, char **cols)
{
	int source = strtol (vals[0], NULL, 10);
	g_hash_table_insert ((GHashTable*)hash, GINT_TO_POINTER (source), g_strdup (vals[1]));
	return 0;
}

guint32
xmms_medialib_source_to_id (xmms_medialib_session_t *session,
                            const gchar *source)
{
	gint32 ret = 0;
	g_return_val_if_fail (source, 0);

	xmms_sqlite_query_int (session->sql, &ret,
	                       "SELECT id FROM Sources WHERE source=%Q",
	                       source);
	if (ret == 0) {
		xmms_sqlite_exec (session->sql,
		                  "INSERT INTO Sources (source) VALUES (%Q)", source);
		xmms_sqlite_query_int (session->sql, &ret,
		                       "SELECT id FROM Sources WHERE source=%Q",
		                       source);
		XMMS_DBG ("Added source %s with id %d", source, ret);
		g_mutex_lock (session->medialib->source_lock);
		g_hash_table_insert (session->medialib->sources, GUINT_TO_POINTER (ret), g_strdup (source));
		g_mutex_unlock (session->medialib->source_lock);
	}

	return ret;
}


static xmms_medialib_session_t *
xmms_medialib_session_new (const char *file, int line)
{
	xmms_medialib_session_t *session;

	session = g_new0 (xmms_medialib_session_t, 1);
	session->medialib = medialib;
	session->file = file;
	session->line = line;
	session->sql = xmms_sqlite_open ();

	sqlite3_create_function (session->sql, "xmms_source_pref", 2, SQLITE_UTF8,
	                         session->medialib, xmms_sqlite_source_pref, NULL, NULL);

	return session;
}



/**
 * Initialize the medialib and open the database file.
 *
 * @param playlist the current playlist pointer
 * @returns TRUE if successful and FALSE if there was a problem
 */

xmms_medialib_t *
xmms_medialib_init (xmms_playlist_t *playlist)
{
	gchar *path;
	xmms_medialib_session_t *session;
	gboolean create;

	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);
	medialib->playlist = playlist;

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIALIB, XMMS_OBJECT (medialib));
	xmms_ipc_broadcast_register (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED);
	xmms_ipc_broadcast_register (XMMS_OBJECT (medialib), XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);

	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_INFO,
	                     XMMS_CMD_FUNC (info));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_ADD_URL,
	                     XMMS_CMD_FUNC (mlib_add));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_REMOVE_ID,
	                     XMMS_CMD_FUNC (mlib_remove));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PATH_IMPORT,
	                     XMMS_CMD_FUNC (path_import));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_REHASH,
	                     XMMS_CMD_FUNC (rehash));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_GET_ID,
	                     XMMS_CMD_FUNC (get_id));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PROPERTY_SET_STR,
	                     XMMS_CMD_FUNC (set_property_str));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PROPERTY_SET_INT,
	                     XMMS_CMD_FUNC (set_property_int));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PROPERTY_REMOVE,
	                     XMMS_CMD_FUNC (remove_property));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_MOVE_URL,
	                     XMMS_CMD_FUNC (mlib_move));

	path = XMMS_BUILD_PATH ("medialib.db");

	xmms_config_property_register ("medialib.path", path, NULL, NULL);
	xmms_config_property_register ("medialib.analyze_on_startup", "0", NULL, NULL);
	xmms_config_property_register ("medialib.allow_remote_fs",
	                               "0", NULL, NULL);

	g_free (path);


	xmms_medialib_debug_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	xmms_medialib_debug_mutex = g_mutex_new ();
	global_medialib_session = NULL;

	/* init the database */
	xmms_sqlite_create (&create);

	if (!sqlite3_threadsafe ()) {
		xmms_log_info ("********************************************************************");
		xmms_log_info ("* Using thread hack to compensate for sqlite without threadsafety! *");
		xmms_log_info ("* This can be a huge performance penalty - upgrade or recompile    *");
		xmms_log_info ("********************************************************************");
		/** Create a global session, this is only used when the sqlite version
		* doesn't support concurrent sessions */
		global_medialib_session = xmms_medialib_session_new ("global", 0);
	}

	global_medialib_session_mutex = g_mutex_new ();

	/**
	 * this dummy just wants to put the default song in the playlist
	 */
	medialib->source_lock = g_mutex_new ();
	medialib->sources = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	session = xmms_medialib_begin_write ();
	sqlite3_exec (session->sql, "SELECT id, source FROM Sources",
	              add_to_source, medialib->sources, NULL);

	if (create) {
		xmms_error_t error;

		xmms_medialib_entry_new (session,
		                         "file://" SHAREDDIR
		                         "/mind.in.a.box-lament_snipplet.ogg",
		                         &error);
		/* A default playlist containing that song has been created
		 * with the mlib.
		 */
	}

	xmms_medialib_end (session);

	return medialib;
}

/** Session handling */

xmms_medialib_session_t *
_xmms_medialib_begin (gboolean write, const char *file, int line)
{
	xmms_medialib_session_t *session;

	{
		gchar *r;
		void *me = g_thread_self ();
		g_mutex_lock (xmms_medialib_debug_mutex);
		r = g_hash_table_lookup (xmms_medialib_debug_hash, me);
		if (r) {
			xmms_log_fatal ("Medialib session begun recursivly at %s:%d, after %s", file, line, r);
		} else {
			g_hash_table_insert (xmms_medialib_debug_hash, me,
			                     g_strdup_printf ("%s:%d", file, line));
		}
		g_mutex_unlock (xmms_medialib_debug_mutex);
	}
	if (global_medialib_session) {
		/** This will only happen when OLD_SQLITE_VERSION is set. */
		g_mutex_lock (global_medialib_session_mutex);
		return global_medialib_session;
	}

	session = xmms_medialib_session_new (file, line);
	xmms_object_ref (XMMS_OBJECT (medialib));
	session->write = write;

	if (write) {
		/* Start a exclusive transaction */
		if (!xmms_sqlite_exec (session->sql, "BEGIN EXCLUSIVE TRANSACTION")) {
			xmms_log_error ("transaction failed!");
		}
	}

	session->next_id = -1;

	return session;
}

void
xmms_medialib_end (xmms_medialib_session_t *session)
{
	g_return_if_fail (session);

	{
		void *me = g_thread_self ();
		g_mutex_lock (xmms_medialib_debug_mutex);
		g_hash_table_remove (xmms_medialib_debug_hash, me);
		g_mutex_unlock (xmms_medialib_debug_mutex);
	}

	if (session->write) {
		xmms_sqlite_exec (session->sql, "COMMIT");
	}

	if (session == global_medialib_session) {
		g_mutex_unlock (global_medialib_session_mutex);
		return;
	}

	xmms_sqlite_close (session->sql);
	xmms_object_unref (XMMS_OBJECT (session->medialib));
	g_free (session);
}

static int
xmms_medialib_string_cb (xmmsv_t **row, gpointer udata)
{
	gchar **str = udata;
	const gchar *buf;

	if (row && row[0] && xmmsv_get_type (row[0]) == XMMSV_TYPE_STRING) {
		xmmsv_get_string (row[0], &buf);
		*str = g_strdup (buf);
	} else
		XMMS_DBG ("Expected string but got something else!");

	return 0;
}

static int
xmms_medialib_value_cb (xmmsv_t **row, gpointer udata)
{
	xmmsv_t **ret = udata;

	*ret = xmmsv_ref (row[0]);

	return 0;
}

/**
 * Retrieve a property from an entry
 *
 * @see xmms_medialib_entry_property_get_str
 */

#define XMMS_MEDIALIB_RETRV_PROPERTY_SQL "SELECT IFNULL (intval, value) FROM Media WHERE key=%Q AND id=%d ORDER BY xmms_source_pref(source, %Q) LIMIT 1"

xmmsv_t *
xmms_medialib_entry_property_get_value (xmms_medialib_session_t *session,
                                        xmms_medialib_entry_t entry,
                                        const gchar *property)
{
	xmmsv_t *ret = NULL;

	g_return_val_if_fail (property, NULL);
	g_return_val_if_fail (session, NULL);

	if (!strcmp (property, XMMS_MEDIALIB_ENTRY_PROPERTY_ID)) {
		ret = xmmsv_new_int (entry);
	} else {
		xmms_sqlite_query_array (session->sql, xmms_medialib_value_cb,
		                         &ret, XMMS_MEDIALIB_RETRV_PROPERTY_SQL,
		                         property, entry, source_pref);
	}

	return ret;
}

/**
 * Retrieve a property from an entry.
 *
 * @param session The medialib session to be used for the transaction.
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
	                         XMMS_MEDIALIB_RETRV_PROPERTY_SQL,
	                         property, entry, source_pref);

	return ret;
}

/**
 * Retrieve a property as a int from a entry.
 *
 * @param session The medialib session to be used for the transaction.
 * @param entry Entry to query.
 * @param property The property to extract. Strings passed should
 * be defined in medialib.h
 *
 * @returns Property as integer, or -1 if it doesn't exist.
 */
gint
xmms_medialib_entry_property_get_int (xmms_medialib_session_t *session,
                                      xmms_medialib_entry_t entry,
                                      const gchar *property)
{
	gint32 ret = -1;

	g_return_val_if_fail (property, -1);
	g_return_val_if_fail (session, -1);

	xmms_sqlite_query_int (session->sql, &ret,
	                       XMMS_MEDIALIB_RETRV_PROPERTY_SQL,
	                       property, entry, source_pref);

	return ret;
}

/**
 * Set a entry property to a new value, overwriting the old value.
 *
 * @param session The medialib session to be used for the transaction.
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
	return xmms_medialib_entry_property_set_int_source (session, entry,
	                                                    property, value,
	                                                    XMMS_MEDIALIB_SOURCE_SERVER_ID);
}


gboolean
xmms_medialib_entry_property_set_int_source (xmms_medialib_session_t *session,
                                             xmms_medialib_entry_t entry,
                                             const gchar *property, gint value,
                                             guint32 source)
{
	gboolean ret;

	g_return_val_if_fail (property, FALSE);
	g_return_val_if_fail (session, FALSE);

	if (!xmms_medialib_check_id_in_session (entry, session)) {
		XMMS_DBG ("Trying to add property to id %d "
		          "that is not yet in the medialib. Denied.", entry);

		return FALSE;
	}

	ret = xmms_sqlite_exec (session->sql,
	                        "INSERT OR REPLACE INTO Media "
	                        "(id, value, intval, key, source) VALUES "
	                        "(%d, '%d', %d, %Q, %d)",
	                        entry, value, value, property, source);

	return ret;

}

/**
 * Set a entry property to a new value, overwriting the old value.
 *
 * @param session The medialib session to be used for the transaction.
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
	return xmms_medialib_entry_property_set_str_source (session, entry,
	                                                    property, value,
	                                                    XMMS_MEDIALIB_SOURCE_SERVER_ID);
}


gboolean
xmms_medialib_entry_property_set_str_source (xmms_medialib_session_t *session,
                                             xmms_medialib_entry_t entry,
                                             const gchar *property, const gchar *value,
                                             guint32 source)
{
	gboolean ret;

	g_return_val_if_fail (property, FALSE);
	g_return_val_if_fail (session, FALSE);

	if (value && !g_utf8_validate (value, -1, NULL)) {
		XMMS_DBG ("OOOOOPS! Trying to set property %s to a NON UTF-8 string (%s) I will deny that!", property, value);
		return FALSE;
	}

	if (!xmms_medialib_check_id_in_session (entry, session)) {
		XMMS_DBG ("Trying to add property to id %d "
		          "that is not yet in the medialib. Denied.", entry);

		return FALSE;
	}

	ret = xmms_sqlite_exec (session->sql,
	                        "INSERT OR REPLACE INTO Media "
	                        "(id, value, intval, key, source) VALUES "
	                        "(%d, %Q, NULL, %Q, %d)",
	                        entry, value, property, source);

	return ret;

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
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE,
	                    XMMSV_TYPE_INT32, entry);
}

/**
 * Trigger an added siginal to the client. This should be
 * called when a new entry has been added to the medialib
 *
 * @param entry Entry to signal an add for.
 */
void
xmms_medialib_entry_send_added (xmms_medialib_entry_t entry)
{
	xmms_object_emit_f (XMMS_OBJECT (medialib),
	                    XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED,
	                    XMMSV_TYPE_INT32, entry);
}

static void
xmms_medialib_entry_remove_method (xmms_medialib_t *medialib, guint32 entry, xmms_error_t *error)
{
	xmms_medialib_entry_remove (entry);
}

/**
 * Remove a medialib entry from the database
 *
 * @param session The medialib session to be used for the transaction.
 * @param entry Entry to remove
 */

void
xmms_medialib_entry_remove (xmms_medialib_entry_t entry)
{
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin_write ();
	xmms_sqlite_exec (session->sql, "DELETE FROM Media WHERE id=%d", entry);
	xmms_medialib_end (session);

	/** @todo safe ? */
	xmms_playlist_remove_by_entry (medialib->playlist, entry);
}

static xmms_medialib_entry_t xmms_medialib_entry_new_insert (xmms_medialib_session_t *session, guint32 id, const char *url, xmms_error_t *error);

static void
process_file (xmms_medialib_session_t *session,
              const gchar *playlist,
              gint32 pos,
              const gchar *path,
              xmms_error_t *error)
{
	xmms_medialib_entry_t entry;

	entry = xmms_medialib_entry_new_encoded (session, path, error);

	if (entry && playlist != NULL) {
		if (pos >= 0) {
			xmms_playlist_insert_entry (session->medialib->playlist,
			                            playlist, pos, entry, error);
		} else {
			xmms_playlist_add_entry (session->medialib->playlist,
			                         playlist, entry, error);
		}
	}
}

static gint
cmp_val (gconstpointer a, gconstpointer b)
{
	xmmsv_t *v1, *v2;
	const gchar *s1, *s2;
	v1 = (xmmsv_t *) a;
	v2 = (xmmsv_t *) b;
	if (xmmsv_get_type (v1) != XMMSV_TYPE_DICT)
		return 0;
	if (xmmsv_get_type (v2) != XMMSV_TYPE_DICT)
		return 0;

	xmmsv_dict_entry_get_string (v1, "path", &s1);
	xmmsv_dict_entry_get_string (v2, "path", &s2);

	return strcmp (s1, s2);
}

/* code ported over from CLI's "radd" command. */
static gboolean
process_dir (const gchar *directory,
             GList **ret,
             xmms_error_t *error)
{
	GList *list;

	list = xmms_xform_browse (NULL, directory, error);
	if (!list) {
		return FALSE;
	}

	list = g_list_sort (list, cmp_val);

	while (list) {
		xmmsv_t *val = list->data;
		const gchar *str;
		gint isdir;

		xmmsv_dict_entry_get_string (val, "path", &str);
		xmmsv_dict_entry_get_int (val, "isdir", &isdir);

		if (isdir == 1) {
			process_dir (str, ret, error);
		} else {
			*ret = g_list_prepend (*ret, g_strdup (str));
		}

		xmmsv_unref (val);
		list = g_list_delete_link (list, list);
	}

	return TRUE;
}

void
xmms_medialib_entry_cleanup (xmms_medialib_session_t *session,
                             xmms_medialib_entry_t entry)
{
	xmms_sqlite_exec (session->sql,
	                  "DELETE FROM Media WHERE id=%d AND source=%d "
	                  "AND key NOT IN (%Q, %Q, %Q, %Q, %Q)",
	                  entry,
	                  XMMS_MEDIALIB_SOURCE_SERVER_ID,
	                  XMMS_MEDIALIB_ENTRY_PROPERTY_URL,
	                  XMMS_MEDIALIB_ENTRY_PROPERTY_ADDED,
	                  XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
	                  XMMS_MEDIALIB_ENTRY_PROPERTY_LMOD,
	                  XMMS_MEDIALIB_ENTRY_PROPERTY_LASTSTARTED);

	xmms_sqlite_exec (session->sql,
	                  "DELETE FROM Media WHERE id=%d AND source IN "
	                  "(SELECT id FROM Sources WHERE source LIKE 'plugin/%%' "
	                   "AND source != 'plugin/playlist')",
	                  entry);

}

static void
xmms_medialib_rehash (xmms_medialib_t *medialib, guint32 id, xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;
	xmms_medialib_session_t *session;

	session = xmms_medialib_begin_write ();

	if (id) {
		xmms_sqlite_exec (session->sql,
		                  "UPDATE Media SET value = '%d', intval = %d "
		                  "WHERE key='%s' AND id=%d",
		                  XMMS_MEDIALIB_ENTRY_STATUS_REHASH,
		                  XMMS_MEDIALIB_ENTRY_STATUS_REHASH,
		                  XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS, id);
	} else {
		xmms_sqlite_exec (session->sql,
		                  "UPDATE Media SET value = '%d', intval = %d "
		                  "WHERE key='%s'",
		                  XMMS_MEDIALIB_ENTRY_STATUS_REHASH,
		                  XMMS_MEDIALIB_ENTRY_STATUS_REHASH,
		                  XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS);
	}

	xmms_medialib_end (session);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);

}

/* Recursively add entries under the given path to the medialib,
 * optionally adding them to a playlist if the playlist argument is
 * not NULL.
 */
void
xmms_medialib_add_recursive (xmms_medialib_t *medialib, const gchar *playlist,
                             const gchar *path, xmms_error_t *error)
{
	/* Just called insert with negative pos to append */
	xmms_medialib_insert_recursive (medialib, playlist, -1, path, error);
}

/* Recursively adding entries under the given path to the medialib,
 * optionally insert them into a playlist at a given position if the
 * playlist argument is not NULL. If the position is negative, entries
 * are appended to the playlist.
 */
void
xmms_medialib_insert_recursive (xmms_medialib_t *medialib, const gchar *playlist,
                                gint32 pos, const gchar *path,
                                xmms_error_t *error)
{
	xmms_medialib_session_t *session;
	GList *first, *list = NULL, *n;

	g_return_if_fail (medialib);
	g_return_if_fail (path);

	/* Allocate our first list node manually here. The following call
	 * to process_dir() will prepend all other nodes, so afterwards
	 * "first" will point to the last node of the list... see below.
	 */
	first = list = g_list_alloc ();

	process_dir (path, &list, error);

	XMMS_DBG ("taking the transaction!");
	session = xmms_medialib_begin_write ();

	/* We now want to iterate the list in the order in which the nodes
	 * were added, ie in reverse order. Thankfully we stored a pointer
	 * to the last node in the list before, which saves us an expensive
	 * g_list_last() call now.
	 */
	for (n = first->prev; n; n = g_list_previous (n)) {
		process_file (session, playlist, pos, n->data, error);
		g_free (n->data);
	}

	g_list_free (list);

	XMMS_DBG ("and we are done!");
	xmms_medialib_end (session);
}

static void
xmms_medialib_path_import (xmms_medialib_t *medialib, const gchar *path,
                           xmms_error_t *error)
{
	xmms_medialib_add_recursive (medialib, NULL, path, error);
}

static xmms_medialib_entry_t
xmms_medialib_entry_new_insert (xmms_medialib_session_t *session,
                                guint32 id,
                                const char *url,
                                xmms_error_t *error)
{
	xmms_mediainfo_reader_t *mr;
	guint source;

	source = XMMS_MEDIALIB_SOURCE_SERVER_ID;

	if (!xmms_sqlite_exec (session->sql,
	                       "INSERT INTO Media (id, key, value, source) VALUES "
	                                         "(%d, '%s', %Q, %d)",
	                       id, XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url,
	                       source)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Sql error/corruption inserting url");
		return 0;
	}

	xmms_medialib_entry_status_set (session, id, XMMS_MEDIALIB_ENTRY_STATUS_NEW);
	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_reader_wakeup (mr);

	return 1;

}

/**
 * @internal
 */
xmms_medialib_entry_t
xmms_medialib_entry_new_encoded (xmms_medialib_session_t *session,
                                 const char *url, xmms_error_t *error)
{
	gint id = 0;
	guint ret = 0;
	guint source;

	g_return_val_if_fail (url, 0);
	g_return_val_if_fail (session, 0);
	g_return_val_if_fail (session->write, 0);

	source = XMMS_MEDIALIB_SOURCE_SERVER_ID;

	xmms_sqlite_query_int (session->sql, &id,
	                       "SELECT id AS value FROM Media "
	                       "WHERE key='%s' AND value=%Q AND source=%d",
	                       XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url,
	                       source);

	if (id) {
		ret = id;
	} else {
		if (session->next_id <= 0 &&
		    !xmms_sqlite_query_int (session->sql, &session->next_id,
		                            "SELECT IFNULL(MAX (id),0)+1 FROM Media")) {
			xmms_error_set (error, XMMS_ERROR_GENERIC,
			                "SQL error/corruption selecting max(id)");
			return 0;
		}

		ret = session->next_id++;

		if (!xmms_medialib_entry_new_insert (session, ret, url, error))
			return 0;
	}

	xmms_medialib_entry_send_added (ret);
	return ret;

}

/**
 * Welcome to a function that should be called something else.
 * Returns a entry for a URL, if the URL is already in the medialib
 * the current entry will be returned otherwise a new one will be
 * created and returned.
 *
 * @todo rename to something better?
 *
 * @param session The medialib session to be used for the transaction.
 * @param url URL to add/retrieve from the medialib
 * @param error If an error occurs, it will be stored in there.
 *
 * @returns Entry mapped to the URL
 */
xmms_medialib_entry_t
xmms_medialib_entry_new (xmms_medialib_session_t *session, const char *url, xmms_error_t *error)
{
	gchar *enc_url;
	xmms_medialib_entry_t res;

	enc_url = xmms_medialib_url_encode (url);
	if (!enc_url)
		return 0;

	res = xmms_medialib_entry_new_encoded (session, enc_url, error);

	g_free (enc_url);

	return res;
}

static gint32
xmms_medialib_entry_get_id (xmms_medialib_t *medialib, const gchar *url,
                            xmms_error_t *error)
{
	gint32 id = 0;
	xmms_medialib_session_t *session = xmms_medialib_begin ();

	xmms_sqlite_query_int (session->sql, &id,
	                       "SELECT id AS value FROM Media WHERE key='%s' AND value=%Q AND source=%d",
	                       XMMS_MEDIALIB_ENTRY_PROPERTY_URL, url,
	                       XMMS_MEDIALIB_SOURCE_SERVER_ID);
	xmms_medialib_end (session);

	return id;
}

static void
xmms_medialib_tree_add_tuple (GTree *tree, const char *key,
                              const char *source, xmmsv_t *value)
{
	xmmsv_t *keytreeval;

	/* Find (or insert) subtree matching the prop key */
	keytreeval = (xmmsv_t *) g_tree_lookup (tree, key);
	if (!keytreeval) {
		keytreeval = xmmsv_new_dict ();
		g_tree_insert (tree, g_strdup (key), keytreeval);
	}

	/* Replace (or insert) value matching the prop source */
	xmmsv_dict_set (keytreeval, source, value);
}

static gboolean
xmms_medialib_list_cb (xmmsv_t **row, gpointer udata)
{
	GList **list = (GList**)udata;

	/* Source */
	*list = g_list_prepend (*list, xmmsv_ref (row[0]));

	/* Key */
	*list = g_list_prepend (*list, xmmsv_ref (row[1]));

	/* Value */
	*list = g_list_prepend (*list, xmmsv_ref (row[2]));

	return TRUE;
}

static gboolean
xmms_medialib_tree_cb (xmmsv_t **row, gpointer udata)
{
	const char *key, *source;
	xmmsv_t *value;
	GTree **tree = (GTree**)udata;

	xmmsv_get_string (row[0], &source);
	xmmsv_get_string (row[1], &key);
	value = row[2];

	xmms_medialib_tree_add_tuple (*tree, key, source, value);

	return TRUE;
}

/**
 * Convert a entry and all properties to a hashtable that
 * could be feed to the client or somewhere else in the daemon.
 *
 * @param session The medialib session to be used for the transaction.
 * @param entry Entry to convert.
 *
 * @returns Newly allocated hashtable with newly allocated strings
 * make sure to free them all.
 */

GList *
xmms_medialib_entry_to_list (xmms_medialib_session_t *session, xmms_medialib_entry_t entry)
{
	GList *ret = NULL;
	gboolean s;

	g_return_val_if_fail (session, NULL);
	g_return_val_if_fail (entry, NULL);

	s = xmms_sqlite_query_array (session->sql, xmms_medialib_list_cb, &ret,
	                             "SELECT s.source, m.key, "
	                                    "IFNULL (m.intval, m.value) "
	                             "FROM Media m LEFT JOIN "
	                             "Sources s ON m.source = s.id "
	                             "WHERE m.id=%d",
	                             entry);
	if (!s || !ret) {
		return NULL;
	}

	/* Source */
	ret = g_list_prepend (ret, xmmsv_new_string ("server"));

	/* Key */
	ret = g_list_prepend (ret, xmmsv_new_string ("id"));

	/* Value */
	ret = g_list_prepend (ret, xmmsv_new_int (entry));

	return g_list_reverse (ret);
}

/**
 * Convert a entry and all properties to a key-source-value tree that
 * could be feed to the client or somewhere else in the daemon.
 *
 * @param session The medialib session to be used for the transaction.
 * @param entry Entry to convert.
 *
 * @returns Newly allocated tree with newly allocated strings
 * make sure to free them all.
 */

static GTree *
xmms_medialib_entry_to_tree (xmms_medialib_session_t *session, xmms_medialib_entry_t entry)
{
	GTree *ret;
	xmmsv_t *v_entry;
	gboolean s;

	g_return_val_if_fail (session, NULL);
	g_return_val_if_fail (entry, NULL);

	if (!xmms_medialib_check_id_in_session (entry, session)) {
		return NULL;
	}

	ret = g_tree_new_full ((GCompareDataFunc) strcmp, NULL, g_free,
	                       (GDestroyNotify) xmmsv_unref);

	s = xmms_sqlite_query_array (session->sql, xmms_medialib_tree_cb,
	                             &ret,
	                             "SELECT s.source, m.key, "
	                                     "IFNULL (m.intval, m.value) "
	                             "FROM Media m LEFT JOIN "
	                             "Sources s ON m.source = s.id "
	                             "WHERE m.id=%d",
	                             entry);
	if (!s || !ret) {
		return NULL;
	}

	v_entry = xmmsv_new_int (entry);
	xmms_medialib_tree_add_tuple (ret, "id", "server", v_entry);
	xmmsv_unref (v_entry);

	return ret;
}

/* Legacy, still used by collections. */
GList *
xmms_medialib_info_list (xmms_medialib_t *medialib, guint32 id, xmms_error_t *err)
{
	xmms_medialib_session_t *session;
	GList *ret = NULL;

	if (!id) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such entry, 0");
	} else {
		session = xmms_medialib_begin ();
		ret = xmms_medialib_entry_to_list (session, id);
		xmms_medialib_end (session);

		if (!ret) {
			xmms_error_set (err, XMMS_ERROR_NOENT,
			                "Could not retrieve info for that entry!");
		}
	}

	return ret;
}

GTree *
xmms_medialib_info (xmms_medialib_t *medialib, guint32 id, xmms_error_t *err)
{
	xmms_medialib_session_t *session;
	GTree *ret = NULL;

	if (!id) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "No such entry, 0");
	} else {
		session = xmms_medialib_begin ();
		ret = xmms_medialib_entry_to_tree (session, id);
		xmms_medialib_end (session);

		if (!ret) {
			xmms_error_set (err, XMMS_ERROR_NOENT,
			                "Could not retrieve info for that entry!");
		}
	}

	return ret;
}

static gboolean
select_callback (xmmsv_t *row, gpointer udata)
{
	GList **l = (GList **) udata;
	*l = g_list_prepend (*l, row);
	return TRUE;
}

/**
 * Add a entry to the medialib. Calls #xmms_medialib_entry_new and then
 * wakes up the mediainfo_reader in order to resolve the metadata.
 *
 * @param medialib Medialib pointer
 * @param url URL to add
 * @param error In case of error this will be filled.
 */

static void
xmms_medialib_add_entry (xmms_medialib_t *medialib, const gchar *url,
                         xmms_error_t *error)
{
	xmms_medialib_entry_t entry;
	xmms_medialib_session_t *session;

	g_return_if_fail (medialib);
	g_return_if_fail (url);

	session = xmms_medialib_begin_write ();

	entry = xmms_medialib_entry_new_encoded (session, url, error);

	xmms_medialib_end (session);
}

/**
 * Changes the URL of an entry in the medialib.
 *
 * @param medialib Medialib pointer
 * @param entry entry to modify
 * @param url URL to change to
 * @param error In case of error this will be filled.
 */
static void
xmms_medialib_move_entry (xmms_medialib_t *medialib, guint32 entry,
                          const gchar *url, xmms_error_t *error)
{
	const gchar *key = XMMS_MEDIALIB_ENTRY_PROPERTY_URL;
	guint32 sourceid = XMMS_MEDIALIB_SOURCE_SERVER_ID;
	gchar *enc_url;

	xmms_medialib_session_t *session;

	enc_url = xmms_medialib_url_encode (url);

	session = xmms_medialib_begin_write ();
	xmms_medialib_entry_property_set_str_source (session, entry, key, enc_url,
	                                             sourceid);
	xmms_medialib_end (session);

	g_free (enc_url);

	xmms_medialib_entry_send_update (entry);
}

static void
xmms_medialib_property_set_str_method (xmms_medialib_t *medialib, guint32 entry,
                                       const gchar *source, const gchar *key,
                                       const gchar *value, xmms_error_t *error)
{
	guint32 sourceid;
	xmms_medialib_session_t *session;

	if (g_ascii_strcasecmp (source, "server") == 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Can't write to source server!");
		return;
	}

	session = xmms_medialib_begin_write ();
	sourceid = xmms_medialib_source_to_id (session, source);

	xmms_medialib_entry_property_set_str_source (session, entry, key, value,
	                                             sourceid);
	xmms_medialib_end (session);

	xmms_medialib_entry_send_update (entry);
}

static void
xmms_medialib_property_set_int_method (xmms_medialib_t *medialib, guint32 entry,
                                       const gchar *source, const gchar *key,
                                       gint32 value, xmms_error_t *error)
{
	guint32 sourceid;
	xmms_medialib_session_t *session;

	if (g_ascii_strcasecmp (source, "server") == 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Can't write to source server!");
		return;
	}

	session = xmms_medialib_begin_write ();
	sourceid = xmms_medialib_source_to_id (session, source);
	xmms_medialib_entry_property_set_int_source (session, entry, key, value,
	                                             sourceid);
	xmms_medialib_end (session);

	xmms_medialib_entry_send_update (entry);
}

void
xmms_medialib_property_remove (xmms_medialib_t *medialib, guint32 entry,
                               const gchar *source, const gchar *key,
                               xmms_error_t *error)
{
	guint32 sourceid;

	xmms_medialib_session_t *session = xmms_medialib_begin_write ();
	sourceid = xmms_medialib_source_to_id (session, source);
	xmms_sqlite_exec (session->sql,
	                  "DELETE FROM Media WHERE source=%d AND key='%s' AND "
	                                          "id=%d",
	                  sourceid, key, entry);
	xmms_medialib_end (session);

	xmms_medialib_entry_send_update (entry);
}

static void
xmms_medialib_property_remove_method (xmms_medialib_t *medialib, guint32 entry,
                                      const gchar *source, const gchar *key,
                                      xmms_error_t *error)
{
	if (g_ascii_strcasecmp (source, "server") == 0) {
		xmms_error_set (error, XMMS_ERROR_GENERIC,
		                "Can't remove properties set by the server!");
		return;
	}

	return xmms_medialib_property_remove (medialib, entry, source, key, error);
}

/**
 * Get a list of GHashTables 's that matches the query.
 *
 * @param session The medialib session to be used for the transaction.
 * @param query SQL query that should be executed.
 * @param error In case of error this will be filled.
 * @returns GList containing GHashTables. Caller are responsible to
 * free all memory.
 */
GList *
xmms_medialib_select (xmms_medialib_session_t *session,
                      const gchar *query, xmms_error_t *error)
{
	GList *res = NULL;
	gint ret;

	g_return_val_if_fail (query, 0);
	g_return_val_if_fail (session, 0);

	ret = xmms_sqlite_query_table (session->sql, select_callback,
	                               (void *)&res, error, "%s", query);

	return ret ? g_list_reverse (res) : NULL;
}

/** @} */

/**
 * @internal
 */

gboolean
xmms_medialib_check_id (xmms_medialib_entry_t entry)
{
	xmms_medialib_session_t *session;
	gboolean ret;

	session = xmms_medialib_begin ();
	ret = xmms_medialib_check_id_in_session (entry, session);
	xmms_medialib_end (session);

	return ret;
}

/**
 * @internal
 */

static gboolean
xmms_medialib_check_id_in_session (xmms_medialib_entry_t entry,
                                   xmms_medialib_session_t *session)
{
	gint c = 0;

	if (!xmms_sqlite_query_int (session->sql, &c,
	                            "SELECT COUNT(id) FROM Media WHERE id=%d",
	                            entry)) {
		return FALSE;
	}

	return c > 0;
}


/**
 * @internal
 * Get the next unresolved entry. Used by the mediainfo reader..
 */

xmms_medialib_entry_t
xmms_medialib_entry_not_resolved_get (xmms_medialib_session_t *session)
{
	gint32 ret = 0;

	g_return_val_if_fail (session, 0);

	xmms_sqlite_query_int (session->sql, &ret,
	                       "SELECT id FROM Media WHERE key='%s' "
	                       "AND source=%d AND intval IN (%d, %d) LIMIT 1",
	                       XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
	                       XMMS_MEDIALIB_SOURCE_SERVER_ID,
	                       XMMS_MEDIALIB_ENTRY_STATUS_NEW,
	                       XMMS_MEDIALIB_ENTRY_STATUS_REHASH);

	return ret;
}

guint
xmms_medialib_num_not_resolved (xmms_medialib_session_t *session)
{
	gint ret;
	g_return_val_if_fail (session, 0);

	xmms_sqlite_query_int (session->sql, &ret,
	                       "SELECT COUNT(id) AS value FROM Media WHERE "
	                       "key='%s' AND intval IN (%d, %d) AND source=%d",
	                       XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
	                       XMMS_MEDIALIB_ENTRY_STATUS_NEW,
	                       XMMS_MEDIALIB_ENTRY_STATUS_REHASH,
	                       XMMS_MEDIALIB_SOURCE_SERVER_ID);

	return ret;
}

gboolean
xmms_medialib_decode_url (char *url)
{
	int i = 0, j = 0;

	g_return_val_if_fail (url, TRUE);

	while (url[i]) {
		unsigned char chr = url[i++];

		if (chr == '+') {
			url[j++] = ' ';
		} else if (chr == '%') {
			char ts[3];
			char *t;

			ts[0] = url[i++];
			if (!ts[0])
				return FALSE;
			ts[1] = url[i++];
			if (!ts[1])
				return FALSE;
			ts[2] = '\0';

			url[j++] = strtoul (ts, &t, 16);
			if (t != &ts[2])
				return FALSE;
		} else {
			url[j++] = chr;
		}
	}

	url[j] = '\0';

	return TRUE;
}


#define GOODCHAR(a) ((((a) >= 'a') && ((a) <= 'z')) || \
                     (((a) >= 'A') && ((a) <= 'Z')) || \
                     (((a) >= '0') && ((a) <= '9')) || \
                     ((a) == ':') || \
                     ((a) == '/') || \
                     ((a) == '-') || \
                     ((a) == '.') || \
                     ((a) == '_'))

/* we don't share code here with medialib because we want to use g_malloc :( */
gchar *
xmms_medialib_url_encode (const gchar *path)
{
	static gchar hex[16] = "0123456789abcdef";
	gchar *res;
	int i = 0, j = 0;

	res = g_malloc (strlen (path) * 3 + 1);
	if (!res)
		return NULL;

	while (path[i]) {
		guchar chr = path[i++];
		if (GOODCHAR (chr)) {
			res[j++] = chr;
		} else if (chr == ' ') {
			res[j++] = '+';
		} else {
			res[j++] = '%';
			res[j++] = hex[((chr & 0xf0) >> 4)];
			res[j++] = hex[(chr & 0x0f)];
		}
	}

	res[j] = '\0';

	return res;
}
