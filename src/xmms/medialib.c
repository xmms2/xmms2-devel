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
#include "xmms/config.h"
#include "xmms/medialib.h"
#include "xmms/transport.h"
#include "xmms/decoder.h"
#include "xmms/plugin.h"
#include "internal/plugin_int.h"
#include "internal/decoder_int.h"
#include "internal/transport_int.h"

#include <string.h>

#include <glib.h>

#ifdef HAVE_SQLITE
#include <sqlite.h>
#endif

struct xmms_medialib_St {
	GMutex *mutex;
	
	guint id;
#ifdef HAVE_SQLITE
	sqlite *sql;
#endif
};


xmms_medialib_t *
xmms_medialib_init (void)
{
	xmms_medialib_t *medialib = NULL;

	medialib = g_new0 (xmms_medialib_t, 1);
#ifdef HAVE_SQLITE
	if (!xmms_sqlite_open (medialib)) {
		g_free (medialib);
		return NULL;
	}
#endif /*HAVE_SQLITE*/
	medialib->mutex = g_mutex_new ();
	
	return medialib;
}

guint
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
	gchar *query;
	xmms_medialib_t *medialib = userdata;
	gint i = 0;

	while (_e[i]) {
		if (g_strcasecmp (_e[i], k) == 0) {
		    return;
		}
		i++;
	}

	query = g_strdup_printf ("insert into Property values (%d, '%s', '%s')", medialib->id - 1, k, v);
	xmms_sqlite_query (medialib, query, NULL, NULL);
}

gboolean
xmms_medialib_entry_store (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry)
{
	gchar *query;
	guint id;

	g_return_val_if_fail (medialib, FALSE);
	XMMS_DBG ("Storing entry to medialib!");
	g_mutex_lock (medialib->mutex);

	id = xmms_medialib_next_id (medialib);
	query = g_strdup_printf ("insert into Media values (%d, '%s', '%s', '%s', '%s', '%s', '%s')",
				 id, xmms_playlist_entry_url_get (entry),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ARTIST),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_ALBUM),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_TITLE),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_GENRE),
				 xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_LMOD));

	if (!xmms_sqlite_query (medialib, query, NULL, NULL)) {
		g_free (query);
		return FALSE;
	}

	g_free (query);

	xmms_playlist_entry_property_foreach (entry, insert_foreach, medialib);
				 
	
	g_mutex_unlock (medialib->mutex);

	return TRUE;
}

static int
mediarow_callback (void *pArg, int argc, char **argv, char **columnName) 
{
	gint i;
	xmms_playlist_entry_t *entry = pArg;

	for (i = 0; i < argc; i ++) {
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

	XMMS_DBG ("Setting %s to %s", argv[1], argv[0]);
	xmms_playlist_entry_property_set (entry, argv[1], argv[0]);

	return 0;
}


gboolean
xmms_medialib_entry_get (xmms_medialib_t *medialib, xmms_playlist_entry_t *entry)
{
	gchar *query;

	g_return_val_if_fail (medialib, FALSE);
	g_return_val_if_fail (entry, FALSE);

	g_mutex_lock (medialib->mutex);

	query = g_strdup_printf ("select * from Media where url = '%s' order by id limit 1", xmms_playlist_entry_url_get (entry));
	if (!xmms_sqlite_query (medialib, query, mediarow_callback, (void *)entry)) {
		g_free (query);
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}
	g_free (query);

	if (!xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID)) {
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}

	query = g_strdup_printf ("select key, value from Property where id = %s", xmms_playlist_entry_property_get (entry, XMMS_PLAYLIST_ENTRY_PROPERTY_MID));

	if (!xmms_sqlite_query (medialib, query, proprow_callback, (void *)entry)) {
		g_free (query);
		g_mutex_unlock (medialib->mutex);
		return FALSE;
	}
	g_free (query);

	g_mutex_unlock (medialib->mutex);


	return TRUE;
}

void
xmms_medialib_close (xmms_medialib_t *medialib)
{
	g_return_if_fail (medialib);

#ifdef HAVE_SQLITE
	xmms_sqlite_close (medialib);
#endif
	g_free (medialib);
}

#ifdef HAVE_SQLITE
void
xmms_medialib_sql_set (xmms_medialib_t *medialib, sqlite *sql)
{
	medialib->sql = sql;
}

sqlite *
xmms_medialib_sql_get (xmms_medialib_t *medialib)
{
	return medialib->sql;
}

#endif

void
xmms_medialib_id_set (xmms_medialib_t *medialib, guint id)
{
	medialib->id = id;
}

