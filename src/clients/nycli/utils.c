/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#include "utils.h"
#include "status.h"

#include "cli_infos.h"
#include "cli_cache.h"
#include "column_display.h"

static void coll_int_attribute_set (xmmsc_coll_t *coll, const char *key, gint value);
static xmmsc_coll_t *coll_make_reference (const char *name, xmmsc_coll_namespace_t ns);
static void coll_copy_attributes (const char *key, const char *value, void *udata);
static void coll_print_attributes (const char *key, const char *value, void *udata);
static xmmsc_coll_t *coll_copy_retype (xmmsc_coll_t *coll, xmmsc_coll_type_t type);

static void pl_print_config (xmmsc_coll_t *coll, const char *name);

static void id_print_info (xmmsc_result_t *res, guint id, gchar *source);

static gint compare_uint (gconstpointer a, gconstpointer b, gpointer userdata);

typedef enum {
	IDLIST_CMD_NONE = 0,
	IDLIST_CMD_REHASH,
	IDLIST_CMD_REMOVE
} idlist_command_t;

/* Dumps a propdict on stdout */
static void
dict_dump (const gchar *source, xmmsv_t *val, void *udata)
{
	xmmsv_type_t type;

	const gchar **keyfilter = (const gchar **) udata;
	const gchar *key = (const gchar *) keyfilter[0];
	const gchar *filter = (const gchar *) keyfilter[1];

	if (filter && strcmp (filter, source) != 0) {
		return;
	}

	type = xmmsv_get_type (val);

	switch (type) {
	case XMMSV_TYPE_UINT32:
	{
		guint value;
		xmmsv_get_uint (val, &value);
		g_printf (_("[%s] %s = %u\n"), source, key, value);
	}
	case XMMSV_TYPE_INT32:
	{
		gint value;
		xmmsv_get_int (val, &value);
		g_printf (_("[%s] %s = %u\n"), source, key, value);
		break;
	}
	case XMMSV_TYPE_STRING:
	{
		const gchar *value;
		xmmsv_get_string (val, &value);
		/* FIXME: special handling for url, guess charset, see common.c:print_entry */
		g_printf (_("[%s] %s = %s\n"), source, key, value);
		break;
	}
	case XMMSV_TYPE_LIST:
		g_printf (_("[%s] %s = <list>\n"), source, key);
		break;
	case XMMSV_TYPE_DICT:
		g_printf (_("[%s] %s = <dict>\n"), source, key);
		break;
	case XMMSV_TYPE_COLL:
		g_printf (_("[%s] %s = <coll>\n"), source, key);
		break;
	case XMMSV_TYPE_BIN:
		g_printf (_("[%s] %s = <bin>\n"), source, key);
		break;
	case XMMSV_TYPE_END:
		g_printf (_("[%s] %s = <end>\n"), source, key);
		break;
	case XMMSV_TYPE_NONE:
		g_printf (_("[%s] %s = <none>\n"), source, key);
		break;
	case XMMSV_TYPE_ERROR:
		g_printf (_("[%s] %s = <error>\n"), source, key);
		break;
	}
}

static void
propdict_dump (const gchar *key, xmmsv_t *src_dict, void *udata)
{
	const gchar *keyfilter[] = {key, udata};

	xmmsv_dict_foreach (src_dict, dict_dump, (void *) keyfilter);
}

/* Compare two uint like strcmp */
static gint
compare_uint (gconstpointer a, gconstpointer b, gpointer userdata)
{
	guint va = *((guint *)a);
	guint vb = *((guint *)b);

	if (va < vb) {
		return -1;
	} else if (va > vb) {
		return 1;
	} else {
		return 0;
	}
}


/* Dummy callback that resets the action status as finished. */
void
done (xmmsc_result_t *res, cli_infos_t *infos)
{
	const gchar *err;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (xmmsv_get_error (val, &err)) {
		g_printf (_("Server error: %s\n"), err);
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
coldisp_finalize (xmmsc_result_t *res, column_display_t *coldisp)
{
	column_display_print_footer (coldisp);
	column_display_free (coldisp);
}

void
tickle (xmmsc_result_t *res, cli_infos_t *infos)
{
	xmmsc_result_t *res2;

	const gchar *err;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		res2 = xmmsc_playback_tickle (infos->sync);
		xmmsc_result_wait (res2);
		done (res2, infos);
	} else {
		g_printf (_("Server error: %s\n"), err);
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
list_plugins (cli_infos_t *infos, xmmsc_result_t *res)
{
	const gchar *name, *desc, *err;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_list_iter_t *it;

		xmmsv_get_list_iter (val, &it);

		for (xmmsv_list_iter_first (it);
		     xmmsv_list_iter_valid (it);
		     xmmsv_list_iter_next (it)) {
			xmmsv_t *elem;

			xmmsv_list_iter_entry (it, &elem);
			xmmsv_get_dict_entry_string (elem, "shortname", &name);
			xmmsv_get_dict_entry_string (elem, "description", &desc);

			g_printf ("%s - %s\n", name, desc);
		}
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);

	cli_infos_loop_resume (infos);
}

static void
print_server_stats (xmmsc_result_t *res)
{
	gint uptime;
	const gchar *version, *err;

	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_get_dict_entry_string (val, "version", &version);
		xmmsv_get_dict_entry_int (val, "uptime", &uptime);
		g_printf ("uptime = %d\n"
		          "version = %s\n", uptime, version);
	} else {
		g_printf ("Server error: %s\n", err);
	}
}

void
print_stats (cli_infos_t *infos, xmmsc_result_t *res)
{
	print_server_stats (res);

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

static void
print_config_entry (const gchar *confname, xmmsv_t *val, void *udata)
{
	xmmsv_type_t type;

	type = xmmsv_get_type (val);

	switch (type) {
	case XMMSV_TYPE_STRING:
	{
		const gchar *confval;
		xmmsv_get_string (val, &confval);
		g_printf ("%s = %s\n", confname, confval);
		break;
	}
	case XMMSV_TYPE_INT32:
	{
		int confval;
		xmmsv_get_int (val, &confval);
		g_printf ("%s = %d\n", confname, confval);
		break;
	}
	case XMMSV_TYPE_UINT32:
	{
		guint confval;
		xmmsv_get_uint (val, &confval);
		g_printf ("%s = %u\n", confname, confval);
		break;
	}
	default:
		break;
	}
}

void
print_config (cli_infos_t *infos, xmmsc_result_t *res, gchar *confname)
{
	const gchar *confval;
	xmmsv_t *val;

	if (confname == NULL) {
		res = xmmsc_configval_list (infos->sync);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		xmmsv_dict_foreach (val, print_config_entry, NULL);
	} else {
		res = xmmsc_configval_get (infos->sync, confname);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		xmmsv_get_string (val, &confval);
		print_config_entry (confname, val, NULL);
	}

	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

void
print_property (cli_infos_t *infos, xmmsc_result_t *res, guint id,
                gchar *source, gchar *property)
{
	if (property == NULL) {
		id_print_info (res, id, source);
	} else {
		/* FIXME(g): print if given an specific property */
	}

	cli_infos_loop_resume (infos);
}

/* Apply operation to an idlist */
void
apply_ids (cli_infos_t *infos, xmmsc_result_t *res, idlist_command_t cmd)
{
	const gchar *err;
	xmmsc_result_t *cmdres;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_list_iter_t *it;

		xmmsv_get_list_iter (val, &it);

		for (xmmsv_list_iter_first (it);
		     xmmsv_list_iter_valid (it);
		     xmmsv_list_iter_next (it)) {
			xmmsv_t *entry;
			guint id;

			xmmsv_list_iter_entry (it, &entry);

			if (xmmsv_get_uint (entry, &id)) {
				switch (cmd) {
				case IDLIST_CMD_REHASH:
					cmdres = xmmsc_medialib_rehash (infos->sync, id);
					break;
				case IDLIST_CMD_REMOVE:
					cmdres = xmmsc_medialib_remove_entry (infos->sync, id);
					break;
				default:
					break;
				}
				xmmsc_result_wait (cmdres);
				xmmsc_result_unref (cmdres);
			}
		}
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

void
remove_ids (cli_infos_t *infos, xmmsc_result_t *res)
{
	apply_ids (infos, res, IDLIST_CMD_REMOVE);
}

void
rehash_ids (cli_infos_t *infos, xmmsc_result_t *res)
{
	apply_ids (infos, res, IDLIST_CMD_REHASH);
}

static void
print_volume_entry (const gchar *key, xmmsv_t *val, void *udata)
{
	gchar *channel = udata;
	guint value;

	if (!udata || !strcmp (key, channel)) {
		xmmsv_get_uint (val, &value);
		g_printf (_("%s = %u\n"), key, value);
	}
}

void
print_volume (xmmsc_result_t *res, cli_infos_t *infos, gchar *channel)
{
	xmmsv_t *val;
	const gchar *err;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_dict_foreach (val, print_volume_entry, channel);
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

static void
dict_keys (const gchar *key, xmmsv_t *val, void *udata)
{
	GList **list = udata;

	*list = g_list_prepend (*list, g_strdup (key));
}

void
set_volume (cli_infos_t *infos, gchar *channel, gint volume)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	GList *it, *channels = NULL;

	if (!channel) {
		/* get all channels */
		res = xmmsc_playback_volume_get (infos->sync);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		xmmsv_dict_foreach (val, dict_keys, &channels);
		xmmsc_result_unref (res);
	} else {
		channels = g_list_prepend (channels, g_strdup (channel));
	}

	/* set volumes for channels in list */
	for (it = g_list_first (channels); it != NULL; it = g_list_next (it)) {
		res = xmmsc_playback_volume_set (infos->sync, it->data, volume);
		xmmsc_result_wait (res);
		xmmsc_result_unref (res);

		/* free channel string */
		g_free (it->data);
	}

	g_list_free (channels);

	cli_infos_loop_resume (infos);
}

void
status_mode (cli_infos_t *infos, gchar *format, gint refresh)
{
	status_entry_t *status;

	status = status_init (format, refresh);

	if (refresh > 0) {
		g_printf (_("\n"
		            "   (n) next song\n"
		            "   (p) previous song\n"
		            "   (t) toggle playback\n"
		            "   (ENTER) exit status mode\n\n"));
		cli_infos_status_mode (infos, status);
	} else {
		status_update_all (infos, status);
		status_print_entry (status);
		status_free (status);
		cli_infos_loop_resume (infos);
	}
}

static void
id_print_info (xmmsc_result_t *res, guint id, gchar *source)
{
	xmmsv_t *val;
	const gchar *err;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_dict_foreach (val, propdict_dump, source);
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (res);
}

void
list_print_info (xmmsc_result_t *res, cli_infos_t *infos)
{
	xmmsc_result_t *infores = NULL;
	xmmsv_t *val;
	const gchar *err;
	guint id;
	gboolean first = true;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_list_iter_t *it;

		xmmsv_get_list_iter (val, &it);
		while (xmmsv_list_iter_valid (it)) {
			xmmsv_t *entry;

			xmmsv_list_iter_entry (it, &entry);
			if (xmmsv_get_uint (entry, &id)) {
				infores = xmmsc_medialib_get_info (infos->sync, id);
				xmmsc_result_wait (infores);

				if (!first) {
					g_printf ("\n");
				} else {
					first = false;
				}
				id_print_info (infores, id, NULL);
			}
			xmmsv_list_iter_next (it);
		}

	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
list_print_row (xmmsc_result_t *res, column_display_t *coldisp)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	cli_infos_t *infos = column_display_infos_get (coldisp);
	xmmsc_result_t *infores = NULL;
	xmmsv_t *val, *info;

	const gchar *err;
	guint id;
	gint i = 0;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_list_iter_t *it;
		column_display_prepare (coldisp);
		column_display_print_header (coldisp);
		xmmsv_get_list_iter (val, &it);
		while (xmmsv_list_iter_valid (it)) {
			xmmsv_t *entry;
			xmmsv_list_iter_entry (it, &entry);
			/* FIXME: check this! seems strange /greafine */
			if (infores) {
				xmmsc_result_unref (infores); /* unref previous infores */
			}
			if (xmmsv_get_uint (entry, &id)) {
				infores = xmmsc_medialib_get_info (infos->sync, id);
				xmmsc_result_wait (infores);
				info = xmmsv_propdict_to_dict (xmmsc_result_get_value (infores),
				                               NULL);
				column_display_print (coldisp, info);
				xmmsv_unref (info);
			}
			xmmsv_list_iter_next (it);
			i++;
		}
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	column_display_print_footer (coldisp);
	column_display_free (coldisp);
	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

void
coll_rename (cli_infos_t *infos, gchar *oldname, gchar *newname,
             xmmsc_coll_namespace_t ns, gboolean force)
{
	xmmsc_result_t *res;

	if (force) {
		res = xmmsc_coll_remove (infos->sync, newname, ns);
		xmmsc_result_wait (res);
		/* FIXME(g): check something? */
		xmmsc_result_unref (res);
	}

	res = xmmsc_coll_rename (infos->sync, oldname, newname, ns);
	xmmsc_result_wait (res);
	done (res, infos);
}

void
coll_save (cli_infos_t *infos, xmmsc_coll_t *coll,
           xmmsc_coll_namespace_t ns, gchar *name, gboolean force)
{
	xmmsc_result_t *res;
	xmmsv_t *val;
	gboolean save = TRUE;

	if (!force) {
		xmmsc_coll_t *exists;
		res = xmmsc_coll_get (infos->sync, name, ns);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		if (xmmsv_get_collection (val, &exists)) {
			g_printf (_("Error: A collection already exists "
			            "with the target name!\n"));
			save = FALSE;
		}
		xmmsc_result_unref (res);
	}

	if (save) {
		res = xmmsc_coll_save (infos->sync, coll, name, ns);
		xmmsc_result_wait (res);
		done (res, infos);
	} else {
		cli_infos_loop_resume (infos);
	}
}

/* (from src/clients/cli/common.c) */
static void
print_info (const gchar *fmt, ...)
{
	gchar buf[8096];
	va_list ap;

	va_start (ap, fmt);
	g_vsnprintf (buf, 8096, fmt, ap);
	va_end (ap);

	g_printf ("%s\n", buf);
}

/* Produce a GString from the idlist of the collection.
   (must be freed manually!)
   (from src/clients/cli/cmd_coll.c) */
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

/* Dump the structure of the collection as a string
   (from src/clients/cli/cmd_coll.c) */
static void
coll_dump (xmmsc_coll_t *coll, guint level)
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
		xmmsc_coll_attribute_get (coll, "field",  &attr1);
		xmmsc_coll_attribute_get (coll, "value", &attr2);
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
/*  */

void
coll_show (cli_infos_t *infos, xmmsc_result_t *res)
{
	const gchar *err;
	xmmsc_coll_t *coll;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_get_collection (val, &coll);
		coll_dump (coll, 0);
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

static void
print_collections_list (xmmsc_result_t *res, cli_infos_t *infos,
                        gchar *mark, gboolean all)
{
	const gchar *s, *err;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err)) {
		xmmsv_list_iter_t *it;
		xmmsv_get_list_iter (val, &it);
		while (xmmsv_list_iter_valid (it)) {
			xmmsv_t *entry;
			xmmsv_list_iter_entry (it, &entry);
			/* Skip hidden playlists if all is FALSE*/
			if (xmmsv_get_string (entry, &s) && ((*s != '_') || all)) {
				/* Highlight active playlist */
				if (mark && strcmp (s, mark) == 0) {
					g_printf ("* %s\n", s);
				} else {
					g_printf ("  %s\n", s);
				}
			}
			xmmsv_list_iter_next (it);
		}
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (res);
}

void
list_print_collections (xmmsc_result_t *res, cli_infos_t *infos)
{
	print_collections_list (res, infos, NULL, TRUE);
}

void
list_print_playlists (xmmsc_result_t *res, cli_infos_t *infos, gboolean all)
{
	print_collections_list (res, infos,
	                        infos->cache->active_playlist_name, all);
}

/* Abstract jump, use inc to choose the direction. */
static void
list_jump_rel (xmmsc_result_t *res, cli_infos_t *infos, gint inc)
{
	guint i;
	guint id;
	xmmsc_result_t *jumpres = NULL;
	xmmsv_t *val;
	const gchar *err;

	gint currpos;
	gint plsize;
	GArray *playlist;

	currpos = infos->cache->currpos;
	plsize = infos->cache->active_playlist->len;
	playlist = infos->cache->active_playlist;

	val = xmmsc_result_get_value (res);

	if (!xmmsv_get_error (val, &err) && xmmsv_is_list (val)) {
		xmmsv_list_iter_t *it;

		xmmsv_get_list_iter (val, &it);

		inc += plsize; /* magic trick so we can loop in either direction */

		/* Loop on the playlist */
		for (i = (currpos + inc) % plsize; i != currpos; i = (i + inc) % plsize) {

			/* Loop on the matched media */
			for (xmmsv_list_iter_first (it);
			     xmmsv_list_iter_valid (it);
			     xmmsv_list_iter_next (it)) {
				xmmsv_t *entry;

				xmmsv_list_iter_entry (it, &entry);

				/* If both match, jump! */
				if (xmmsv_get_uint (entry, &id)
				    && g_array_index (playlist, guint, i) == id) {
					jumpres = xmmsc_playlist_set_next (infos->sync, i);
					xmmsc_result_wait (jumpres);
					tickle (jumpres, infos);
					goto finish;
				}
			}
		}
	}

	finish:

	/* No matching media found, don't jump */
	if (!jumpres) {
		g_printf (_("No media matching the pattern in the playlist!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
list_jump_back (xmmsc_result_t *res, cli_infos_t *infos)
{
	list_jump_rel (res, infos, -1);
}

void
list_jump (xmmsc_result_t *res, cli_infos_t *infos)
{
	list_jump_rel (res, infos, 1);
}

void
playback_play (cli_infos_t *infos)
{
	xmmsc_result_t *res;

	res = xmmsc_playback_start (infos->sync);
	xmmsc_result_wait (res);
	done (res, infos);
}

void
playback_pause (cli_infos_t *infos)
{
	xmmsc_result_t *res;

	res = xmmsc_playback_pause (infos->sync);
	xmmsc_result_wait (res);
	done (res, infos);
}

void
playback_toggle (cli_infos_t *infos)
{
	guint status;

	status = infos->cache->playback_status;

	if (status == XMMS_PLAYBACK_STATUS_PLAY) {
		playback_pause (infos);
	} else {
		playback_play (infos);
	}
}

void
set_next_rel (cli_infos_t *infos, gint offset)
{
	xmmsc_result_t *res;

	res = xmmsc_playlist_set_next_rel (infos->sync, offset);
	xmmsc_result_wait (res);
	tickle (res, infos);
}

void
add_pls (xmmsc_result_t *plsres, cli_infos_t *infos,
          gchar *playlist, gint pos)
{
	xmmsc_result_t *res;
	xmmsc_coll_t *coll;
	xmmsv_t *val;
	const char *err;

	val = xmmsc_result_get_value (plsres);

	if (!xmmsv_get_error (val, &err) &&
	    xmmsv_get_collection (val, &coll)) {
		res = xmmsc_playlist_add_idlist (infos->sync, playlist, coll);
		xmmsc_result_wait (res);
		xmmsc_result_unref (res);
	} else {
		g_printf (_("Server error: %s\n"), err);
	}

	xmmsc_result_unref (plsres);
}

void
add_list (xmmsc_result_t *matching, cli_infos_t *infos,
          gchar *playlist, gint pos)

{
	/* FIXME: w00t at code copy-paste, please modularize */
	xmmsc_result_t *insres;
	guint id;
	gint offset;
	const gchar *err;

	xmmsv_t *val;

	val = xmmsc_result_get_value (matching);

	offset = 0;

	if (xmmsv_get_error (val, &err) || !xmmsv_is_list (val)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else {
		xmmsv_list_iter_t *it;

		xmmsv_get_list_iter (val, &it);

		/* Loop on the matched media */
		for (xmmsv_list_iter_first (it);
		     xmmsv_list_iter_valid (it);
		     xmmsv_list_iter_next (it)) {
			xmmsv_t *entry;

			xmmsv_list_iter_entry (it, &entry);

			if (xmmsv_get_uint (entry, &id)) {
				insres = xmmsc_playlist_insert_id (infos->sync, playlist,
				                                   pos + offset, id);
				xmmsc_result_wait (insres);
				xmmsc_result_unref (insres);
				offset++;
			}
		}
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matching);
}

void
move_entries (xmmsc_result_t *matching, cli_infos_t *infos,
                 gchar *playlist, gint pos)
{
	xmmsc_result_t *movres, *lisres;
	guint id, curr;
	gint inc;
	gboolean up;
	GTree *list;

	const gchar *err;
	xmmsv_t *val, *lisval;

	val = xmmsc_result_get_value (matching);

	if (xmmsv_get_error (val, &err) || !xmmsv_is_list (val)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else {
		xmmsv_list_iter_t *it;
		xmmsv_t *entry;

		lisres = xmmsc_playlist_list_entries (infos->sync, playlist);
		xmmsc_result_wait (lisres);

		if (xmmsv_get_error (lisval, &err) || !xmmsv_is_list (lisval)) {
			g_printf (_("Error retrieving playlist entries\n"));
			goto finish;
		}

		list = g_tree_new_full (compare_uint, NULL, g_free, NULL);

		/* store matching mediaids in a tree (faster lookup) */
		xmmsv_get_list_iter (val, &it);
		for (xmmsv_list_iter_first (it);
		     xmmsv_list_iter_valid (it);
		     xmmsv_list_iter_next (it)) {

			xmmsv_list_iter_entry (it, &entry);

			if (xmmsv_get_uint (entry, &id)) {
				guint *tid;
				tid = g_new (guint, 1);
				*tid = id;
				g_tree_insert (list, tid, tid);
			}
		}

		/* move matched playlist items */
		curr = 0;
		inc = 0;
		up = TRUE;
		xmmsv_get_list_iter (lisval, &it);
		for (xmmsv_list_iter_first (it);
		     xmmsv_list_iter_valid (it);
		     xmmsv_list_iter_next (it)) {

			xmmsv_list_iter_entry (it, &entry);

			if (curr == pos) {
				up = FALSE;
			}
			if (xmmsv_get_uint (entry, &id) &&
			    g_tree_lookup (list, &id) != NULL) {
				if (up) {
					/* moving forward */
					movres = xmmsc_playlist_move_entry (infos->sync, playlist,
					                                    curr - inc, pos - 1);
				} else {
					/* moving backward */
					movres = xmmsc_playlist_move_entry (infos->sync, playlist,
					                                    curr, pos + inc);
				}
				xmmsc_result_wait (movres);
				xmmsc_result_unref (movres);
				inc++;
			}
			curr++;
		}
		g_tree_destroy (list);
	}

    finish:

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matching);
	xmmsc_result_unref (lisres);
}

void
remove_cached_list (xmmsc_result_t *matching, cli_infos_t *infos)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	xmmsc_result_t *rmres;
	guint plid, id;
	gint plsize;
	GArray *playlist;
	gint i;

	const gchar *err;
	xmmsv_t *val;

	val = xmmsc_result_get_value (matching);

	plsize = infos->cache->active_playlist->len;
	playlist = infos->cache->active_playlist;

	if (xmmsv_get_error (val, &err) || !xmmsv_is_list (val)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else {
		xmmsv_list_iter_t *it;

		xmmsv_get_list_iter (val, &it);

		/* Loop on the playlist (backward, easier to remove) */
		for (i = plsize - 1; i >= 0; i--) {
			plid = g_array_index (playlist, guint, i);

			/* Loop on the matched media */
			for (xmmsv_list_iter_first (it);
			     xmmsv_list_iter_valid (it);
			     xmmsv_list_iter_next (it)) {
				xmmsv_t *entry;

				xmmsv_list_iter_entry (it, &entry);

				/* If both match, remove! */
				if (xmmsv_get_uint (entry, &id) && plid == id) {
					rmres = xmmsc_playlist_remove_entry (infos->sync, NULL, i);
					xmmsc_result_wait (rmres);
					xmmsc_result_unref (rmres);
					break;
				}
			}
		}
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matching);
}

void
remove_list (xmmsc_result_t *matchres, xmmsc_result_t *plistres,
                cli_infos_t *infos, gchar *playlist)
{
	/* FIXME: w00t at code copy-paste, please modularize */
	xmmsc_result_t *rmres;
	guint plid, id, i;
	gint offset;

	const gchar *err;
	xmmsv_t *matchval, *plistval;

	matchval = xmmsc_result_get_value (matchres);
	plistval = xmmsc_result_get_value (plistres);

	if (xmmsv_get_error (matchval, &err) || !xmmsv_is_list (matchval)) {
		g_printf (_("Error retrieving the media matching the pattern!\n"));
	} else if (xmmsv_get_error (plistval, &err) || !xmmsv_is_list (plistval)) {
		g_printf (_("Error retrieving the playlist!\n"));
	} else {
		xmmsv_list_iter_t *matchit, *plistit;

		/* FIXME: Can we use a GList to remove more safely in the rev order? */
		offset = 0;
		i = 0;

		xmmsv_get_list_iter (matchval, &matchit);
		xmmsv_get_list_iter (plistval, &plistit);

		/* Loop on the playlist */
		for (xmmsv_list_iter_first (plistit);
		     xmmsv_list_iter_valid (plistit);
		     xmmsv_list_iter_next (plistit)) {
			xmmsv_t *plist_entry;

			xmmsv_list_iter_entry (plistit, &plist_entry);

			if (!xmmsv_get_uint (plist_entry, &plid)) {
				plid = 0;  /* failed to get id, should not happen */
			}

			/* Loop on the matched media */
			for (xmmsv_list_iter_first (matchit);
			     xmmsv_list_iter_valid (matchit);
			     xmmsv_list_iter_next (matchit)) {
				xmmsv_t *match_entry;

				xmmsv_list_iter_entry (matchit, &match_entry);

				/* If both match, jump! */
				if (xmmsv_get_uint (match_entry, &id) && plid == id) {
					rmres = xmmsc_playlist_remove_entry (infos->sync, playlist,
					                                     i - offset);
					xmmsc_result_wait (rmres);
					xmmsc_result_unref (rmres);
					offset++;
					break;
				}
			}

			i++;
		}
	}

	cli_infos_loop_resume (infos);
	xmmsc_result_unref (matchres);
	xmmsc_result_unref (plistres);
}

void
copy_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist)
{
	xmmsc_result_t *saveres;
	xmmsc_coll_t *coll;

	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (xmmsv_get_collection (val, &coll)) {
		saveres = xmmsc_coll_save (infos->sync, coll, playlist,
		                           XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_wait (saveres);
		done (saveres, infos);
	} else {
		g_printf (_("Cannot find the playlist to copy!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void configure_collection (xmmsc_result_t *res, cli_infos_t *infos,
                           gchar *ns, gchar *name,
                           gchar *attrname, gchar *attrvalue)
{
	xmmsc_coll_t *coll;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (xmmsv_get_collection (val, &coll)) {
		xmmsc_coll_attribute_set (coll, attrname, attrvalue);
		coll_save (infos, coll, ns, name, TRUE);
	} else {
		g_printf (_("Invalid collection!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
configure_playlist (xmmsc_result_t *res, cli_infos_t *infos, gchar *playlist,
                    gint history, gint upcoming, xmmsc_coll_type_t type,
                    gchar *input)
{
	xmmsc_result_t *saveres;
	xmmsc_coll_t *coll;
	xmmsc_coll_t *newcoll;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (xmmsv_get_collection (val, &coll)) {
		if (type >= 0 && xmmsc_coll_get_type (coll) != type) {
			newcoll = coll_copy_retype (coll, type);
			xmmsc_coll_unref (coll);
			coll = newcoll;
		}
		if (history >= 0) {
			coll_int_attribute_set (coll, "history", history);
		}
		if (upcoming >= 0) {
			coll_int_attribute_set (coll, "upcoming", upcoming);
		}
		if (input) {
			/* Replace previous operand. */
			newcoll = coll_make_reference (input, XMMS_COLLECTION_NS_COLLECTIONS);
			xmmsc_coll_operand_list_clear (coll);
			xmmsc_coll_add_operand (coll, newcoll);
			xmmsc_coll_unref (newcoll);
		}

		saveres = xmmsc_coll_save (infos->sync, coll, playlist,
		                           XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_wait (saveres);
		done (saveres, infos);
	} else {
		g_printf (_("Cannot find the playlist to configure!\n"));
		cli_infos_loop_resume (infos);
	}

	xmmsc_result_unref (res);
}

void
collection_print_config (xmmsc_result_t *res, cli_infos_t *infos,
                         gchar *attrname)
{
	xmmsc_coll_t *coll;
	gchar *attrvalue;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (xmmsv_get_collection (val, &coll)) {
		if (attrname == NULL) {
			xmmsc_coll_attribute_foreach (coll,
			                              coll_print_attributes, NULL);
		} else {
			if (xmmsc_coll_attribute_get (coll, attrname, &attrvalue)) {
				coll_print_attributes (attrname, attrvalue, NULL);
			} else {
				g_printf (_("Invalid attribute!\n"));
			}
		}
	} else {
		g_printf (_("Invalid collection!\n"));
	}

	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

void
playlist_print_config (xmmsc_result_t *res, cli_infos_t *infos,
                       gchar *playlist)
{
	xmmsc_coll_t *coll;
	xmmsv_t *val;

	val = xmmsc_result_get_value (res);

	if (xmmsv_get_collection (val, &coll)) {
		pl_print_config (coll, playlist);
	} else {
		g_printf (_("Invalid playlist!\n"));
	}

	cli_infos_loop_resume (infos);

	xmmsc_result_unref (res);
}

gboolean
playlist_exists (cli_infos_t *infos, gchar *playlist)
{
	gboolean retval = FALSE;
	xmmsc_result_t *res;
	xmmsv_t *val;

	res = xmmsc_coll_get (infos->sync, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);

	if (!xmmsv_is_error (val)) {
		retval = TRUE;
	}

	xmmsc_result_unref (res);

	return retval;
}

static void
coll_int_attribute_set (xmmsc_coll_t *coll, const char *key, gint value)
{
	gchar buf[MAX_INT_VALUE_BUFFER_SIZE + 1];

	g_snprintf (buf, MAX_INT_VALUE_BUFFER_SIZE, "%d", value);
	xmmsc_coll_attribute_set (coll, key, buf);
}

static xmmsc_coll_t *
coll_make_reference (const char *name, xmmsc_coll_namespace_t ns)
{
	xmmsc_coll_t *ref;

	ref = xmmsc_coll_new (XMMS_COLLECTION_TYPE_REFERENCE);
	xmmsc_coll_attribute_set (ref, "reference", name);
	xmmsc_coll_attribute_set (ref, "namespace", ns);

	return ref;
}

static void
coll_copy_attributes (const char *key, const char *value, void *udata)
{
	xmmsc_coll_attribute_set ((xmmsc_coll_t *) udata, key, value);
}

static void
coll_print_attributes (const char *key, const char *value, void *udata)
{
	g_printf ("[%s] %s\n", key, value);
}

static xmmsc_coll_t *
coll_copy_retype (xmmsc_coll_t *coll, xmmsc_coll_type_t type)
{
	xmmsc_coll_t *copy;
	gint idlistsize;
	gint i;
	guint id;

	copy = xmmsc_coll_new (type);

	idlistsize = xmmsc_coll_idlist_get_size (coll);
	for (i = 0; i < idlistsize; i++) {
		xmmsc_coll_idlist_get_index (coll, i, &id);
		xmmsc_coll_idlist_append (copy, id);
	}

	xmmsc_coll_attribute_foreach (coll, coll_copy_attributes, copy);

	return copy;
}

static void
pl_print_config (xmmsc_coll_t *coll, const char *name)
{
	xmmsc_coll_t *op;
	xmmsc_coll_type_t type;
	gchar *upcoming = NULL;
	gchar *history = NULL;
	gchar *input = NULL;
	gchar *input_ns = NULL;

	type = xmmsc_coll_get_type (coll);

	xmmsc_coll_attribute_get (coll, "upcoming", &upcoming);
	xmmsc_coll_attribute_get (coll, "history", &history);

	g_printf (_("name: %s\n"), name);

	switch (type) {
	case XMMS_COLLECTION_TYPE_IDLIST:
		g_printf (_("type: list\n"));
		break;
	case XMMS_COLLECTION_TYPE_QUEUE:
		g_printf (_("type: queue\n"));
		g_printf (_("history: %s\n"), history);
		break;
	case XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		xmmsc_coll_operand_list_first (coll);
		if (xmmsc_coll_operand_list_entry (coll, &op)) {
			xmmsc_coll_attribute_get (op, "reference", &input);
			xmmsc_coll_attribute_get (op, "namespace", &input_ns);
		}

		g_printf (_("type: pshuffle\n"));
		g_printf (_("history: %s\n"), history);
		g_printf (_("upcoming: %s\n"), upcoming);
		g_printf (_("input: %s/%s\n"), input_ns, input);
		break;
	default:
		g_printf (_("type: unknown!\n"));
		break;
	}
}

void
print_padding (gint length)
{
	while (length-- > 0) {
		g_printf (" ");
	}
}

void
print_indented (const gchar *string, guint level)
{
	gboolean indent = TRUE;
	const gchar *c;

	for (c = string; *c; c++) {
		if (indent) {
			print_padding (level);
			indent = FALSE;
		}
		g_printf ("%c", *c);
		if (*c == '\n') {
			indent = TRUE;
		}
	}
}

/* FIXME: not portable? */
gint
find_terminal_width ()
{
	gint columns;
	struct winsize ws;
	char *colstr, *endptr;

	if (!isatty (STDOUT_FILENO)) {
		columns = 0;
	} else if (!ioctl (STDIN_FILENO, TIOCGWINSZ, &ws)) {
		columns = ws.ws_col;
	} else {
		colstr = getenv ("COLUMNS");
		if (colstr != NULL) {
			columns = strtol (colstr, &endptr, 10);
		}
	}

	/* Default to 80 columns */
	if (columns <= 0) {
		columns = 80;
	}

	return columns;
}
