/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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
 *  Functions to serialize (save/restore) collections.
 */

#include "xmmspriv/xmms_collserial.h"
#include "xmmspriv/xmms_collection.h"
#include "xmmspriv/xmms_utils.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>


#define COLL_BUILD_PATH(...) g_build_filename (coll_path, __VA_ARGS__, NULL)

/* If this is set to 1 xmms_collection_dag_save will NOT save anything.
 * This is to prevent overwriting anything if we have trouble opening files.
 */
int disable_saving = 0;
char *coll_path = NULL;


static xmmsv_coll_t *xmms_collection_read_operator (FILE *file);
static void xmms_collection_write_operator (xmmsv_coll_t *coll, FILE *file);
static void write_operator (void *key, void *value, void *udata);
static void write_coll_attributes (const char *key, xmmsv_t *value, void *udata);

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

static void
setup_coll_path (xmms_coll_dag_t *dag, const gchar *uuid)
{
	xmms_config_property_t *coll_conf;
	char *path, *foo, *bar;
	int uuid_len;

	if (coll_path != NULL)
		return;

	path = XMMS_BUILD_PATH ("collections", "${uuid}");
	coll_conf = xmms_config_property_register ("collection.directory",
	                                           path, NULL, NULL);
	coll_path = strdup (xmms_config_property_get_string (coll_conf));
	g_free (path);

	uuid_len = strlen (uuid);

	/* Replace all occurences of ${uuid} with the real uuid */
	while ((foo = strstr (coll_path, "${uuid}")) != NULL) {
		int uuid_pos = foo - coll_path;

		bar = malloc (strlen (coll_path) + 1 + uuid_len - 7);
		memcpy (bar, coll_path, uuid_pos);
		strcpy (bar + uuid_pos, uuid);
		strcpy (bar + uuid_pos + uuid_len, foo + 7);

		free (coll_path);
		coll_path = bar;
	}
}


/** Save the collection DAG to disc.
 *
 * @param dag  The collection DAG to save.
 */
void
xmms_collection_dag_save (xmms_coll_dag_t *dag, const gchar *uuid)
{
	gint i;
	char *path;
	const char *namespace, *name;
	FILE *file;
	xmmsv_coll_t *coll;

	setup_coll_path (dag, uuid);

	if (disable_saving)
		return;

	/* Write all collections in all namespaces */
	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		namespace = xmms_collection_get_namespace_string (i);
		path = COLL_BUILD_PATH (namespace);

		if (!create_dir (path))
			return;

		g_free (path);

		xmms_collection_foreach_in_namespace (dag, i, write_operator,
		                                      (void*)namespace);
	}

	/* We treat the _active entry a bit differently,
	 * we simply write the name of the playlist it
	 * points to in the file
	 */
	coll = xmms_collection_get_pointer (dag, XMMS_ACTIVE_PLAYLIST,
	                                    XMMS_COLLECTION_NSID_PLAYLISTS);
	name = xmms_collection_find_alias (dag,
	                                   XMMS_COLLECTION_NSID_PLAYLISTS,
	                                   coll, XMMS_ACTIVE_PLAYLIST);
	path = COLL_BUILD_PATH (XMMS_COLLECTION_NS_PLAYLISTS,
	                        XMMS_ACTIVE_PLAYLIST);
	file = fopen (path, "w");

	if (file == NULL) {
		xmms_log_error ("Could not open %s, %s", path, strerror (errno));
	} else {
		fprintf (file, "%s", name);
		fclose (file);
	}

	g_free (path);
}

/** Restore the collection DAG from disc.
 *
 * @param dag  The collection DAG to restore to.
 */
void
xmms_collection_dag_restore (xmms_coll_dag_t *dag, const gchar *uuid)
{
	xmmsv_coll_t *coll = NULL;
	char *path, buffer[1024];
	const char *label, *namespace;
	GDir *dir;
	FILE *file;
	int i;

	setup_coll_path (dag, uuid);

	for (i = 0; i < XMMS_COLLECTION_NUM_NAMESPACES; ++i) {
		namespace = xmms_collection_get_namespace_string (i);
		path = COLL_BUILD_PATH (namespace);
		dir = g_dir_open (path, 0, NULL);
		g_free (path);

		while (dir != NULL && (label = g_dir_read_name (dir)) != NULL) {
			if (strcmp (label, XMMS_ACTIVE_PLAYLIST) == 0)
				continue;

			path = COLL_BUILD_PATH (namespace, label);
			file = fopen (path, "r");

			if (file == NULL) {
				xmms_log_error ("Could not open %s, %s. "
				                "Collections will not be saved (to prevent overwriting something)",
				                path, strerror (errno));
				disable_saving = 1;
			} else {
				coll = xmms_collection_read_operator (file);
				fclose (file);
				if (coll != NULL)
					xmms_collection_dag_replace (dag, i, g_strdup (label), coll);
			}

			g_free (path);
		}

		if (dir != NULL)
			g_dir_close (dir);
	}

	path = COLL_BUILD_PATH (XMMS_COLLECTION_NS_PLAYLISTS,
	                        XMMS_ACTIVE_PLAYLIST);
	file = fopen (path, "r");

	if (file == NULL) {
		coll = xmms_collection_get_pointer (dag, "Default",
		                                    XMMS_COLLECTION_NSID_PLAYLISTS);

		if (coll == NULL) {
			coll = xmmsv_coll_new (XMMS_COLLECTION_TYPE_IDLIST);
			xmms_collection_dag_replace (dag, XMMS_COLLECTION_NSID_PLAYLISTS,
			                             g_strdup ("Default"), coll);
		}
	} else {
		fgets (buffer, 1024, file);
		fclose (file);

		coll = xmms_collection_get_pointer (dag, buffer,
		                                    XMMS_COLLECTION_NSID_PLAYLISTS);
	}

	g_free (path);
	xmmsv_coll_ref (coll);
	xmms_collection_dag_replace (dag, XMMS_COLLECTION_NSID_PLAYLISTS,
	                             g_strdup (XMMS_ACTIVE_PLAYLIST), coll);

	/* FIXME: validate ? */

	/* Link references in collections to actual operators */
	xmms_collection_apply_to_all_collections (dag, bind_all_references, NULL);
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
read_attributes (xmmsv_coll_t *coll, FILE *file)
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
		xmmsv_coll_attribute_set (coll, key, val);
	}
}

/* Read the idlist */
static void
read_idlist (xmmsv_coll_t *coll, FILE *file)
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
static xmmsv_coll_t *
xmms_collection_read_operator (FILE *file)
{
	xmmsv_coll_t *coll;
	xmmsv_coll_t *op;
	xmmsv_coll_type_t type;
	int ret = fscanf (file, " ( %i ", (int*)&type);

	if (ret == EOF || ret == 0)
		return NULL;

	coll = xmmsv_coll_new (type);

	read_attributes (coll, file);
	read_idlist (coll, file);

	while ((op = xmms_collection_read_operator (file)) != NULL) {
		xmmsv_coll_add_operand (coll, op);
		xmmsv_coll_unref (op);
	}

	fscanf (file, " ) ");

	return coll;
}


/** Write the given collection to the file given.
 *
 * @param coll The collection to write
 * @param file The file to write to
 */
static void
xmms_collection_write_operator (xmmsv_coll_t *coll, FILE *file)
{
	gint i;
	int32_t id;
	xmmsv_coll_t *op;
	xmmsv_t *attrs;

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
	char *path = COLL_BUILD_PATH (namespace, label);
	xmmsv_coll_t *coll = value;
	FILE *file;

	if (strcmp (key, XMMS_ACTIVE_PLAYLIST) != 0) {
		file = fopen (path, "w");

		if (file == NULL) {
			xmms_log_error ("Could not open %s, %s.", path, strerror (errno));
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
