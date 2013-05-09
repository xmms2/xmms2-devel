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

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <xmmsclient/xmmsclient.h>

#include "commands.h"
#include "cli_infos.h"
#include "configuration.h"
#include "command.h"
#include "currently_playing.h"
#include "matching_browse.h"
#include "status.h"
#include "utils.h"
#include "xmmscall.h"

typedef struct cli_info_print_positions_St {
	cli_infos_t *infos;
	gint inc;
	gint pos;
} cli_info_print_positions_t;

static void
cli_info_print (xmmsv_t *propdict)
{
	xmmsv_dict_iter_t *pit, *dit;
	xmmsv_t *dict, *value;
	const gchar *source, *key;

	xmmsv_get_dict_iter (propdict, &pit);
	while (xmmsv_dict_iter_pair (pit, &key, &dict)) {
		xmmsv_get_dict_iter (dict, &dit);
		while (xmmsv_dict_iter_pair (dit, &source, &value)) {
			xmmsv_print_value (source, key, value);
			xmmsv_dict_iter_next (dit);
		}
		xmmsv_dict_iter_next (pit);
	}
}

static void
cli_info_print_list (cli_infos_t *infos, xmmsv_t *val)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
	xmmsc_connection_t *conn = cli_infos_xmms_sync (pack->infos);
	xmmsv_t *playlist = cli_infos_active_playlist (pack->infos);
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
cli_info_print_positions (cli_infos_t *infos, playlist_positions_t *positions)
{
	cli_info_print_positions_t udata = { infos, 0, 0 };
	playlist_positions_foreach (positions, cli_info_print_position, TRUE, &udata);
}

/* TODO: Not really a part of the server sub-command, but in the future it
 *       can be refactored to just call "xmms2 server property" and let that
 *       do the printing
 */
gboolean
cli_info (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	playlist_positions_t *positions;
	xmmsv_t *query;

	gint current_position = cli_infos_current_position (infos);
	gint current_id = cli_infos_current_id (infos);

	if (command_arg_positions_get (cmd, 0, &positions, current_position)) {
		cli_info_print_positions (infos, positions);
		playlist_positions_free (positions);
	} else if (command_arg_pattern_get (cmd, 0, &query, FALSE)) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0);
		                 FUNC_CALL_P (cli_info_print_list, infos, XMMS_PREV_VALUE));
		xmmsv_unref (query);
	} else {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, current_id),
		                 FUNC_CALL_P (cli_info_print, XMMS_PREV_VALUE));
	}

	return FALSE;
}

gboolean
cli_server_import (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
cli_server_browse (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	const gchar *url;

	if (!command_arg_string_get (cmd, 0, &url)) {
		return FALSE;
	}

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_xform_media_browse, conn, url),
	                 FUNC_CALL_P (cli_server_browse_print, XMMS_PREV_VALUE));
	return FALSE;
}

static void
cli_server_remove_ids (cli_infos_t *infos, xmmsv_t *list)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_list_iter_t *it;
	gint32 id;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		XMMS_CALL (xmmsc_medialib_remove_entry, conn, id);
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_server_remove (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
	                 FUNC_CALL_P (cli_server_remove_ids, infos, XMMS_PREV_VALUE));
	xmmsv_unref (coll);

finish:
	g_free (pattern);

	return FALSE;
}

static void
cli_server_rehash_ids (cli_infos_t *infos, xmmsv_t *list)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_list_iter_t *it;
	gint32 id;

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		XMMS_CALL (xmmsc_medialib_rehash, conn, id);
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_server_rehash (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *pattern = NULL;
	xmmsv_t *coll;

	if (command_arg_longstring_get_escaped (cmd, 0, &pattern)) {
		if (!xmmsc_coll_parse (pattern, &coll)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
		} else {
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, coll, NULL, 0, 0),
			                 FUNC_CALL_P (cli_server_rehash_ids, infos, XMMS_PREV_VALUE));
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
	if (confname) {
		/* Filter out keys that don't match the config name after
		 * shell wildcard expansion.  */
		xmmsv_dict_iter_t *it;
		const gchar *key;
		xmmsv_t *val;

		xmmsv_get_dict_iter (config, &it);
		while (xmmsv_dict_iter_pair (it, &key, &val)) {
			if (fnmatch (confname, key, 0)) {
				xmmsv_dict_iter_remove (it);
			} else {
				xmmsv_dict_iter_next (it);
			}
		}
	}

	xmmsv_dict_foreach (config, cli_server_config_print_entry, NULL);
}

gboolean
cli_server_config (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
cli_server_property_print (xmmsv_t *propdict, const gchar *filter)
{
	xmmsv_dict_iter_t *pit, *dit;
	xmmsv_t *dict, *value;
	const gchar *source, *key;

	xmmsv_get_dict_iter (propdict, &pit);
	while (xmmsv_dict_iter_pair (pit, &source, &dict)) {
		if (strcmp (source, filter) == 0) {
			xmmsv_get_dict_iter (dict, &dit);
			while (xmmsv_dict_iter_pair (dit, &key, &value)) {
				xmmsv_print_value (source, key, value);
				xmmsv_dict_iter_next (dit);
			}
		}
		xmmsv_dict_iter_next (pit);
	}
}

gboolean
cli_server_property (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gint mid;
	gchar *default_source = NULL;
	gboolean delete, fint, fstring;
	const gchar *source, *propname, *propval;

	delete = fint = fstring = FALSE;

	command_flag_boolean_get (cmd, "delete", &delete);
	command_flag_boolean_get (cmd, "int", &fint);
	command_flag_boolean_get (cmd, "string", &fstring);

	if (delete && (fint || fstring)) {
		g_printf ("Error: --int and --string flags are invalid with --delete!\n");
		return FALSE;
	}

	if (fint && fstring) {
		g_printf ("Error: --int and --string flags are mutually exclusive!\n");
		return FALSE;
	}

	if (!command_arg_int_get (cmd, 0, &mid)) {
		g_printf ("Error: you must provide a media-id!\n");
		return FALSE;
	}

	default_source = g_strdup_printf ("client/%s", CLI_CLIENTNAME);

	if (!command_flag_string_get (cmd, "source", &source)) {
		source = default_source;
	}

	if (!command_arg_string_get (cmd, 1, &propname)) {
		propname = NULL;
		propval = NULL;
	} else if (!command_arg_string_get (cmd, 2, &propval)) {
		propval = NULL;
	}

	if (delete) {
		if (!propname) {
			g_printf (_("Error: you must provide a property to delete!\n"));
			goto finish;
		}
		XMMS_CALL (xmmsc_medialib_entry_property_remove_with_source,
		           conn, mid, source, propname);
	} else if (!propval) {
		const gchar *filter = source == default_source ? NULL : source;
		/* use source-preference when printing and user hasn't set --source */
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, mid),
		                 FUNC_CALL_P (cli_server_property_print, XMMS_PREV_VALUE, filter));
	} else {
		gint value;
		gboolean cons;
		gchar *endptr;

		value = strtol (propval, &endptr, 0);

		/* determine save-type of the property */
		cons = endptr == '\0';
		fstring =  !cons & !fint;
		fint = cons | fint;

		if (fint) {
			XMMS_CALL (xmmsc_medialib_entry_property_set_int_with_source,
			           conn, mid, source, propname, value);
		} else {
			XMMS_CALL (xmmsc_medialib_entry_property_set_str_with_source,
			           conn, mid, source, propname, propval);
		}
	}

finish:
	g_free (default_source);

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
cli_server_plugins (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
cli_server_volume_adjust (cli_infos_t *infos, xmmsv_t *val, const gchar *channel, gint relative)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_dict_iter_t *it;
	const gchar *innerchan;

	gint volume;

	xmmsv_get_dict_iter (val, &it);

	while (xmmsv_dict_iter_pair_int (it, &innerchan, &volume)) {
		if (channel && strcmp (channel, innerchan) == 0) {
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
cli_server_volume_set (cli_infos_t *infos, const gchar *channel, gint volume)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
cli_server_volume (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
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
			                 FUNC_CALL_P (cli_server_volume_adjust, infos, XMMS_PREV_VALUE, channel, volume));
		} else {
			cli_server_volume_set (infos, channel, volume);
		}
	}

	return FALSE;
}

static void
cli_server_stats_print (xmmsv_t *val)
{
	const gchar *version;
	gint uptime;

	xmmsv_dict_entry_get_string (val, "version", &version);
	xmmsv_dict_entry_get_int (val, "uptime", &uptime);

	g_printf ("uptime = %d\n"
	          "version = %s\n", uptime, version);
}

gboolean
cli_server_stats (cli_infos_t *infos, command_t *cmd)
{
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_main_stats, cli_infos_xmms_sync (infos)),
	                 FUNC_CALL_P (cli_server_stats_print, XMMS_PREV_VALUE));
	return FALSE;
}

gboolean
cli_server_sync (cli_infos_t *infos, command_t *cmd)
{
	XMMS_CALL (xmmsc_coll_sync, cli_infos_xmms_sync (infos));
	return FALSE;
}

/* The loop is resumed in the disconnect callback */
gboolean
cli_server_shutdown (cli_infos_t *infos, command_t *cmd)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	if (conn != NULL) {
		XMMS_CALL (xmmsc_quit, conn);
	}
	return FALSE;
}
