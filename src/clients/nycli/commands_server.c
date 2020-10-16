/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <xmmsclient/xmmsclient.h>

#include "commands.h"
#include "cli_context.h"
#include "configuration.h"
#include "command.h"
#include "currently_playing.h"
#include "matching_browse.h"
#include "status.h"
#include "utils.h"
#include "xmmscall.h"

typedef struct cli_info_print_positions_St {
	cli_context_t *ctx;
	gint inc;
	gint pos;
} cli_info_print_positions_t;

static void
cli_info_pad_source (GString *sb, gint source_width, const gchar *source)
{
	gint i, length;

	g_string_truncate (sb, 0);

	length = strlen (source);
	for (i = 0; i < (source_width - length); i++) {
		g_string_insert_c (sb, i, ' ');
	}
	g_string_insert (sb, i, source);
}

static void
cli_info_print (xmmsv_t *propdict)
{
	xmmsv_t *properties, *sourcedict, *sources, *value;
	xmmsv_list_iter_t *pit, *sit;
	const gchar *source, *property;
	gint source_width;
	GString *sb;

	if (!xmmsv_propdict_lengths (propdict, NULL, &source_width)) {
		return;
	}

	sb = g_string_sized_new (source_width);

	xmmsv_dict_keys (propdict, &properties);
	xmmsv_list_sort (properties, xmmsv_strcmp);

	xmmsv_get_list_iter (properties, &pit);
	while (xmmsv_list_iter_entry_string (pit, &property)) {
		if (xmmsv_dict_get (propdict, property, &sourcedict)) {
			xmmsv_dict_keys (sourcedict, &sources);
			xmmsv_list_sort (sources, xmmsv_strcmp);

			xmmsv_get_list_iter (sources, &sit);
			while (xmmsv_list_iter_entry_string (sit, &source)) {
				if (xmmsv_dict_get (sourcedict, source, &value)) {
					cli_info_pad_source (sb, source_width, source);
					xmmsv_print_value (sb->str, property, value);
				}
				xmmsv_list_iter_next (sit);
			}
		}
		xmmsv_list_iter_next (pit);
	}

	g_string_free (sb, TRUE);
}

static void
cli_info_print_list (cli_context_t *ctx, xmmsv_t *val)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsv_list_iter_t *it;
	gboolean first = TRUE;
	gint32 id;

	xmmsv_get_list_iter (val, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		if (!first) {
			g_printf ("\n");
		} else {
			first = FALSE;
		}

		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, id),
		                 FUNC_CALL_P (cli_info_print, XMMS_PREV_VALUE));

		xmmsv_list_iter_next (it);
	}
}

static void
cli_info_print_position (gint pos, void *userdata)
{
	cli_info_print_positions_t *pack = (cli_info_print_positions_t *) userdata;
	xmmsc_connection_t *conn = cli_context_xmms_sync (pack->ctx);
	xmmsv_t *playlist = cli_context_active_playlist (pack->ctx);
	gint id;

	/* Skip if outside of playlist */
	if (!xmmsv_list_get_int (playlist, pos, &id)) {
		return;
	}

	/* Do not prepend newline before the first entry */
	if (pack->inc > 0) {
		g_printf ("\n");
	} else {
		pack->inc++;
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, id),
	                 FUNC_CALL_P (cli_info_print, XMMS_PREV_VALUE));
}

static void
cli_info_print_positions (cli_context_t *ctx, playlist_positions_t *positions)
{
	cli_info_print_positions_t udata = { ctx, 0, 0 };
	playlist_positions_foreach (positions, cli_info_print_position, TRUE, &udata);
}

/* TODO: Not really a part of the server sub-command, but in the future it
 *       can be refactored to just call "xmms2 server property" and let that
 *       do the printing
 */
gboolean
cli_info (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	playlist_positions_t *positions;
	xmmsv_t *query;

	gint current_position = cli_context_current_position (ctx);
	gint current_id = cli_context_current_id (ctx);

	if (command_arg_positions_get (cmd, 0, &positions, current_position)) {
		cli_info_print_positions (ctx, positions);
		playlist_positions_free (positions);
	} else if (command_arg_pattern_get (cmd, 0, &query, FALSE)) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0);
		                 FUNC_CALL_P (cli_info_print_list, ctx, XMMS_PREV_VALUE));
		xmmsv_unref (query);
	} else {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, current_id),
		                 FUNC_CALL_P (cli_info_print, XMMS_PREV_VALUE));
	}

	return FALSE;
}

gboolean
cli_server_import (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsc_result_t *res = NULL;

	gint i, count;
	const gchar *path;
	gboolean norecurs;

	if (!command_flag_boolean_get (cmd, "non-recursive", &norecurs)) {
		norecurs = FALSE;
	}

	for (i = 0, count = command_arg_count (cmd); i < count; ++i) {
		GList *files = NULL, *it;
		gchar *vpath, *enc;

		command_arg_string_get (cmd, i, &path);
		vpath = format_url (path, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_DIR);
		if (vpath == NULL) {
			g_printf (_ ("Warning: Skipping invalid url: '%s'"), path);
			continue;
		}

		enc = encode_url (vpath);
		files = matching_browse (conn, enc);

		for (it = g_list_first (files); it != NULL; it = g_list_next (it)) {
			browse_entry_t *entry = it->data;
			const gchar *url;
			gboolean is_directory;

			browse_entry_get (entry, &url, &is_directory);

			if (res != NULL) {
				/* Clean up any result from the last iteration */
				xmmsc_result_unref (res);
			}

			if (norecurs || !is_directory) {
				res = xmmsc_medialib_add_entry_encoded (conn, url);
			} else {
				res = xmmsc_medialib_import_path_encoded (conn, url);
			}

			browse_entry_free (entry);
		}

		g_free (enc);
		g_free (vpath);
		g_list_free (files);
	}

	if (res != NULL) {
		/* Wait for the last result to execute until we're done. */
		xmmsc_result_wait (res);
		xmmsc_result_unref (res);
	}

	if (count == 0) {
		g_printf (_("Error: no path to import!\n"));
	}

	return FALSE;
}

static void
cli_server_browse_print (xmmsv_t *list)
{
	xmmsv_list_iter_t *it;
	xmmsv_t *dict;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry (it, &dict)) {
		const gchar *path;
		gboolean isdir;

		/* Use realpath instead of path, good for playlists */
		if (!xmmsv_dict_entry_get_string (dict, "realpath", &path)) {
			if (!xmmsv_dict_entry_get_string (dict, "path", &path)) {
				/* broken data */
				continue;
			}
		}

		/* Append trailing slash to indicate directory */
		xmmsv_dict_entry_get_int (dict, "isdir", &isdir);

		g_print ("%s%c\n", path, isdir ? '/' : ' ');

		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_server_browse (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	const gchar *url;

	if (!command_arg_string_get (cmd, 0, &url)) {
		return FALSE;
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_xform_media_browse, conn, url),
	                 FUNC_CALL_P (cli_server_browse_print, XMMS_PREV_VALUE));
	return FALSE;
}

static void
cli_server_remove_ids (cli_context_t *ctx, xmmsv_t *list)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsv_list_iter_t *it;
	gint32 id;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		XMMS_CALL (xmmsc_medialib_remove_entry, conn, id);
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_server_remove (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	gchar *pattern;
	xmmsv_t *coll;

	if (!command_arg_longstring_get_escaped (cmd, 0, &pattern)) {
		g_printf (_("Error: you must provide a pattern!\n"));
		return FALSE;
	}

	if (!xmmsc_coll_parse (pattern, &coll)) {
		g_printf (_("Error: failed to parse the pattern!\n"));
		goto finish;
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, coll, NULL, 0, 0),
	                 FUNC_CALL_P (cli_server_remove_ids, ctx, XMMS_PREV_VALUE));
	xmmsv_unref (coll);

finish:
	g_free (pattern);

	return FALSE;
}

static void
cli_server_rehash_ids (cli_context_t *ctx, xmmsv_t *list)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsv_list_iter_t *it;
	gint32 id;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		XMMS_CALL (xmmsc_medialib_rehash, conn, id);
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_server_rehash (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	gchar *pattern = NULL;
	xmmsv_t *coll;

	if (command_arg_longstring_get_escaped (cmd, 0, &pattern)) {
		if (!xmmsc_coll_parse (pattern, &coll)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
		} else {
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, coll, NULL, 0, 0),
			                 FUNC_CALL_P (cli_server_rehash_ids, ctx, XMMS_PREV_VALUE));
			xmmsv_unref (coll);
		}
	} else {
		/* Rehash all media-library */
		XMMS_CALL (xmmsc_medialib_rehash, conn, 0);
	}

	g_free (pattern);

	return FALSE;
}

static void
cli_server_config_print_entry (const gchar *confname, xmmsv_t *val, void *udata)
{
	const gchar *string;
	gint number;

	if (xmmsv_get_string (val, &string))
		g_printf ("%s = %s\n", confname, string);
	else if (xmmsv_get_int (val, &number))
		g_printf ("%s = %d\n", confname, number);
}

static void
cli_server_config_print (xmmsv_t *config, const gchar *confname)
{
	xmmsv_dict_iter_t *dit;
	xmmsv_list_iter_t *lit;
	const gchar *key;
	xmmsv_t *value, *list;

	list = xmmsv_new_list ();

	xmmsv_get_dict_iter (config, &dit);
	while (xmmsv_dict_iter_pair (dit, &key, &value)) {
		/* Filter out keys that don't match the config name after
		 * shell wildcard expansion.
		 */
		if (confname != NULL && fnmatch (confname, key, 0)) {
			xmmsv_dict_iter_remove (dit);
		} else {
			xmmsv_list_append_string (list, key);
			xmmsv_dict_iter_next (dit);
		}
	}

	xmmsv_list_sort (list, xmmsv_strcmp);

	xmmsv_get_list_iter (list, &lit);
	while (xmmsv_list_iter_entry_string (lit, &key)) {
		xmmsv_dict_get (config, key, &value);
		cli_server_config_print_entry (key, value, NULL);
		xmmsv_list_iter_next (lit);
	}

	xmmsv_unref (list);
}

gboolean
cli_server_config (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	const gchar *confname, *confval;

	if (!command_arg_string_get (cmd, 0, &confname)) {
		confname = NULL;
		confval = NULL;
	} else if (!command_arg_string_get (cmd, 1, &confval)) {
		confval = NULL;
	}

	if (confval) {
		XMMS_CALL (xmmsc_config_set_value, conn, confname, confval);
	} else {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_config_list_values, conn),
		                 FUNC_CALL_P (cli_server_config_print, XMMS_PREV_VALUE, confname));
	}

	return FALSE;
}

static void
cli_server_property_print_one (xmmsv_t *dict,
                               gboolean show_source,
                               const gchar *key,
                               const gchar **sourcepref, const guint *len,
                               gboolean all_tied, gboolean all)
{
	gint i;
	xmmsv_dict_iter_t *it;
	const gchar *source;
	xmmsv_t *value;
	gboolean printed = FALSE;

	if (all)
		all_tied = TRUE;

	for (i = 0; sourcepref[i]; i++) {
		xmmsv_get_dict_iter (dict, &it);
		while (xmmsv_dict_iter_pair (it, &source, &value)) {
			if (0 == g_ascii_strncasecmp (sourcepref[i], source, len[i])) {
				xmmsv_print_value (show_source ? source : NULL, key, value);
				printed = TRUE;
				xmmsv_dict_iter_remove (it);
			} else {
				xmmsv_dict_iter_next (it);
			}

			if (printed && !all_tied)
				break;
		}
		xmmsv_dict_iter_explicit_destroy (it);

		if (printed && !all)
			break;
	}

	if (all) {
		xmmsv_get_dict_iter (dict, &it);
		while (xmmsv_dict_iter_pair (it, &source, &value)) {
			xmmsv_print_value (show_source ? source : NULL, key, value);
			xmmsv_dict_iter_next(it);
		}
		xmmsv_dict_iter_explicit_destroy (it);
	}
}


static void
cli_server_property_print (xmmsv_t *propdict,
                           gboolean show_source,
                           const gchar *propname,
                           const gchar **sourcepref,
                           gboolean all_tied, gboolean all)
{
	gint i;
	guint *len;
	xmmsv_dict_iter_t *it;
	xmmsv_t *dict;
	const gchar *key;

	if (!sourcepref) { sourcepref = xmmsv_default_source_pref; }

	len = g_malloc_n (g_strv_length ((gchar **)sourcepref), sizeof(guint));
	for (i = 0; sourcepref[i]; i++) {
		guint tmp = strlen (sourcepref[i]);
		len[i] = (tmp > 0 && sourcepref[i][tmp-1] == '*') ? tmp - 1 : tmp + 1;
	}

	if (propname) {
		if (xmmsv_dict_get (propdict, propname, &dict)) {
			cli_server_property_print_one (dict, show_source, NULL,
			                               sourcepref, len,
			                               all_tied, all);
		}
	} else {
		xmmsv_get_dict_iter (propdict, &it);
		while (xmmsv_dict_iter_pair (it, &key, &dict)) {
			cli_server_property_print_one (dict, show_source, key,
			                               sourcepref, len,
			                               all_tied, all);
			xmmsv_dict_iter_next (it);
		}
		xmmsv_dict_iter_explicit_destroy (it);
	}

	g_free (len);
}

gboolean
cli_server_property (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	gint mid;
	gboolean delete, fint, fstring, flong, fall, falltied;
	const gchar *source, *propname, *propval;
	const gchar **sourcepref = NULL;

	delete = fint = fstring = flong = fall = falltied = FALSE;

	/* get arguments */
	command_flag_boolean_get (cmd, "delete", &delete);
	command_flag_boolean_get (cmd, "int", &fint);
	command_flag_boolean_get (cmd, "string", &fstring);
	command_flag_boolean_get (cmd, "long", &flong);
	command_flag_boolean_get (cmd, "all", &fall);
	command_flag_boolean_get (cmd, "all-tied", &falltied);
	command_flag_stringarray_get (cmd, "source", &sourcepref);
	if (!command_arg_int_get (cmd, 0, &mid)) {
		g_printf ("Error: you must provide a media-id!\n");
		return FALSE;
	}
	if (!command_arg_string_get (cmd, 1, &propname)) {
		propname = NULL;
		propval = NULL;
	} else if (!command_arg_string_get (cmd, 2, &propval)) {
		propval = NULL;
	}

	if (sourcepref && sourcepref[0]) {
		source = sourcepref[0];
	} else {
		sourcepref = NULL;
		source = "client/" CLI_CLIENTNAME;
	}

	/* Do action */
	if (propname && propval) { /* Set */
		gboolean set_as_int;

		if (delete || flong || fall || falltied) {
			g_printf ("Error: Flags -D, -l, -a, and -A not allowed when setting property!\n");
			return FALSE;
		}
		if (fint && fstring) {
			g_printf ("Error: Flags -s and -i are mutually exclusive!\n");
			return FALSE;
		}
		if (sourcepref && sourcepref[0] && sourcepref[1]) {
			g_printf ("Error: Only one -S option allowed when setting value!\n");
			return FALSE;
		}

		if (fstring) {
			set_as_int = FALSE;
		} else {
			char *end;
			strtol (propval, &end, 0);
			if (*end != '\0' && fint) {
				g_printf ("Error: Flag -i used, but value is not an integer!\n");
				return FALSE;
			}
			set_as_int = (*end == '\0');
		}
		if (set_as_int) {
			XMMS_CALL (xmmsc_medialib_entry_property_set_int_with_source,
			           conn, mid, source, propname, strtol (propval, NULL, 0));
		} else {
			XMMS_CALL (xmmsc_medialib_entry_property_set_str_with_source,
			           conn, mid, source, propname, propval);
		}
	} else if (delete) { /* Delete */
		if (fint || fstring || flong || fall || falltied) {
			g_printf ("Error: Flags -i, -s, -l, -a, and -A not allowed when deleting value!\n");
			return FALSE;
		}
		if (sourcepref && sourcepref[0] && sourcepref[1]) {
			g_printf ("Error: Only one -S option allowed when deleting value!\n");
			return FALSE;
		}
		if (!propname) {
			g_printf ("Error: Property name mandatory when deleting value!\n");
			return FALSE;
		}

		XMMS_CALL (xmmsc_medialib_entry_property_remove_with_source,
		           conn, mid, source, propname);
	} else { /* Show */
		if (delete || fint || fstring) {
			g_printf ("Error: Flags -D, -s and -i not allowed when showing properties!\n");
			return FALSE;
		}
		if (fall && falltied) {
			g_printf ("Error: Flags -a and -A are mutually exclusive!\n");
			return FALSE;
		}

		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, mid),
		                 FUNC_CALL_P (cli_server_property_print,
		                              XMMS_PREV_VALUE, flong, propname, sourcepref, falltied, fall));
	}

	return FALSE;
}

static gint
cli_server_plugins_sortfunc (xmmsv_t **a, xmmsv_t **b)
{
	const gchar *an, *bn;
	xmmsv_dict_entry_get_string (*a, "shortname", &an);
	xmmsv_dict_entry_get_string (*b, "shortname", &bn);
	return g_strcmp0 (an, bn);
}

static void
cli_server_plugins_print (xmmsv_t *value)
{
	const gchar *name, *desc;
	xmmsv_list_iter_t *it;
	xmmsv_t *elem;

	xmmsv_list_sort (value, cli_server_plugins_sortfunc);

	xmmsv_get_list_iter (value, &it);
	while (xmmsv_list_iter_entry (it, &elem)) {
		xmmsv_dict_entry_get_string (elem, "shortname", &name);
		xmmsv_dict_entry_get_string (elem, "description", &desc);
		g_printf ("%-15s - %s\n", name, desc);
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_server_plugins (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_main_list_plugins, conn, XMMS_PLUGIN_TYPE_ALL),
	                 FUNC_CALL_P (cli_server_plugins_print, XMMS_PREV_VALUE));
	return FALSE;
}

static void
cli_server_volume_print_entry (const gchar *key, xmmsv_t *val, void *udata)
{
	const gchar *channel = udata;
	gint32 value;

	if (!udata || !strcmp (key, channel)) {
		xmmsv_get_int (val, &value);
		g_printf (_("%s = %u\n"), key, value);
	}
}

static void
cli_server_volume_print (xmmsv_t *dict, const gchar *channel)
{
	xmmsv_dict_foreach (dict, cli_server_volume_print_entry, (void *) channel);
}

static void
cli_server_volume_adjust (cli_context_t *ctx, xmmsv_t *val, const gchar *channel, gint relative)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsv_dict_iter_t *it;
	const gchar *innerchan;

	gint volume;

	xmmsv_get_dict_iter (val, &it);

	while (xmmsv_dict_iter_pair_int (it, &innerchan, &volume)) {
		if (channel == NULL || strcmp (channel, innerchan) == 0) {
			volume += relative;
			if (volume > 100) {
				volume = 100;
			} else if (volume < 0) {
				volume = 0;
			}

			XMMS_CALL (xmmsc_playback_volume_set, conn, innerchan, volume);
		}
		xmmsv_dict_iter_next (it);
	}
}

static void
cli_server_volume_collect_channels (const gchar *key, xmmsv_t *val, void *udata)
{
	GList **list = udata;
	*list = g_list_prepend (*list, g_strdup (key));
}

static void
cli_server_volume_set (cli_context_t *ctx, const gchar *channel, gint volume)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	xmmsc_result_t *res;
	xmmsv_t *val;
	GList *it, *channels = NULL;

	if (!channel) {
		/* get all channels */
		res = xmmsc_playback_volume_get (conn);
		xmmsc_result_wait (res);
		val = xmmsc_result_get_value (res);
		xmmsv_dict_foreach (val, cli_server_volume_collect_channels, &channels);
		xmmsc_result_unref (res);
	} else {
		channels = g_list_prepend (channels, g_strdup (channel));
	}

	/* set volumes for channels in list */
	for (it = g_list_first (channels); it != NULL; it = g_list_next (it)) {
		XMMS_CALL (xmmsc_playback_volume_set, conn, it->data, volume);
	}

	g_list_free_full (channels, g_free);
}

gboolean
cli_server_volume (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	const gchar *channel;
	gint volume;
	const gchar *volstr;
	bool relative_vol = false;

	if (!command_flag_string_get (cmd, "channel", &channel)) {
		channel = NULL;
	}

	if (!command_arg_int_get (cmd, 0, &volume)) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playback_volume_get, conn),
		                 FUNC_CALL_P (cli_server_volume_print, XMMS_PREV_VALUE, channel));
	} else {
		if (command_arg_string_get (cmd, 0, &volstr)) {
			relative_vol = (volstr[0] == '+') || volume < 0;
		}
		if (relative_vol) {
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playback_volume_get, conn),
			                 FUNC_CALL_P (cli_server_volume_adjust, ctx, XMMS_PREV_VALUE, channel, volume));
		} else {
			cli_server_volume_set (ctx, channel, volume);
		}
	}

	return FALSE;
}

static void
cli_server_stats_print (xmmsv_t *val)
{
	const gchar *version;
	gint d_days, d_hours, d_minutes, d_seconds;
	gint p_days, p_hours, p_minutes, p_seconds;
	gint uptime;
	int64_t size, duration, playtime;
	double size_gib;

	size = duration = playtime = 0;

	xmmsv_dict_entry_get_string (val, "version", &version);
	xmmsv_dict_entry_get_int (val, "uptime", &uptime);
	xmmsv_dict_entry_get_int64 (val, "size", &size);
	xmmsv_dict_entry_get_int64 (val, "duration", &duration);
	xmmsv_dict_entry_get_int64 (val, "playtime", &playtime);

	size_gib = size * 1.0 / 1024 / 1024 / 1024;

	breakdown_timespan (duration, &d_days, &d_hours, &d_minutes, &d_seconds);
	breakdown_timespan (playtime, &p_days, &p_hours, &p_minutes, &p_seconds);

	g_printf ("uptime = %d\n"
	          "version = %s\n"
	          "size = %.2fGiB\n"
	          "duration = %d days, %d hours, %d minutes, and %d seconds\n"
	          "playtime = %d days, %d hours, %d minutes, and %d seconds\n",
	          uptime, version, size_gib,
	          d_days, d_hours, d_minutes, d_seconds,
	          p_days, p_hours, p_minutes, p_seconds);
}

gboolean
cli_server_stats (cli_context_t *ctx, command_t *cmd)
{
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_main_stats, cli_context_xmms_sync (ctx)),
	                 FUNC_CALL_P (cli_server_stats_print, XMMS_PREV_VALUE));
	return FALSE;
}

gboolean
cli_server_sync (cli_context_t *ctx, command_t *cmd)
{
	XMMS_CALL (xmmsc_coll_sync, cli_context_xmms_sync (ctx));
	return FALSE;
}

/* The loop is resumed in the disconnect callback */
gboolean
cli_server_shutdown (cli_context_t *ctx, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_context_xmms_sync (ctx);
	if (conn != NULL) {
		XMMS_CALL (xmmsc_quit, conn);
	}
	return FALSE;
}
