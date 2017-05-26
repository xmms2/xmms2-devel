/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

/* Crude conversion tool for the first version of the collection
 * serialization format used during early development of XMMS2 0.9.
 *
 * This tool will never be needed for people who run stable versions,
 * and for those who want to keep their sanity, close this file now
 * and move on to reading the newer serialization code in collsync.c.
 */

#include <xmmspriv/xmms_collection.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#define COLL_BUILD_PATH(...) g_build_filename (__VA_ARGS__, NULL)

typedef struct {
	const gchar *key;
	xmmsv_t *value;
} coll_table_pair_t;

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


static const gchar *
get_namespace_string (xmms_collection_namespace_id_t nsid)
{
	const gchar *name;

	switch (nsid) {
	case XMMS_COLLECTION_NSID_ALL:
		name = XMMS_COLLECTION_NS_ALL;
		break;
	case XMMS_COLLECTION_NSID_COLLECTIONS:
		name = XMMS_COLLECTION_NS_COLLECTIONS;
		break;
	case XMMS_COLLECTION_NSID_PLAYLISTS:
		name = XMMS_COLLECTION_NS_PLAYLISTS;
		break;
	case XMMS_COLLECTION_NSID_INVALID:
	default:
		name = NULL;
		break;
	}

	return name;
}

/* Read an escaped string */
static void
read_string (FILE *file, char *buffer)
{
	int c;

	while (isspace (c = fgetc (file)) && c != EOF);

	while ((c = fgetc (file)) != '"' && c != EOF) {
		if (c == '\\')
			c = fgetc (file);

		*buffer++ = c;
	}

	while (isspace (c = fgetc (file)) && c != EOF);
	ungetc (c, file);

	*buffer = '\0';
}

/* Read all the attributes from the file */
static void
read_attributes (xmmsv_t *coll, FILE *file)
{
	char key[1024];
	char val[1024];
	int c;

	fscanf (file, " [ ");

	while ((c = getc (file)) != ']') {
		ungetc (c, file);
		read_string (file, key);
		fgetc (file); /* To remove the colon separating the strings */
		read_string (file, val);
		xmmsv_coll_attribute_set_string (coll, key, val);
	}
}

/* Read the idlist */
static void
read_idlist (xmmsv_t *coll, FILE *file)
{
	int id, c;

	fscanf (file, " [ ");

	while ((c = getc (file)) != ']') {
		ungetc (c, file);
		fscanf (file, " %i ", &id);
		xmmsv_coll_idlist_append (coll, id);
	}
}

/** Read a collection from the file given.
 *
 * @param file The file to read from.
 * @return The collection or NULL on error.
 */
static xmmsv_t *
collection_read_operator (FILE *file)
{
	xmmsv_t *coll, *op;
	xmmsv_coll_type_t type;
	int ret = fscanf (file, " ( %i ", (int*)&type);

	if (ret == EOF || ret == 0)
		return NULL;

	coll = xmmsv_new_coll (type);

	read_attributes (coll, file);
	read_idlist (coll, file);

	while ((op = collection_read_operator (file)) != NULL) {
		xmmsv_coll_add_operand (coll, op);
		xmmsv_unref (op);
	}

	fscanf (file, " ) ");

	return coll;
}

static xmmsv_t *
collection_get_pointer (GHashTable **ht, const gchar *collname, guint nsid)
{
	xmmsv_t *coll = NULL;
	gint i;

	if (nsid == XMMS_COLLECTION_NSID_ALL) {
		for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES && coll == NULL; ++i) {
			coll = g_hash_table_lookup (ht[i], collname);
		}
	} else {
		coll = g_hash_table_lookup (ht[nsid], collname);
	}

	return coll;
}

/** Update the DAG to update the value of the pair with the given key. */
static void
collection_dag_replace (GHashTable **ht,
                        xmms_collection_namespace_id_t nsid,
                        const gchar *key, xmmsv_t *newcoll)
{
	g_hash_table_replace (ht[nsid], g_strdup (key), newcoll);
}

/** Restore the collection DAG from disc.
 *
 * @param dag  The collection DAG to restore to.
 */
static gboolean
collection_dag_restore (GHashTable **ht, const gchar *base_path)
{
	xmmsv_t *coll = NULL;
	gchar *path, buffer[1024];
	const gchar *label, *namespace;
	GDir *dir;
	FILE *file;
	gint i;

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		namespace = get_namespace_string (i);
		path = COLL_BUILD_PATH (base_path, namespace);
		dir = g_dir_open (path, 0, NULL);
		g_free (path);

		while (dir != NULL && (label = g_dir_read_name (dir)) != NULL) {
			if (strcmp (label, XMMS_ACTIVE_PLAYLIST) == 0)
				continue;

			path = COLL_BUILD_PATH (base_path, namespace, label);
			file = fopen (path, "r");

			if (file == NULL) {
				fprintf (stderr, "Could not open %s, %s.", path, strerror (errno));
				return FALSE;
			} else {
				coll = collection_read_operator (file);
				fclose (file);
				if (coll != NULL)
					collection_dag_replace (ht, i, label, coll);
			}

			g_free (path);
		}

		if (dir != NULL)
			g_dir_close (dir);
	}

	path = COLL_BUILD_PATH (XMMS_COLLECTION_NS_PLAYLISTS, XMMS_ACTIVE_PLAYLIST);
	file = fopen (path, "r");

	if (file == NULL) {
		coll = collection_get_pointer (ht, "Default", XMMS_COLLECTION_NSID_PLAYLISTS);

		if (coll == NULL) {
			coll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_IDLIST);
			collection_dag_replace (ht, XMMS_COLLECTION_NSID_PLAYLISTS, "Default", coll);
		}
	} else {
		fgets (buffer, 1024, file);
		fclose (file);

		coll = collection_get_pointer (ht, buffer, XMMS_COLLECTION_NSID_PLAYLISTS);
	}


	collection_dag_replace (ht, XMMS_COLLECTION_NSID_PLAYLISTS,
	                        XMMS_ACTIVE_PLAYLIST, coll);


	g_free (path);

	return TRUE;
}

/** Save the collection DAG to disc.
 *
 * @param dag  The collection DAG to save.
 */
static gboolean
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
			return FALSE;
		}
	} else {
		fprintf (stderr, "Error while creating directory for collections / playlists. (%s)", g_strerror (errno));
		return FALSE;
	}

	g_free (dirname);
	xmmsv_unref (serialized);

	return TRUE;
}

static gboolean
collection_dir_rename (const gchar *path)
{
	gchar *legacy;
	gint64 now;
	gint res;

	now = g_get_real_time ();

	legacy = g_strdup_printf ("%s.%" G_GINT64_FORMAT ".legacy", path, now);
	res = g_rename (path, legacy);
	g_free (legacy);

	if (res != 0) {
		fprintf (stderr, "Could not rename old directory. (%s)\n", g_strerror (errno));
		return FALSE;
	}

	return TRUE;
}

int
main (int argc, char **argv)
{
	GHashTable **ht;
	gint i;

	if (argc != 2) {
		fprintf (stderr, "Usage: %s directory\n"
		         "\tdirectory  - the directory to convert\n",
		         argv[0]);
		exit (1);
	}

	ht = g_new (GHashTable *, XMMS_COLLECTION_NUM_NAMESPACES);
	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; i++) {
		ht[i] = g_hash_table_new (g_str_hash, g_str_equal);
	}

	if (g_file_test (argv[1], G_FILE_TEST_IS_REGULAR)) {
		fprintf (stderr, "The supplied path is not a directory, bailing\n");
		return EXIT_FAILURE;
	}

	if (!collection_dag_restore (ht, argv[1])) {
		return EXIT_FAILURE;
	}

	if (!collection_dir_rename (argv[1])) {
		return EXIT_FAILURE;
	}

	if (!collection_dag_save (ht, argv[1])) {
	    return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
