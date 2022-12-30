/*  S4 - An XMMS2 medialib backend
 *  Copyright (C) 2009 Sivert Berg
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

#include "s4.h"
#include <xmmspriv/xmms_collection.h>
#include <xmmspriv/xmms_utils.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include <glib.h>
#include <ctype.h>

extern gboolean try_upgrade (sqlite3 *sql);
extern void collection_restore (sqlite3 *db, GHashTable **ht);
extern void collection_dag_save (GHashTable **ht, const char *bdir);

static s4_t *s4;

/**
 * Check if a string is a number, if it is save it in val
 *
 * @param str The str to check
 * @param val A pointer to where we want the number to be saved
 * @return TRUE if the str is a number, FALSE otherwise
 */
static gboolean
xmms_is_int (const gchar *str, int *val)
{
	gboolean ret = FALSE;
	gchar *end;

	if (str != NULL && !isspace (*str)) {
		*val = strtol (str, &end, 10);
		if (*end == '\0')
			ret = TRUE;
	}

	return ret;
}

static int tree_cmp (gconstpointer a, gconstpointer b)
{
	int pa, pb;
	pa = *(int*)a;
	pb = *(int*)b;

	if (pa < pb)
		return -1;
	if (pa > pb)
		return 1;
	return 0;
}

static int source_callback (void *u, int argc, char *argv[], char *col[])
{
	GTree *sources = u;
	int i;
	char *src = NULL;
	int *id = malloc (sizeof (int));

	for (i = 0; i < argc; i++) {
		if (!strcmp ("source", col[i]))
			src = strdup (argv[i]);
		else if (!strcmp ("id", col[i]))
			*id = atoi (argv[i]);
	}

	g_tree_insert (sources, id, src);

	return 0;
}

static int media_callback (void *u, int argc, char *argv[], char *col[])
{
	GTree *sources = u;
	int id = 0, src_id = 0, i, intval;
	char *key = NULL, *val = NULL, *intrepr = NULL, *src;
	s4_val_t *id_val, *val_val;
	s4_transaction_t *trans;

	intrepr = val = NULL;

	for (i = 0; i < argc; i++) {
		if (!strcmp ("id", col[i])) {
			id = atoi (argv[i]);
		} else if (!strcmp ("key", col[i])) {
			key = argv[i];
		} else if (!strcmp ("value", col[i])) {
			val = argv[i];
		} else if (!strcmp ("intval", col[i])) {
			intrepr = argv[i];
		} else if (!strcmp ("source", col[i])) {
			src_id = atoi (argv[i]);
		}
	}

	src = g_tree_lookup (sources, &src_id);

	id_val = s4_val_new_int (id);

	if (xmms_is_int (intrepr, &intval)) {
		val_val = s4_val_new_int (intval);
	} else {
		val_val = s4_val_new_string (val);
	}

	do {
		trans = s4_begin (s4, 0);
		s4_add (trans, "song_id", id_val, key, val_val, src);
	} while (!s4_commit (trans));

	s4_val_free (val_val);
	s4_val_free (id_val);

	return 0;
}

int main (int argc, char *argv[])
{
	sqlite3 *db;
	char *coll_path, *foo, *bar, *uuid, *errmsg = NULL;
	int ret, i, uuid_len;
	GTree *sources = g_tree_new (tree_cmp);
	GHashTable **ht;

	if (argc != 4) {
		fprintf (stderr, "Usage: %s infile outfile\n"
				"\tinfile  - the sql file to import\n"
				"\toutfile - the s4 file to write to\n"
				"\tcolldir - the directory to place the collections\n",
				argv[0]);
		exit (1);
	}

	ret = sqlite3_open (argv[1], &db);
	if (ret) {
		fprintf (stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close (db);
		exit (1);
	}

	if (!try_upgrade (db)) {
		fprintf (stderr, "Could not upgrade sqlite database to latest version\n");
		sqlite3_close (db);
		exit (1);
	}

	s4 = s4_open (argv[2], NULL, S4_NEW);
	if (s4 == NULL) {
		fprintf (stderr, "Can't open s4 file\n");
		exit (1);
	}

	ret = sqlite3_exec (db, "select id,source from Sources;",
			source_callback, sources, &errmsg);

	ret = sqlite3_exec (db, "select id,key,value,intval,source from Media;",
			media_callback, sources, &errmsg);


	ht = malloc (sizeof (GHashTable*) * XMMS_COLLECTION_NUM_NAMESPACES);
	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; i++) {
		ht[i] = g_hash_table_new (g_str_hash, g_str_equal);
	}

	/* Replace ${uuid} in the coll path with the real uuid */
	uuid = s4_get_uuid_string (s4);
	uuid_len = strlen (uuid);
	coll_path = strdup (argv[3]);

	while ((foo = strstr (coll_path, "${uuid}")) != NULL) {
		int uuid_pos = foo - coll_path;

		bar = malloc (strlen (coll_path) - strlen ("${uuid}") + uuid_len);
		memcpy (bar, coll_path, uuid_pos);
		strcpy (bar + uuid_pos, uuid);
		strcpy (bar + uuid_pos + uuid_len, foo + strlen ("${uuid}"));
		free (coll_path);
		coll_path = bar;
	}

	collection_restore (db, ht);
	collection_dag_save (ht, coll_path);
	free (coll_path);

	sqlite3_close (db);
	s4_close (s4);

	return 0;
}
