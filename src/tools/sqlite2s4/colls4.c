/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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


#include <xmmspriv/xmms_collection.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>


typedef struct {
	const gchar *key;
	xmmsv_t *value;
} coll_table_pair_t;

void collection_dag_save (GHashTable **ht, const char *path);

/**
 * Returns TRUE if the value of the pair is equal to the value stored
 * in the udata structure, and save the corresponding key in that
 * structure.
 */
static gboolean
value_match_save_key (gpointer key, gpointer val, gpointer udata)
{
        gboolean found = FALSE;
        coll_table_pair_t *pair = (coll_table_pair_t*)udata;
        xmmsv_t *coll = (xmmsv_t*)val;

        /* value matching and key not ignored, found! */
        if ((coll == pair->value) &&
            (pair->key == NULL || strcmp (pair->key, key) != 0)) {
                pair->key = key;
                found = TRUE;
        }

        return found;
}


/** Save the collection DAG to disc.
 *
 * @param dag  The collection DAG to save.
 */
void
collection_dag_save (GHashTable **ht, const char *path)
{
	GError *error = NULL;
	GHashTableIter iter;
	xmmsv_t *snapshot, *collections, *playlists, *coll, *active_playlist, *serialized;
	const guchar *buffer;
	const gchar *name;
	gchar *dirname;
	guint length;

	snapshot = xmmsv_new_dict ();

	collections = xmmsv_new_dict ();
	xmmsv_dict_set (snapshot, "collections", collections);
	xmmsv_unref (collections);

	playlists = xmmsv_new_dict ();
	xmmsv_dict_set (snapshot, "playlists", playlists);
	xmmsv_unref (playlists);

	g_hash_table_iter_init (&iter, ht[XMMS_COLLECTION_NSID_COLLECTIONS]);
	while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &coll))
		xmmsv_dict_set (collections, name, coll);

	/* We treat the _active entry a bit differently,
	 * we simply store the name of the playlist it
	 * points to.
	 */
	active_playlist = g_hash_table_lookup (ht[XMMS_COLLECTION_NSID_PLAYLISTS], XMMS_ACTIVE_PLAYLIST);

	g_hash_table_iter_init (&iter, ht[XMMS_COLLECTION_NSID_PLAYLISTS]);
	while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &coll))
		if (coll != active_playlist || strcmp (name, XMMS_ACTIVE_PLAYLIST) != 0)
			xmmsv_dict_set (playlists, name, coll);

	/* Find alias */
	name = NULL;
	coll_table_pair_t search_pair = { XMMS_ACTIVE_PLAYLIST, coll };

	if (g_hash_table_find (ht[XMMS_COLLECTION_NSID_PLAYLISTS], value_match_save_key,
				&search_pair) != NULL) {
		name = search_pair.key;
	}

	xmmsv_dict_set_string (snapshot, "active-playlist", name);

	serialized = xmmsv_serialize (snapshot);
	xmmsv_unref (snapshot);

	xmmsv_get_bin (serialized, &buffer, &length);

	dirname = g_path_get_dirname (path);
	if (g_mkdir_with_parents (dirname, 0755) == 0) {
		if (!g_file_set_contents (path, (const gchar *) buffer, (gssize) length, &error)) {
			fprintf (stderr, "Error while writing collections / playlists to disk. (%s)", error->message);
		}
	} else {
		fprintf (stderr, "Error while creating directory for collections / playlists. (%s)", g_strerror (errno));
	}

	g_free (dirname);
	xmmsv_unref (serialized);
}
