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

#include "xmms/util.h"
#include "xmms/playlist.h"
#include "xmms/config.h"
#include "xmms/medialib.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/plugin.h"
#include "xmms/signal_xmms.h"
#include "xmms/ipc.h"
#include "internal/plugin_int.h"
#include "internal/decoder_int.h"
#include "internal/output_int.h"
#include "internal/transport_int.h"

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <time.h>

#include <sqlite.h>

struct xmms_medialib_St {
	xmms_object_t object;
	xmms_playlist_t *playlist;
	struct sqlite *sql;
	GMutex *mutex;
	guint32 nextid;
};


/** 
  * Ok, so the functions are written with reentrency in mind, but
  * we choose to have a global medialib object here. It will be
  * much easier, and I don't see the real use of multiple medialibs
  * right now. This could be changed by removing this global one
  * and altering the function callers...
  */

static xmms_medialib_t *medialib;

static GHashTable *xmms_medialib_info (xmms_medialib_t *playlist, guint32 id, xmms_error_t *err);
static void xmms_medialib_select_and_add (xmms_medialib_t *medialib, gchar *query, xmms_error_t *error);
void xmms_medialib_add_entry (xmms_medialib_t *, gchar *, xmms_error_t *);
static GList *xmms_medialib_select_method (xmms_medialib_t *, gchar *, xmms_error_t *);
GList *xmms_medialib_select (gchar *query, xmms_error_t *error);
static void xmms_medialib_playlist_save_current (xmms_medialib_t *, gchar *, xmms_error_t *);
static void xmms_medialib_playlist_load (xmms_medialib_t *, gchar *, xmms_error_t *);

/** Methods */
XMMS_CMD_DEFINE (info, xmms_medialib_info, xmms_medialib_t *, HASHTABLE, UINT32, NONE);
XMMS_CMD_DEFINE (select, xmms_medialib_select_method, xmms_medialib_t *, HASHLIST, STRING, NONE);
XMMS_CMD_DEFINE (mlib_add, xmms_medialib_add_entry, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (playlist_save_current, xmms_medialib_playlist_save_current, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (playlist_load, xmms_medialib_playlist_load, xmms_medialib_t *, NONE, STRING, NONE);
XMMS_CMD_DEFINE (addtopls, xmms_medialib_select_and_add, xmms_medialib_t *, NONE, STRING, NONE);


static void 
xmms_medialib_destroy (xmms_object_t *object)
{
	xmms_medialib_t *mlib = (xmms_medialib_t *)object;

	g_mutex_free (mlib->mutex);
	xmms_sqlite_close (mlib->sql);

	xmms_ipc_broadcast_unregister (XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE);
	xmms_ipc_object_unregister (XMMS_IPC_OBJECT_OUTPUT);
}
 
gboolean
xmms_medialib_init (xmms_playlist_t *playlist)
{
	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);
	medialib->sql = xmms_sqlite_open (&medialib->nextid);
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
	                     XMMS_IPC_CMD_PLAYLIST_SAVE_CURRENT,
	                     XMMS_CMD_FUNC (playlist_save_current));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_PLAYLIST_LOAD,
	                     XMMS_CMD_FUNC (playlist_load));
	xmms_object_cmd_add (XMMS_OBJECT (medialib),
	                     XMMS_IPC_CMD_ADD_TO_PLAYLIST,
	                     XMMS_CMD_FUNC (addtopls));

	xmms_config_value_register ("medialib.dologging",
				    "1",
				    NULL, NULL);
	
	return TRUE;
}

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
		snprintf (tmp, 16, "%u", (guint) starttime);
		xmms_medialib_entry_property_set (entry, "laststarted", tmp);
	}
}


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

gboolean
xmms_medialib_entry_is_resolved (xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_RESOLVED);
}

/** Just a convient function wrapper */
guint
xmms_medialib_entry_id_get (xmms_medialib_entry_t entry)
{
	return xmms_medialib_entry_property_get_int (entry, XMMS_MEDIALIB_ENTRY_PROPERTY_ID);
}

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
				query)) {
		xmms_error_set (error, XMMS_ERROR_GENERIC, "Query failed!");
		g_mutex_unlock (medialib->mutex);
		return;
	}

	g_mutex_unlock (medialib->mutex);

}

xmms_medialib_entry_t
xmms_medialib_entry_new_unlocked (const char *url)
{
	guint id = 0;
	guint ret;

	g_return_val_if_fail (url, 0);

	xmms_sqlite_query (medialib->sql, xmms_medialib_int_cb, &id, 
			   "select id from Media where key='url' and value=%Q", url);

	if (id) {
		ret = id;
	} else {
		ret = ++medialib->nextid;
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert into Media (id, key, value) values (%d, 'url', %Q)",
				       ret, url)) {
			ret = 0;
		}
		if (!xmms_sqlite_query (medialib->sql, NULL, NULL,
				       "insert or replace into Media (id, key, value) values (%d, 'resolved', '0')",
				       ret)) {
			ret = 0;
		}

	}

	return ret;

}

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

void
xmms_medialib_add_entry (xmms_medialib_t *medialib, gchar *url, xmms_error_t *error)
{
	xmms_medialib_entry_t entry;
	xmms_mediainfo_reader_t *mr;

	g_return_if_fail (medialib);
	g_return_if_fail (url);

	XMMS_DBG ("adding %s to the mediainfo db!", url);

	entry = xmms_medialib_entry_new (url);

	mr = xmms_playlist_mediainfo_reader_get (medialib->playlist);
	xmms_mediainfo_entry_add (mr, entry);
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

static void
xmms_medialib_playlist_save_current (xmms_medialib_t *medialib,
                                     gchar *name, xmms_error_t *error)
{
	GList *entries, *l;
	gint ret;
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

	/* finally, add the playlist entries */
	entries = xmms_playlist_list (medialib->playlist, NULL);

	for (l = entries; l; l = g_list_next (l)) {
		xmms_medialib_entry_t entry = (xmms_medialib_entry_t) l->data;
		gchar mid[32];

		if (!entry) {
			g_mutex_unlock (medialib->mutex);

			return;
		}

		g_snprintf (mid, sizeof (mid), "mlib://%d", entry);

		ret = xmms_sqlite_query (medialib->sql, NULL, NULL,
		                         "insert into PlaylistEntries"
		                         "(playlist_id, entry) "
		                         "values (%u, %Q)",
		                         playlist_id, mid);

		if (!ret) {
			gchar buf[64];

			g_snprintf (buf, sizeof (buf),
			            "Couldn't add entry %s to playlist %u",
			            mid, playlist_id);
			xmms_error_set (error, XMMS_ERROR_GENERIC, buf);
			g_mutex_unlock (medialib->mutex);

			return;
		}

	}

	g_mutex_unlock (medialib->mutex);
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
	                         "where playlist_id = %u", playlist_id);
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
			e = xmms_medialib_entry_new_unlocked (entry);
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
 * Make sure that query are correctly escaped before entering here!
 */
GList *
xmms_medialib_select (gchar *query, xmms_error_t *error)
{
	GList *res = NULL;
	gint ret;

	g_return_val_if_fail (query, 0);

	g_mutex_lock (medialib->mutex);

	ret = xmms_sqlite_query (medialib->sql, select_callback, (void *)&res, query);

	g_mutex_unlock (medialib->mutex);

	if (!ret)
		return NULL;

	res = g_list_reverse (res);
	return res;
}

void
xmms_medialib_playlist_save_autosaved ()
{
	xmms_error_t err;

	xmms_medialib_playlist_save_current (medialib, "autosaved", &err);
}

void
xmms_medialib_playlist_load_autosaved ()
{
	xmms_error_t err;

	xmms_medialib_playlist_load (medialib, "autosaved", &err);
}
