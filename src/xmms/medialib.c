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




/** @file
 *  This controls the medialib.
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

#ifdef HAVE_SQLITE
#include <sqlite.h>
#endif

/** @defgroup MediaLibrary MediaLibrary
  * @ingroup XMMSServer
  * @brief The library.
  *
  * The medialibrary is responsible for holding information about your whole
  * music collection. 
  * @{
  */

struct xmms_medialib_St {
	xmms_object_t object;

	GMutex *mutex;
	guint id;

	xmms_playlist_t *playlist;

#ifdef HAVE_SQLITE
	sqlite *sql;
#endif
};


/** Global medialib */
static xmms_medialib_t *medialib;
  
static GList *xmms_medialib_select_method (xmms_medialib_t *, gchar *, xmms_error_t *);
XMMS_CMD_DEFINE (select, xmms_medialib_select_method, xmms_medialib_t *, HASHLIST, STRING, NONE);
void xmms_medialib_add_entry (xmms_medialib_t *, gchar *, xmms_error_t *);
XMMS_CMD_DEFINE (mlib_add, xmms_medialib_add_entry, xmms_medialib_t *, NONE, STRING, NONE);

static void xmms_medialib_playlist_save_current (xmms_medialib_t *, gchar *, xmms_error_t *);
XMMS_CMD_DEFINE (playlist_save_current, xmms_medialib_playlist_save_current, xmms_medialib_t *, NONE, STRING, NONE);
static void xmms_medialib_playlist_load (xmms_medialib_t *, gchar *, xmms_error_t *);
XMMS_CMD_DEFINE (playlist_load, xmms_medialib_playlist_load, xmms_medialib_t *, NONE, STRING, NONE);

static void
xmms_medialib_destroy (xmms_object_t *medialib)
{
	xmms_medialib_t *m = (xmms_medialib_t*) medialib;

	g_mutex_free (m->mutex);

	xmms_object_unref (m->playlist);

#ifdef HAVE_SQLITE
	if (m->sql) 
		xmms_sqlite_close (m);
#endif
}

/** Initialize the medialib */
gboolean
xmms_medialib_init ()
{

	medialib = xmms_object_new (xmms_medialib_t, xmms_medialib_destroy);


#ifdef HAVE_SQLITE
	if (!xmms_sqlite_open ()) {
		xmms_object_unref (medialib);
		medialib = NULL;
		return FALSE;
	}
#endif /*HAVE_SQLITE*/
	medialib->mutex = g_mutex_new ();

	xmms_config_value_register ("medialib.dologging",
				    "1",
				    NULL, NULL);

	xmms_ipc_object_register (XMMS_IPC_OBJECT_MEDIALIB, XMMS_OBJECT (medialib));

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

	return TRUE;
}

void
xmms_medialib_playlist_set (xmms_playlist_t *playlist)
{
	g_mutex_lock (medialib->mutex);

	xmms_object_ref (playlist);
	xmms_object_unref (medialib->playlist);
	medialib->playlist = playlist;

	g_mutex_unlock (medialib->mutex);
}

static void
statuschange (xmms_object_t *object, gconstpointer data, gpointer userdata)
{
	const gchar *mid;
	gboolean ret;
	xmms_playlist_entry_t *entry;
	xmms_config_value_t *cv;
	guint32 status = ((xmms_object_cmd_arg_t *) data)->retval.uint32;

	cv = xmms_config_lookup ("medialib.dologging");
	g_return_if_fail (cv);
	
	if (!xmms_config_value_int_get (cv))
		return;

	entry = xmms_output_playing_entry_get ((xmms_output_t *)object, NULL);
	if (!entry)
		return;

	mid = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID);
	if (!mid) {
		xmms_object_unref (entry);
		return;
	}

	if (status == XMMS_OUTPUT_STATUS_STOP) {
		const gchar *tmp, *sek;
		gint value = 0.0;

		tmp = xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_DURATION);
		if (tmp) {
			value = (gint) (100.0 * xmms_output_playtime ((xmms_output_t *)object, NULL) / (gdouble)atoi (tmp));
		}
		
		XMMS_DBG ("ENTRY STOPPED value = %d", value);

		sek = xmms_playlist_entry_property_get (entry, "laststarted");
		if (!sek)
			xmms_object_unref (entry);
		g_return_if_fail (sek);

		g_mutex_lock (medialib->mutex);
		ret = xmms_sqlite_query (NULL, NULL, 
					 "update Log set value=%d where id=%s and starttime=%s", 
					 value, mid, sek);
		
		g_mutex_unlock (medialib->mutex);
	} else if (status == XMMS_OUTPUT_STATUS_PLAY) {
		char tmp[16];
		time_t stime = time (NULL);
		
		g_mutex_lock (medialib->mutex);
		ret = xmms_sqlite_query (NULL, NULL, 
					 "insert into Log (id, starttime) values (%s, %u)", 
					 mid, (guint)stime);

		g_mutex_unlock (medialib->mutex);

		if (ret) {
			snprintf (tmp, 16, "%u", (guint)stime);
			xmms_playlist_entry_property_set (entry, "laststarted", tmp);
		}
	}

	xmms_object_unref (entry);
}


void
xmms_medialib_output_register (xmms_output_t *output)
{
	xmms_object_connect (XMMS_OBJECT (output),
			     XMMS_IPC_SIGNAL_OUTPUT_STATUS,
			     (xmms_object_handler_t) statuschange, NULL);
}


static guint
xmms_medialib_next_id (xmms_medialib_t *medialib)
{
	guint id=0;
	g_return_val_if_fail (medialib, 0);

	id = medialib->id ++;

	return id;
}



static gchar *_e[] = { 	
	"id", "url",
	XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST,
	XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM,
	XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE,
	XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD,
	NULL
};

static void
insert_foreach (gpointer key, gpointer value, gpointer userdata)
{
	gchar *k = key;
	gchar *v = value;
	xmms_medialib_t *medialib = userdata;
	gint i = 0;

	while (_e[i]) {
		if (g_strcasecmp (_e[i], k) == 0) {
		    return;
		}
		i++;
	}

	xmms_sqlite_query (NULL, NULL, "insert into Property (id, key, value) values (%d, %Q, %Q)", medialib->id - 1, k, v);
}

/** 
  * Takes an #xmms_playlist_entry_t and stores it
  * to the library
  *
  * @todo what if the entry already exists?
  */

gboolean
xmms_medialib_entry_store (xmms_playlist_entry_t *entry)
{
	gboolean ret;
	guint id;
	gchar *tmp;

	g_return_val_if_fail (medialib, FALSE);
	XMMS_DBG ("Storing entry to medialib!");
	g_mutex_lock (medialib->mutex);

	id = xmms_medialib_next_id (medialib);
	ret = xmms_sqlite_query (NULL, NULL,
				 "insert into Media values (%d, %Q, %Q, %Q, %Q, %Q, %Q, %d)",
				 id, xmms_playlist_entry_url_get (entry),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD),
				 time (NULL));

	if (!ret) {
		return FALSE;
	}

	xmms_playlist_entry_property_foreach (entry, insert_foreach, medialib);

	tmp = g_strdup_printf ("%d", id);
	xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID, tmp);
	g_free (tmp);
				 
	
	g_mutex_unlock (medialib->mutex);

	return TRUE;
}

static int
select_callback (void *pArg, int argc, char **argv, char **cName)
{
	gint i=0;
	GList **l = (GList **) pArg;
	GHashTable *table = g_hash_table_new (g_str_hash, g_str_equal);

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
	xmms_playlist_entry_t *entry;
	xmms_mediainfo_thread_t *mt;

	g_return_if_fail (medialib);
	g_return_if_fail (url);

	XMMS_DBG ("adding %s to the mediainfo db!", url);

	entry = xmms_playlist_entry_new (url);

	mt = xmms_playlist_mediainfo_thread_get (medialib->playlist);
	xmms_mediainfo_entry_add (mt, entry);

	xmms_object_unref (XMMS_OBJECT (entry));
}

static int
get_playlist_id_cb (void *pArg, int argc, char **argv, char **cName)
{
	guint *playlist_id = pArg;

	*playlist_id = argv[0] ? atoi (argv[0]) : 0;

	return 0;
}

static guint
get_playlist_id (gchar *name)
{
	gint ret;
	guint id = 0;

	ret = xmms_sqlite_query (get_playlist_id_cb, &id,
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
		ret = xmms_sqlite_query (NULL, NULL,
		                         "delete from PlaylistEntries "
		                         "where playlist_id = %u", id);
		return ret ? id : 0;
	}

	/* supplied id is zero, so we need to add a new playlist first */
	ret = xmms_sqlite_query (get_playlist_id_cb, &id,
	                         "select MAX (id) from Playlist");
	if (!ret) {
		return 0;
	}

	id++; /* we want MAX + 1 */

	ret = xmms_sqlite_query (NULL, NULL,
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
		xmms_playlist_entry_t *entry;
		gchar mid[32];

		entry = xmms_playlist_get_byid (medialib->playlist,
		                                (guint) l->data);
		if (!entry) {
			g_mutex_unlock (medialib->mutex);

			return;
		}

		g_snprintf (mid, sizeof (mid), "mid://%s",
		            xmms_playlist_entry_property_get (entry,
		            XMMS_PLAYLIST_ENTRY_PROPERTY_MID));

		ret = xmms_sqlite_query (NULL, NULL,
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
			xmms_object_unref (entry);
			g_mutex_unlock (medialib->mutex);

			return;
		}

		xmms_object_unref (entry);
	}

	g_mutex_unlock (medialib->mutex);
}

static int
get_playlist_entries_cb (void *pArg, int argc, char **argv,
                         char **cName)
{
	GList **entries = pArg;

	/* valid prefixes for the playlist entries are:
	 * 'mid://' and 'sql://', so any valid string is longer
	 * than 6 characters.
	 */
	if (argv[0] && strlen (argv[0]) > 6) {
		*entries = g_list_prepend (*entries, g_strdup (argv[0]));
	}

	return 0;
}

static int
get_media_url_cb (void *pArg, int argc, char **argv, char **cName)
{
	gchar **url = pArg;

	*url = g_strdup (argv[0]);

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

	ret = xmms_sqlite_query (get_playlist_entries_cb, &entries,
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
		gchar *entry = entries->data, *url = NULL;
		guint mid;

		if (!strncmp (entry, "mid://", 6)) {
			mid = atoi (entry + 6);
			if (mid) {
				ret = xmms_sqlite_query (get_media_url_cb, &url,
				                         "select url from Media "
				                         "where id = %u", mid);
				if (ret) {
					xmms_playlist_addurl (medialib->playlist, url, NULL);
					g_free (url);
				}
			}
		} else if (!strncmp (entry, "sql://", 6)) {
			xmms_sqlite_query (playlist_load_sql_query_cb,
			                   medialib, "select url from Media where %q", entry);
		}

		g_free (entry);
		entries = g_list_delete_link (entries, entries);
	}

	g_mutex_unlock (medialib->mutex);
}

static void
ghash_to_entry (gpointer key, gpointer value, gpointer udata)
{
	xmms_playlist_entry_t *entry = (xmms_playlist_entry_t *)udata;
	if (g_strcasecmp ((gchar *)key, "id") == 0) {
		xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID, (gchar*)value);
	} else if (g_strcasecmp ((gchar *)key, "url") == 0) {
		return;
	} else {
		xmms_playlist_entry_property_set (entry, (gchar*)key, (gchar*)value);
	}
}

/**
 * Get a list of #xmms_playlist_entry_t 's that matches the query.
 * To use this function you'll need to select the URL column from the
 * database.
 *
 * Make sure that query are correctly escaped before entering here!
 */
GList *
xmms_medialib_select_entries (gchar *query, xmms_error_t *error)
{
	GList *l = NULL;

	l = xmms_medialib_select (query, error);
	if (l) {
		GList *ret = NULL;
		GList *n;
		xmms_playlist_entry_t *e;

		for (n = l; n; n = g_list_next (n)) {
			gchar *tmp = g_hash_table_lookup (n->data, "url");
			XMMS_DBG ("got %s", tmp);
			if (tmp) {
				e = xmms_playlist_entry_new (tmp);
				g_hash_table_foreach (n->data, ghash_to_entry, e);
				ret = g_list_prepend (ret, e);
				g_hash_table_destroy (n->data);
			}
		}

		ret = g_list_reverse (ret);

		g_list_free (l);

		return ret;
	}

	return NULL;
		
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

	ret = xmms_sqlite_query (select_callback, (void *)&res, query);

	if (!ret)
		return NULL;

	res = g_list_reverse (res);
	return res;
}

static int
mediarow_callback (void *pArg, int argc, char **argv, char **columnName) 
{
	gint i;
	xmms_playlist_entry_t *entry = pArg;

	for (i = 0; i < argc; i ++) {

		if (!argv[i])
			continue;

		XMMS_DBG ("Setting %s to %s", columnName[i], argv[i]);
		if (g_strcasecmp (columnName[i], "id") == 0) {
			xmms_playlist_entry_property_set (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID, argv[i]);
		} else if (g_strcasecmp (columnName[i], "url") == 0) {
			continue;
		} else {
			xmms_playlist_entry_property_set (entry, columnName[i], argv[i]);
		}
	}

	return 0;
	
}

static int
proprow_callback (void *pArg, int argc, char **argv, char **columnName) 
{
	xmms_playlist_entry_t *entry = pArg;

	XMMS_DBG ("Setting %s to %s", argv[0], argv[1]);
	xmms_playlist_entry_property_set (entry, argv[0], argv[1]);

	return 0;
}


/**
  * Takes a #xmms_playlist_entry_t and fills in the blanks as
  * they are in the database.
  */ 
gboolean
xmms_medialib_entry_get (xmms_playlist_entry_t *entry)
{
	gboolean ret;

	g_return_val_if_fail (medialib, FALSE);
	g_return_val_if_fail (entry, FALSE);

	g_mutex_lock (medialib->mutex);

	ret = xmms_sqlite_query (mediarow_callback, (void *)entry, 
				 "select * from Media where url = %Q order by id limit 1", 
				 xmms_playlist_entry_url_get (entry));

	if (!ret) {
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}

	if (!xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID)) {
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}

	ret = xmms_sqlite_query (proprow_callback, (void *) entry, 
				 "select key, value from Property where id = %q", 
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID));

	if (!ret) {
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}

	g_mutex_unlock (medialib->mutex);


	return TRUE;
}

/** Shutdown the medialib. */
void
xmms_medialib_shutdown ()
{
	g_return_if_fail (medialib);

	xmms_object_unref (medialib);
}

#ifdef HAVE_SQLITE
void
xmms_medialib_sql_set (sqlite *sql)
{
	medialib->sql = sql;
}

sqlite *
xmms_medialib_sql_get ()
{
	return medialib->sql;
}

#endif

/** @internal */
void
xmms_medialib_id_set (guint id)
{
	medialib->id = id;
}

/** @} */

