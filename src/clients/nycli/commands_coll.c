/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 */

#include <string.h>

#include <glib.h>
#include <xmmsclient/xmmsclient.h>

#include "main.h"
#include "cli_cache.h"
#include "cli_infos.h"
#include "column_display.h"
#include "configuration.h"
#include "commands.h"
#include "command_utils.h"
#include "utils.h"
#include "xmmscall.h"

static void
cli_search_print (xmmsv_t *list, column_display_t *coldisp)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *entry;

	column_display_prepare (coldisp);
	column_display_print_header (coldisp);

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry (it, &entry)) {
		column_display_set_position (coldisp, xmmsv_list_iter_tell (it));
		enrich_mediainfo (entry);
		column_display_print (coldisp, entry);
		xmmsv_list_iter_next (it);
	}

	column_display_print_footer (coldisp);
	column_display_free (coldisp);
}

gboolean
cli_search (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsv_t *fetchval, *query, *ordered_query;
	column_display_t *coldisp;
	const gchar **order = NULL;
	const gchar *playlist_marker;
	const gchar **columns = NULL;
	const gchar *default_columns[] = { "id", "artist", "album", "title", NULL };

	if (!command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		return FALSE;
	}

	if (command_flag_stringlist_get (ctx, "columns", &columns)) {
		fetchval = xmmsv_make_stringlist ((gchar **) columns, -1);
	} else {
		fetchval = xmmsv_make_stringlist ((gchar **) default_columns, -1);
	}

	if (command_flag_stringlist_get (ctx, "order", &order)) {
		xmmsv_t *orderval = xmmsv_make_stringlist ((gchar **) order, -1);
		ordered_query = xmmsv_coll_add_order_operators (query, orderval);
		xmmsv_unref (orderval);
	} else {
		ordered_query = xmmsv_coll_apply_default_order (query);
	}

	playlist_marker = configuration_get_string (infos->config, "PLAYLIST_MARKER");

	if (columns != NULL) {
		coldisp = column_display_build (columns, playlist_marker, infos->cache->currpos);
	} else {
		coldisp = column_display_build (default_columns, playlist_marker, infos->cache->currpos);
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_infos, infos->sync, ordered_query, NULL, 0, 0, fetchval, NULL),
	                 FUNC_CALL_P (cli_search_print, XMMS_PREV_VALUE, coldisp));

	xmmsv_unref (ordered_query);
	xmmsv_unref (query);
	xmmsv_unref (fetchval);

	g_free (order);
	g_free (columns);

	return FALSE;
}

static void
coll_save (cli_infos_t *infos, xmmsv_t *coll,
           xmmsc_coll_namespace_t ns, const gchar *name, gboolean force)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	gboolean save = TRUE;

	if (!force) {
		res = xmmsc_coll_get (infos->sync, name, ns);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		if (xmmsv_is_type (val, XMMSV_TYPE_COLL)) {
			g_printf (_("Error: A collection already exists "
			            "with the target name!\n"));
			save = FALSE;
		}
		xmmsc_result_unref (res);
	}

	if (save) {
		XMMS_CALL (xmmsc_coll_save, infos->sync, coll, name, ns);
	}
}

static void
cli_coll_list_print (xmmsv_t *list)
{
	xmmsv_list_iter_t *it;
	const gchar *name;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_string (it, &name)) {
		g_printf ("  %s\n", name);
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_coll_list (cli_infos_t *infos, command_context_t *ctx)
{
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_list, infos->sync, XMMS_COLLECTION_NS_COLLECTIONS),
	                 FUNC_CALL_P (cli_coll_list_print, XMMS_PREV_VALUE));
	return FALSE;
}

/* Strings must be free manually */
static void
coll_name_split (const gchar *str, gchar **ns, gchar **name)
{
	gchar **v;

	v = g_strsplit (str, "/", 2);
	if (!v[0]) {
		*ns = NULL;
		*name = NULL;
	} else if (!v[1]) {
		*ns = g_strdup (XMMS_COLLECTION_NS_COLLECTIONS);
		*name = v[0];
	} else {
		*ns = v[0];
		*name = v[1];
	}

	g_free (v);
}

static GString *
coll_idlist_to_string (xmmsv_t *coll)
{
	xmmsv_list_iter_t *it;
	gint32 entry;
	gboolean first = TRUE;
	GString *s;

	s = g_string_new ("(");

	xmmsv_get_list_iter (xmmsv_coll_idlist_get (coll), &it);
	while (xmmsv_list_iter_entry_int (it, &entry)) {
		if (first) {
			first = FALSE;
		} else {
			g_string_append (s, ", ");
		}

		xmmsv_list_iter_entry_int (it, &entry);
		g_string_append_printf (s, "%d", entry);

		xmmsv_list_iter_next (it);
	}
	xmmsv_list_iter_explicit_destroy (it);

	g_string_append_c (s, ')');

	return s;
}

static void coll_dump (xmmsv_t *coll, guint level);

/* (from src/clients/cli/cmd_coll.c) */
static void
coll_dump_list (xmmsv_t *list, unsigned int level)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *v;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry (it, &v)) {
		coll_dump (v, level);
		xmmsv_list_iter_next (it);
	}

}

static void
coll_write_attribute (const char *key, xmmsv_t *val, gboolean *first)
{
	const char *str;
	if (xmmsv_get_string (val, &str)) {
		if (*first) {
			g_printf ("%s: %s", key, str);
			*first = FALSE;
		} else {
			g_printf (", %s: %s", key, str);
		}
	}
}

static void
coll_dump_attributes (xmmsv_t *attr, gchar *indent)
{
	gboolean first = TRUE;
	if (xmmsv_dict_get_size (attr) > 0) {
		g_printf ("%sAttributes: (", indent);
		xmmsv_dict_foreach (attr, (xmmsv_dict_foreach_func)coll_write_attribute, &first);
		g_printf (")\n");
	}
}

static void
coll_dump (xmmsv_t *coll, guint level)
{
	gint i;
	gchar *indent;

	const gchar *type;
	GString *idlist_str;

	indent = g_malloc ((level * 2) + 1);
	for (i = 0; i < level * 2; ++i) {
		indent[i] = ' ';
	}
	indent[i] = '\0';

	/* type */
	switch (xmmsv_coll_get_type (coll)) {
		case XMMS_COLLECTION_TYPE_REFERENCE:
			type = "Reference";
			break;
		case XMMS_COLLECTION_TYPE_UNIVERSE:
			type = "Universe";
			break;
		case XMMS_COLLECTION_TYPE_UNION:
			type = "Union";
			break;
		case XMMS_COLLECTION_TYPE_INTERSECTION:
			type = "Intersection";
			break;
		case XMMS_COLLECTION_TYPE_COMPLEMENT:
			type = "Complement";
			break;
		case XMMS_COLLECTION_TYPE_HAS:
			type = "Has";
			break;
		case XMMS_COLLECTION_TYPE_MATCH:
			type = "Match";
			break;
		case XMMS_COLLECTION_TYPE_TOKEN:
			type = "Token";
			break;
		case XMMS_COLLECTION_TYPE_EQUALS:
			type = "Equals";
			break;
		case XMMS_COLLECTION_TYPE_NOTEQUAL:
			type = "Not-equal";
			break;
		case XMMS_COLLECTION_TYPE_SMALLER:
			type = "Smaller";
			break;
		case XMMS_COLLECTION_TYPE_SMALLEREQ:
			type = "Smaller-equal";
			break;
		case XMMS_COLLECTION_TYPE_GREATER:
			type = "Greater";
			break;
		case XMMS_COLLECTION_TYPE_GREATEREQ:
			type = "Greater-equal";
			break;
		case XMMS_COLLECTION_TYPE_IDLIST:
			type = "Idlist";
			break;
		default:
			type = "Unknown Operator!";
			break;
	}

	g_printf ("%sType: %s\n", indent, type);

	/* Idlist */
	idlist_str = coll_idlist_to_string (coll);
	if (strcmp (idlist_str->str, "()") != 0) {
		g_printf ("%sIDs: %s\n", indent, idlist_str->str);
	}
	g_string_free (idlist_str, TRUE);

	/* Attributes */
	coll_dump_attributes (xmmsv_coll_attributes_get (coll), indent);

	/* Operands */
	coll_dump_list (xmmsv_coll_operands_get (coll), level + 1);
}

gboolean
cli_coll_show (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *collection, *name, *ns;

	if (!command_arg_longstring_get (ctx, 0, &collection)) {
		g_printf (_("Error: You must provide a collection!\n"));
		return FALSE;
	}

	coll_name_split (collection, &ns, &name);

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, infos->sync, name, ns),
	                 FUNC_CALL_P (coll_dump, XMMS_PREV_VALUE, 0));

	g_free (ns);
	g_free (name);
	g_free (collection);

	return FALSE;
}

gboolean
cli_coll_create (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsv_t *coll = NULL;
	gchar *ns, *name, *pattern = NULL;
	gboolean force = FALSE, empty = FALSE, copy;
	const gchar *collection, *fullname;

	if (!command_arg_string_get (ctx, 0, &fullname)) {
		g_printf (_("Error: You must provide a collection name!\n"));
		return FALSE;
	}

	command_flag_boolean_get (ctx, "empty", &empty);
	copy = command_flag_string_get (ctx, "collection", &collection);
	command_arg_longstring_get_escaped (ctx, 1, &pattern);

	if ((empty && copy) || (empty && pattern) || (copy && pattern)) {
		g_printf (_("Error: -e, -c and pattern are mutually exclusive!"));
		return FALSE;
	}

	command_flag_boolean_get (ctx, "force", &force);
	coll_name_split (fullname, &ns, &name);

	if (pattern) {
		if (!xmmsc_coll_parse (pattern, &coll)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
		}
	} else if (copy) {
		xmmsc_result_t *res;
		gchar *from_ns, *from_name;

		coll_name_split (collection, &from_ns, &from_name);

		/* get collection to copy from */
		res = xmmsc_coll_get (infos->sync, from_name, from_ns);
		xmmsc_result_wait (res);
		coll = xmmsc_result_get_value (res);

		if (xmmsv_is_type (coll, XMMSV_TYPE_COLL)) {
			xmmsv_ref (coll);
		} else {
			g_printf (_("Error: cannot find collection to copy\n"));
		}

		xmmsc_result_unref (res);
		g_free (from_ns);
		g_free (from_name);
	} else if (empty) {
		xmmsv_t *univ;

		/* empty collection == NOT 'All Media' */
		univ = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);

		coll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_COMPLEMENT);
		xmmsv_coll_add_operand (coll, univ);
		xmmsv_unref (univ);
	} else {
		coll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	}

	if (coll) {
		coll_save (infos, coll, ns, name, force);
		xmmsv_unref (coll);
	}

	g_free (pattern);
	g_free (ns);
	g_free (name);

	return FALSE;
}

gboolean
cli_coll_rename (cli_infos_t *infos, command_context_t *ctx)
{
	gboolean force;
	gchar *from_ns, *to_ns, *from_name, *to_name;
	const gchar *oldname, *newname;

	if (!command_flag_boolean_get (ctx, "force", &force)) {
		force = FALSE;
	}

	if (!command_arg_string_get (ctx, 0, &oldname)) {
		g_printf (_("Error: failed to read collection name!\n"));
		return FALSE;
	}

	if (!command_arg_string_get (ctx, 1, &newname)) {
		g_printf (_("Error: failed to read collection new name!\n"));
		return FALSE;
	}

	coll_name_split (oldname, &from_ns, &from_name);
	coll_name_split (newname, &to_ns, &to_name);

	if (g_strcmp0 (from_ns, to_ns)) {
		g_printf ("Error: collections namespaces can't be different!\n");
	} else {
		if (force) {
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, infos->sync, newname, to_ns),
			                 XMMS_CALL_P (xmmsc_coll_remove, infos->sync, newname, to_ns));
		}
		XMMS_CALL (xmmsc_coll_rename, infos->sync, oldname, newname, to_ns);
	}

	g_free (from_ns);
	g_free (from_name);
	g_free (to_ns);
	g_free (to_name);

	return FALSE;
}

gboolean
cli_coll_remove (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *collection, *name, *ns;

	if (!command_arg_longstring_get (ctx, 0, &collection)) {
		g_printf (_("Error: failed to read the collection name!\n"));
		return FALSE;
	}

	coll_name_split (collection, &ns, &name);

	XMMS_CALL (xmmsc_coll_remove, infos->sync, name, ns);

	g_free (ns);
	g_free (name);
	g_free (collection);

	return FALSE;
}

static void
configure_collection (xmmsv_t *val, cli_infos_t *infos,
                      const gchar *ns, const gchar *name,
                      const gchar *attrname, const gchar *attrvalue)
{
	xmmsv_coll_attribute_set_string (val, attrname, attrvalue);
	coll_save (infos, val, ns, name, TRUE);
}

static void
coll_print_attributes (const char *key, xmmsv_t *val, void *udata)
{
	const char *value;

	if (xmmsv_get_string (val, &value)) {
		g_printf ("[%s] %s\n", key, value);
	}
}

static void
collection_print_config (xmmsv_t *coll, const gchar *attrname)
{
	const gchar *attrvalue;

	if (attrname == NULL) {
		xmmsv_dict_foreach (xmmsv_coll_attributes_get (coll),
		                    coll_print_attributes, NULL);
	} else {
		if (xmmsv_coll_attribute_get_string (coll, attrname, &attrvalue)) {
			g_printf ("[%s] %s\n", attrname, attrvalue);
		} else {
			g_printf (_("Invalid attribute!\n"));
		}
	}
}

gboolean
cli_coll_config (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *name, *ns;
	const gchar *collection, *attrname, *attrvalue;

	if (!command_arg_string_get (ctx, 0, &collection)) {
		g_printf (_("Error: you must provide a collection!\n"));
		return FALSE;
	}

	coll_name_split (collection, &ns, &name);

	if (!command_arg_string_get (ctx, 1, &attrname)) {
		attrname = NULL;
		attrvalue = NULL;
	} else if (!command_arg_string_get (ctx, 2, &attrvalue)) {
		attrvalue = NULL;
	}

	if (attrvalue) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, infos->sync, name, ns),
		                 FUNC_CALL_P (configure_collection, XMMS_PREV_VALUE, infos, ns, name, attrname, attrvalue));
	} else {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, infos->sync, name, ns),
		                 FUNC_CALL_P (collection_print_config, XMMS_PREV_VALUE, attrname));
	}

	g_free (ns);
	g_free (name);

	return FALSE;
}
