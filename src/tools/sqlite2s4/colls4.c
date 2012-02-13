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


#include "xmmspriv/xmms_collection.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>


typedef struct {
	const gchar *key;
	xmmsv_coll_t *value;
} coll_table_pair_t;

void collection_dag_save (GHashTable **ht, const char *bdir);
static void xmms_collection_write_operator (xmmsv_coll_t *coll, FILE *file);
static void write_operator (void *key, void *value, void *udata);
static void write_coll_attributes (const char *key, xmmsv_t *value, void *udata);

const char *base_dir = "";

/*
 * How it works:
 * Collections are written to files in
 * {config_dir}/collections/{namespace}/{collection_name}
 *
 * Collections are written like this:
 * ( type [ "key":"value" ... ] [ id1 id2 ... ] (..) (..) .. )
 *   type    attributes           idlist         operands
 */


/* Creates a directory (if it doesn't exist) and removes everything
 * that's in it.
 */
static int
create_dir (const char *path)
{
	GDir *dir;
	char buffer[PATH_MAX];
	const char *name;
	int len;

	if (g_mkdir_with_parents (path, 0755)) {
		printf ("Could not create %s\n", path);
		return 0;
	}

	dir = g_dir_open (path, 0, NULL);
	len = strlen (path) + 1;

	sprintf (buffer, "%s/", path);

	while ((name = g_dir_read_name (dir)) != NULL) {
		strcpy (buffer + len, name);
		g_unlink (buffer);
	}

	g_dir_close (dir);

	return 1;
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
        xmmsv_coll_t *coll = (xmmsv_coll_t*)val;

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
collection_dag_save (GHashTable **ht, const char *bdir)
{
	gint i;
	char *path;
	const char *namespace, *name;
	FILE *file;
	xmmsv_coll_t *coll;

	base_dir = bdir;

	/* Write all collections in all namespaces */
	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		namespace = get_namespace_string (i);
		path = g_build_filename (base_dir, namespace, NULL);

		if (!create_dir (path))
			return;

		g_free (path);

		g_hash_table_foreach (ht[i], write_operator, (void*)namespace);
	}

	/* We treat the _active entry a bit differently,
	 * we simply write the name of the playlist it
	 * points to in the file
	 */
	coll = g_hash_table_lookup (ht[XMMS_COLLECTION_NSID_PLAYLISTS], XMMS_ACTIVE_PLAYLIST);

	/* Find alias */
	name = NULL;
	coll_table_pair_t search_pair = { XMMS_ACTIVE_PLAYLIST, coll };

	if (g_hash_table_find (ht[XMMS_COLLECTION_NSID_PLAYLISTS], value_match_save_key,
				&search_pair) != NULL) {
		name = search_pair.key;
	}

	path = g_build_filename (base_dir, XMMS_COLLECTION_NS_PLAYLISTS,
			XMMS_ACTIVE_PLAYLIST, NULL);
	file = fopen (path, "w");

	if (file == NULL) {
		fprintf (stderr, "Could not open %s, %s", path, strerror (errno));
	} else {
		fprintf (file, "%s", name);
		fclose (file);
	}

	g_free (path);
}


/** Write the given collection to the file given.
 *
 * @param coll The collection to write
 * @param file The file to write to
 */
static void
xmms_collection_write_operator (xmmsv_coll_t *coll, FILE *file)
{
	int32_t id;
	gint i;
	xmmsv_coll_t *op;
	xmmsv_t *attrs;

	if (coll == NULL)
		return;

	fprintf (file, "( %i [ ", xmmsv_coll_get_type (coll));

	/* Write attributes */
	attrs = xmmsv_coll_attributes_get (coll);
	xmmsv_dict_foreach (attrs, write_coll_attributes, file);

	fprintf (file, "] [ ");

	/* Write idlist */
	for (i = 0; xmmsv_coll_idlist_get_index (coll, i, &id); i++) {
		fprintf (file, "%i ", id);
	}
	fprintf (file, "] ");

	/* Save operands and connections (don't recurse in ref operand) */
	if (xmmsv_coll_get_type (coll) != XMMS_COLLECTION_TYPE_REFERENCE) {
		xmmsv_t *tmp;
		xmmsv_list_iter_t *iter;

		xmmsv_get_list_iter (xmmsv_coll_operands_get (coll), &iter);

		for (xmmsv_list_iter_first (iter);
		     xmmsv_list_iter_valid (iter);
		     xmmsv_list_iter_next (iter)) {

			xmmsv_list_iter_entry (iter, &tmp);
			xmmsv_get_coll (tmp, &op);

			xmms_collection_write_operator (op, file);
		}
		xmmsv_list_iter_explicit_destroy (iter);
	}

	fprintf (file, ")");
}

/* For all label-operator pairs, write the operator and all its
 * operands to the file recursively. */
static void
write_operator (void *key, void *value, void *udata)
{
	char *label = key;
	char *namespace = udata;
	char *path = g_build_filename (base_dir, namespace, label, NULL);
	xmmsv_coll_t *coll = value;
	FILE *file;

	if (strcmp (key, XMMS_ACTIVE_PLAYLIST) != 0) {
		file = fopen (path, "w");

		if (file == NULL) {
			fprintf (stderr, "Could not open %s, %s.", path, strerror (errno));
		} else {
			xmms_collection_write_operator (coll, file);
			fclose (file);
		}
	}

	g_free (path);
}

/* Write the string given to the file. Replace " with \",
 * \ with \\ and add enclosing "'s
 */
static void
write_string (FILE *file, const char *str)
{
	fputc ('"', file);
	while (*str) {
		switch (*str) {
			case '\\':
			case '"':
				fputc ('\\', file);
			default:
				fputc (*str++, file);
		}
	}
	fputc ('"', file);
}

/* Write the attributes to file */
static void
write_coll_attributes (const char *key, xmmsv_t *value, void *udata)
{
	FILE *file = udata;
	const gchar *s;
	int r;

	r = xmmsv_get_string (value, &s);
	g_return_if_fail (r);

	write_string (file, key);
	fputc (':', file);
	write_string (file, s);
	fputc (' ', file);
}
