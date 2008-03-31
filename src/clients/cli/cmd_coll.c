/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
#include "cmd_coll.h"
#include "common.h"



cmds coll_commands[] = {
	{ "save", "[collname] [pattern] - Save a pattern as a collection", cmd_coll_save },
	{ "rename", "[oldname] [newname] - Rename a collection", cmd_coll_rename },
	{ "list", "[namespace] - List all collections in a given namespace", cmd_coll_list },
	{ "query", "[collname] [order] - Display all the media in a collection", cmd_coll_query },
	{ "queryadd", "[collname] [order] - Add all media in a collection to active playlist", cmd_coll_queryadd },
	{ "find", "[mid] [namespace] - Find all collections that contain the given media", cmd_coll_find },
	{ "get", "[collname] - Display the structure of a collection", cmd_coll_get },
	{ "remove", "[collname] - Remove a saved collection", cmd_coll_remove },
	{ "attr", "[collname] [attr] [val] - Get/set an attribute for a saved collection", cmd_coll_attr },
	{ "sync", "- Synchronize the collections into the medialib", cmd_coll_sync },
	{ NULL, NULL, NULL },
};


static void
cmd_coll_help (void) {
	gint i;

	print_info ("Available collection commands:");
	for (i = 0; coll_commands[i].name; i++) {
		print_info ("  %s\t %s", coll_commands[i].name,
		            coll_commands[i].help);
	}
}

/* Dispatch collection subcommands */
void
cmd_coll (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gint i;
	if (argc < 3) {
		cmd_coll_help ();
		return;
	}

	for (i = 0; coll_commands[i].name; i++) {
		if (g_strcasecmp (coll_commands[i].name, argv[2]) == 0) {
			coll_commands[i].func (conn, argc, argv);
			return;
		}
	}

	cmd_coll_help ();
	print_error ("Unrecognised coll command: %s", argv[2]);
}


static void
coll_list (xmmsc_connection_t *conn, const gchar *namespace)
{
	xmmsc_result_t *res;

	res = xmmsc_coll_list (conn, namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't list collections in namespace %s: %s",
		             namespace, xmmsc_result_get_error (res));
	} else {
		while (xmmsc_result_list_valid (res)) {
			const gchar *name;

			xmmsc_result_get_string (res, &name);
			print_info ("%s", name);
			xmmsc_result_list_next (res);
		}
	}
	xmmsc_result_unref (res);
}

static void
coll_find (xmmsc_connection_t *conn, const gchar *namespace, guint mid)
{
	xmmsc_result_t *res;

	res = xmmsc_coll_find (conn, mid, namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't find collections containing media %d in namespace %s: %s",
		             mid, namespace, xmmsc_result_get_error (res));
	} else {
		while (xmmsc_result_list_valid (res)) {
			const gchar *name;

			xmmsc_result_get_string (res, &name);
			print_info ("%s", name);
			xmmsc_result_list_next (res);
		}
	}
	xmmsc_result_unref (res);
}

/* Produce a GString from the idlist of the collection (must be freed manually!) */
static GString *
coll_idlist_to_string (xmmsc_coll_t *coll)
{
	gint i;
	guint *idlist;
	GString *s;

	s = g_string_new ("(");

	idlist = xmmsc_coll_get_idlist (coll);
	for (i = 0; idlist[i] != 0; ++i) {
		if (i > 0) {
			g_string_append (s, ", ");
		}
		g_string_append_printf (s, "%d", idlist[i]);
	}
	g_string_append_c (s, ')');

	return s;
}

/* Dump the structure of the collection as a string */
static void
coll_dump (xmmsc_coll_t *coll, unsigned int level)
{
	gint i;
	gchar *indent;

	gchar *attr1;
	gchar *attr2;
	xmmsc_coll_t *operand;
	GString *idlist_str;

	indent = g_malloc ((level * 2) + 1);
	for (i = 0; i < level * 2; ++i) {
		indent[i] = ' ';
	}
	indent[i] = '\0';

	/* type */
	switch (xmmsc_coll_get_type (coll)) {
	case XMMS_COLLECTION_TYPE_REFERENCE:
		xmmsc_coll_attribute_get (coll, "reference", &attr1);
		print_info ("%sReference: '%s'", indent, attr1);
		break;

	case XMMS_COLLECTION_TYPE_UNION:
		print_info ("%sUnion:", indent);
		for (xmmsc_coll_operand_list_first (coll);
		     xmmsc_coll_operand_list_entry (coll, &operand);
		     xmmsc_coll_operand_list_next (coll)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_INTERSECTION:
		print_info ("%sIntersection:", indent);
		for (xmmsc_coll_operand_list_first (coll);
		     xmmsc_coll_operand_list_entry (coll, &operand);
		     xmmsc_coll_operand_list_next (coll)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_COMPLEMENT:
		print_info ("%sComplement:", indent);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_EQUALS:
		xmmsc_coll_attribute_get (coll, "field",  &attr1);
		xmmsc_coll_attribute_get (coll, "value", &attr2);
		print_info ("%sEquals ('%s', '%s') for:", indent, attr1, attr2);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_HAS:
		xmmsc_coll_attribute_get (coll, "field",  &attr1);
		print_info ("%sHas ('%s') for:", indent, attr1);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_MATCH:
		xmmsc_coll_attribute_get (coll, "field",  &attr1);
		xmmsc_coll_attribute_get (coll, "value", &attr2);
		print_info ("%sMatch ('%s', '%s') for:", indent, attr1, attr2);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_SMALLER:
		xmmsc_coll_attribute_get (coll, "field",  &attr1);
		xmmsc_coll_attribute_get (coll, "value", &attr2);
		print_info ("%sSmaller ('%s', '%s') for:", indent, attr1, attr2);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_GREATER:
		xmmsc_coll_attribute_get(coll, "field",  &attr1);
		xmmsc_coll_attribute_get(coll, "value", &attr2);
		print_info ("%sGreater ('%s', '%s') for:", indent, attr1, attr2);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	case XMMS_COLLECTION_TYPE_IDLIST:
		idlist_str = coll_idlist_to_string (coll);
		print_info ("%sIdlist: %s", indent, idlist_str->str);
		g_string_free (idlist_str, TRUE);
		break;

	case XMMS_COLLECTION_TYPE_QUEUE:
		idlist_str = coll_idlist_to_string (coll);
		print_info ("%sQueue: %s", indent, idlist_str->str);
		g_string_free (idlist_str, TRUE);
		break;

	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		idlist_str = coll_idlist_to_string (coll);
		print_info ("%sParty Shuffle: %s from :", indent, idlist_str->str);
		g_string_free (idlist_str, TRUE);
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &operand)) {
			coll_dump (operand, level + 1);
		}
		break;

	default:
		print_info ("%sUnknown Operator!", indent);
		break;
	}
}

static void
coll_print (xmmsc_coll_t *coll)
{
	coll_dump (coll, 0);
}

static void
coll_attr_print (const gchar *key, const gchar *val, gpointer userdata)
{
	print_info ("[%s] %s", (gchar*)key, (gchar*)val);
}



void
cmd_coll_save (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name, *namespace;
	xmmsc_coll_t *coll = NULL;
	xmmsc_result_t *res;
	gchar *pattern;
	gchar **args;
	int i;

	if (argc < 5) {
		print_error ("usage: coll save [collname] [pattern]");
	}

	if (!coll_read_collname (argv[3], &name, &namespace)) {
		print_error ("invalid collection name");
	}

	args = g_new0 (char*, argc - 3);
	for (i = 0; i < argc - 4; i++) {
		args[i] = string_escape (argv[i + 4]);
	}
	args[i] = NULL;

	pattern = g_strjoinv (" ", args);
	if (!xmmsc_coll_parse (pattern, &coll)) {
		print_error ("Unable to generate query");
	}

	for (i = 0; i < argc - 4; i++) {
		g_free (args[i]);
	}
	g_free (args);
	g_free (pattern);

	res = xmmsc_coll_save (conn, coll, name, namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't save %s in namespace %s: %s", name, namespace,
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
	xmmsc_coll_unref (coll);
	g_free (name);
	g_free (namespace);
}


void
cmd_coll_rename (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *from_name, *from_namespace;
	gchar *to_name, *to_namespace;
	xmmsc_result_t *res;

	if (argc < 5) {
		print_error ("usage: coll rename [collname] [newname]");
	}

	if (!coll_read_collname (argv[3], &from_name, &from_namespace)) {
		print_error ("invalid old collection name");
	}

	if (!coll_read_collname (argv[4], &to_name, &to_namespace)) {
		print_error ("invalid new collection name");
	}

	if (strcmp (from_namespace, to_namespace) != 0) {
		print_error ("cannot rename collection to a different namespace");
	}

	res = xmmsc_coll_rename (conn, from_name, to_name, from_namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("Couldn't rename %s to %s in namespace %s: %s",
		             from_name, to_name, from_namespace,
		             xmmsc_result_get_error (res));
	}
	xmmsc_result_unref (res);
	g_free (from_name);
	g_free (from_namespace);
	g_free (to_name);
	g_free (to_namespace);
}


void
cmd_coll_list (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	/* No namespace, show all namespaces */
	if (argc < 4) {
		print_info ("* %s Namespace:", "Playlists");
		coll_list (conn, "Playlists");
		print_info ("* %s Namespace:", "Collections");
		coll_list (conn, "Collections");
	} else {
		coll_list (conn, argv[3]);
	}
}


void
cmd_coll_query (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name, *namespace;
	gchar **order = NULL;
	xmmsc_coll_t *collref;
	xmmsc_result_t *res;
	GList *n = NULL;

	if (argc < 4) {
		print_error ("usage: coll query [collname] [order]");
	}

	if (!coll_read_collname (argv[3], &name, &namespace)) {
		print_error ("invalid collection name");
	}

	/* allow custom ordering, if specified */
	if (argc > 4) {
		order = g_strsplit (argv[4], ",", 0);
		g_assert (order);
	}

	/* Create a reference collection to the saved coll */
	collref = xmmsc_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
 	xmmsc_coll_attribute_set (collref, "reference", name);
 	xmmsc_coll_attribute_set (collref, "namespace", namespace);

	res = xmmsc_coll_query_ids (conn, collref, (const gchar**)order, 0, 0);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	while (xmmsc_result_list_valid (res)) {
		guint id;

		if (!xmmsc_result_get_uint (res, &id)) {
			print_error ("Broken resultset");
		}

		n = g_list_prepend (n, XINT_TO_POINTER (id));
		xmmsc_result_list_next (res);
	}
	n = g_list_reverse (n);
	format_pretty_list (conn, n);
	g_list_free (n);

	xmmsc_result_unref (res);

	if (order) {
		g_strfreev (order);
	}

	xmmsc_coll_unref (collref);
	g_free (name);
	g_free (namespace);
}

void
cmd_coll_queryadd (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name, *namespace;
	gchar **order = NULL;
	xmmsc_coll_t *collref;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("usage: coll queryadd [collname] [order]");
	}

	if (!coll_read_collname (argv[3], &name, &namespace)) {
		print_error ("invalid collection name");
	}

	/* allow custom ordering, if specified */
	if (argc > 4) {
		order = g_strsplit (argv[4], ",", 0);
		g_assert (order);
	}

	/* Create a reference collection to the saved coll */
	collref = xmmsc_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsc_coll_attribute_set (collref, "reference", name);
	xmmsc_coll_attribute_set (collref, "namespace", namespace);

	res = xmmsc_playlist_add_collection (conn, NULL, collref, (const gchar**)order);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	xmmsc_result_unref (res);

	if (order) {
		g_strfreev (order);
	}

	xmmsc_coll_unref (collref);
	g_free (name);
	g_free (namespace);
}

void
cmd_coll_find (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	guint mid;

	if (argc < 4) {
		print_error ("usage: coll find [mid] [namespace]");
	}

	mid = strtoul (argv[3], NULL, 10);
	if (!mid) {
		print_error ("invalid media id");
	}

	if (argc < 5) {
		print_info ("* %s Namespace:", "Playlists");
		coll_find (conn, "Playlists", mid);
		print_info ("* %s Namespace:", "Collections");
		coll_find (conn, "Collections", mid);
	} else {
		coll_find (conn, argv[4], mid);
	}
}


void
cmd_coll_get (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name, *namespace;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("usage: coll get [collname]");
	}

	if (!coll_read_collname (argv[3], &name, &namespace)) {
		print_error ("invalid collection name");
	}

	res = xmmsc_coll_get (conn, name, namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	} else {
		xmmsc_coll_t *coll;
		xmmsc_result_get_collection (res, &coll);
		coll_print (coll);
	}

	g_free (name);
	g_free (namespace);
}


void
cmd_coll_remove (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name, *namespace;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("usage: coll remove [collname]");
	}

	if (!coll_read_collname (argv[3], &name, &namespace)) {
		print_error ("invalid collection name");
	}

	res = xmmsc_coll_remove (conn, name, namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}

	g_free (name);
	g_free (namespace);
}


void
cmd_coll_attr (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	gchar *name, *namespace;
	xmmsc_result_t *res;

	if (argc < 4) {
		print_error ("usage: coll attr [collname] [attr] [val]");
	}

	if (!coll_read_collname (argv[3], &name, &namespace)) {
		print_error ("invalid collection name");
	}

	res = xmmsc_coll_get (conn, name, namespace);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	} else {
		xmmsc_coll_t *coll;
		xmmsc_result_get_collection (res, &coll);

		/* Print all attributes */
		if (argc == 4) {
			xmmsc_coll_attribute_foreach (coll, coll_attr_print, NULL);

		/* Print the given attribute */
		} else if (argc == 5) {
			gchar *val;
			if (xmmsc_coll_attribute_get (coll, argv[4], &val)) {
				coll_attr_print (argv[4], val, NULL);
			}

		/* Set the given attribute */
		} else {
			xmmsc_result_t *saveres;
			if (strlen (argv[5]) > 0) {
				xmmsc_coll_attribute_set (coll, argv[4], argv[5]);
			} else {
				/* Empty value = remove */
				xmmsc_coll_attribute_remove (coll, argv[4]);
			}

			saveres = xmmsc_coll_save (conn, coll, name, namespace);
			xmmsc_result_wait (saveres);

			if (xmmsc_result_iserror (saveres)) {
				print_error ("Couldn't save %s in namespace %s: %s",
				             name, namespace,
				             xmmsc_result_get_error (saveres));
			}

			xmmsc_result_unref (saveres);
		}

		xmmsc_coll_unref (coll);
	}

	xmmsc_result_unref (res);

	g_free (name);
	g_free (namespace);
}

void
cmd_coll_sync (xmmsc_connection_t *conn, gint argc, gchar **argv)
{
	xmmsc_result_t *res;

	res = xmmsc_coll_sync (conn);
	xmmsc_result_wait (res);

	if (xmmsc_result_iserror (res)) {
		print_error ("%s", xmmsc_result_get_error (res));
	}
}
