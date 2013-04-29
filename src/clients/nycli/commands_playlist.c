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

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <xmmsclient/xmmsclient.h>

#include "main.h"
#include "cli_infos.h"
#include "column_display.h"
#include "command_utils.h"
#include "commands.h"
#include "configuration.h"
#include "matching_browse.h"
#include "playlist_positions.h"
#include "xmmscall.h"
#include "utils.h"

typedef struct cli_move_positions_St {
	xmmsc_connection_t *sync;
	const gchar *playlist;
	gint inc;
	gint pos;
} cli_move_positions_t;

typedef struct cli_list_positions_St {
	xmmsc_connection_t *sync;
	column_display_t *coldisp;
	xmmsv_t *entries;
} cli_list_positions_t;

typedef struct cli_remove_positions_St {
	xmmsc_connection_t *sync;
	const gchar *playlist;
} cli_remove_positions_t;

typedef void (*cli_list_print_func_t)(cli_infos_t *infos, column_display_t *coldisp, xmmsv_t *list, gpointer udata);

static gint
g_direct_compare (gconstpointer x, gconstpointer y)
{
	if (x > y)
		return 1;
	if (x < y)
		return -1;
	return 0;
}

static GTree *
g_tree_new_from_xmmsv (xmmsv_t *list)
{
	xmmsv_list_iter_t *it;
	GTree *tree;
	gint id;

	tree = g_tree_new (g_direct_compare);

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		g_tree_insert (tree, GINT_TO_POINTER (id), GINT_TO_POINTER (id));
		xmmsv_list_iter_next (it);
	}

	return tree;
}

static xmmsv_t *
get_coll (cli_infos_t *infos, const gchar *name, xmmsv_coll_namespace_t ns)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsc_result_t *res;
	xmmsv_t *val;
	const gchar *err;

	res = xmmsc_coll_get (conn, name, ns);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (!xmmsv_get_error (val, &err)) {
		xmmsv_ref (val);
	} else {
		g_printf (_("Error: Could not retrieve collection %s.\n"), name);
		val = NULL;
	}

	xmmsc_result_unref (res);

	return val;
}

/* Get current position in @playlist or in active playlist if
   @playlist == NULL. */
static gboolean
playlist_currpos_get (cli_infos_t *infos, const gchar *playlist, gint *pos)
{
	xmmsv_t *coll;
	const gchar *str;

	if (playlist) {
		if ((coll = get_coll (infos, playlist, XMMS_COLLECTION_NS_PLAYLISTS))) {
			if (xmmsv_coll_attribute_get_string (coll, "position", &str)) {
				*pos = strtol (str, NULL, 10);
			} else {
				*pos = -1;
			}
			xmmsv_unref (coll);
		} else {
			return FALSE;
		}
	} else {
		*pos = cli_infos_current_position (infos);
	}

	return TRUE;
}

/* Get length of @playlist or of active playlist if @playlist == NULL. */
static gboolean
playlist_length_get (cli_infos_t *infos, const gchar *playlist, gint *len)
{
	xmmsv_t *coll;

	if (playlist) {
		if ((coll = get_coll (infos, playlist, XMMS_COLLECTION_NS_PLAYLISTS))) {
			*len = xmmsv_coll_idlist_get_size (coll);
			xmmsv_unref (coll);
		} else {
			return FALSE;
		}
	} else {
		xmmsv_t *entries = cli_infos_active_playlist (infos);
		*len = xmmsv_list_get_size (entries);
	}

	return TRUE;
}

static gboolean
cmd_flag_pos_get_playlist (cli_infos_t *infos, command_context_t *ctx,
                           gint *pos, const gchar *playlist)
{
	gboolean next, at_isset;
	gint at;
	gint tmp;

	at_isset = command_flag_int_get (ctx, "at", &at);
	command_flag_boolean_get (ctx, "next", &next);

	if (next && at_isset) {
		g_printf (_("Error: --next and --at are mutually exclusive!\n"));
		return FALSE;
	} else if (next) {
		playlist_currpos_get (infos, playlist, &tmp);
		*pos = tmp + 1;
	} else if (at_isset) {
		if (!playlist_length_get (infos, playlist, &tmp)) {
			return FALSE;
		}

		if (at == 0 || (at > 0 && at > tmp + 1)) {
			g_printf (_("Error: specified position is outside the playlist!\n"));
			return FALSE;
		} else {
			*pos = at - 1;  /* playlist ids start at 0 */
		}
	} else {
		/* default to append */
		playlist_length_get (infos, playlist, pos);
	}

	return TRUE;
}

static column_display_t *
cli_list_classic_column_display (cli_infos_t *infos)
{
	configuration_t *config = cli_infos_config (infos);
	column_display_t *coldisp;
	const gchar *format, *marker;

	marker = configuration_get_string (config, "PLAYLIST_MARKER");
	format = configuration_get_string (config, "CLASSIC_LIST_FORMAT");

	/* FIXME: compute field size dynamically instead of hardcoding maxlen? */

	coldisp = column_display_init ();

	column_display_set_list_marker (coldisp, marker);

	column_display_add_special (coldisp, "",
	                            GINT_TO_POINTER(cli_infos_current_position (infos)), 2,
	                            COLUMN_DEF_SIZE_FIXED,
	                            COLUMN_DEF_ALIGN_LEFT,
	                            column_display_render_highlight);
	column_display_add_separator (coldisp, "[");
	column_display_add_special (coldisp, "pos", NULL, 0,
	                            COLUMN_DEF_SIZE_AUTO,
	                            COLUMN_DEF_ALIGN_RIGHT,
	                            column_display_render_position);
	column_display_add_separator (coldisp, "/");
	column_display_add_property (coldisp, "id", "id", 0,
	                             COLUMN_DEF_SIZE_AUTO,
	                             COLUMN_DEF_ALIGN_LEFT);
	column_display_add_separator (coldisp, "] ");

	column_display_add_format (coldisp, "tracks", format, 0,
	                           COLUMN_DEF_SIZE_AUTO,
	                           COLUMN_DEF_ALIGN_LEFT);

	/* FIXME: making duration part of the format would require proper
	 * rendering of duration in xmmsv_dict_format and conditional
	 * expressions to the parentheses if no duration is present. */

	/* FIXME: if time takes 6 chars, the display will exceed termwidth.. */
	column_display_add_separator (coldisp, " (");
	column_display_add_special (coldisp, "duration", (gpointer) "duration", 5,
	                            COLUMN_DEF_SIZE_FIXED,
	                            COLUMN_DEF_ALIGN_LEFT,
	                            column_display_render_time);
	column_display_add_separator (coldisp, ")");

	return coldisp;
}


static void
cli_list_print_row (column_display_t *coldisp, xmmsv_t *propdict)
{
	xmmsv_t *info = xmmsv_propdict_to_dict (propdict, NULL);
	enrich_mediainfo (info);
	column_display_print (coldisp, info);
}

static void
cli_list_print_positions_row (gint pos, void *udata)
{
	cli_list_positions_t *pack = (cli_list_positions_t *) udata;
	gint id;

	if (pos >= xmmsv_list_get_size (pack->entries)) {
		return;
	}

	if (xmmsv_list_get_int (pack->entries, pos, &id)) {
		column_display_set_position (pack->coldisp, pos);
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, pack->sync, id),
		                 FUNC_CALL_P (cli_list_print_row, pack->coldisp, XMMS_PREV_VALUE));
	}
}

static void
cli_list_print_positions (cli_infos_t *infos, column_display_t *coldisp,
                          xmmsv_t *list, gpointer udata)
{
	cli_list_positions_t pudata = {
		.sync = cli_infos_xmms_sync (infos),
		.coldisp = coldisp,
		.entries = list
	};
	playlist_positions_t *positions = (playlist_positions_t *) udata;
	playlist_positions_foreach (positions, cli_list_print_positions_row, TRUE, &pudata);
}

static void
cli_list_print_ids (cli_infos_t *infos, column_display_t *coldisp,
                    xmmsv_t *list, gpointer udata)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_list_iter_t *it;
	GTree *lookup = NULL;
	gint id;

	xmmsv_t *filter = (xmmsv_t *) udata;

	if (filter != NULL)
		lookup = g_tree_new_from_xmmsv (filter);

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		column_display_set_position (coldisp, xmmsv_list_iter_tell (it));
		if (lookup == NULL || g_tree_lookup (lookup, GINT_TO_POINTER (id)) != NULL) {
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_medialib_get_info, conn, id),
			                 FUNC_CALL_P (cli_list_print_row, coldisp, XMMS_PREV_VALUE));
		}
		xmmsv_list_iter_next (it);
	}

	if (lookup)
		g_tree_destroy (lookup);
}

static void
cli_list_print (cli_infos_t *infos, xmmsv_t *list,
                column_display_t *coldisp, gboolean column_style,
                cli_list_print_func_t func, gpointer udata)
{
	column_display_prepare (coldisp);
	if (column_style) {
		column_display_print_header (coldisp);
	}

	func (infos, coldisp, list, udata);

	if (column_style) {
		column_display_print_footer (coldisp);
	} else {
		g_printf ("\n");
		column_display_print_footer_totaltime (coldisp);
	}

	column_display_free (coldisp);
}

gboolean
cli_list (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	configuration_t *config = cli_infos_config (infos);
	playlist_positions_t *positions;
	column_display_t *coldisp;
	xmmsv_t *query = NULL;
	gboolean column_style, filter_by_pos = FALSE;
	const gchar *default_columns[] = { "curr", "pos", "id", "artist", "album", "title", NULL };
	const gchar *playlist;
	gchar *pattern = NULL;
	gint pos;

	/* Default to active playlist (from cache) */
	if (!command_flag_string_get (ctx, "playlist", &playlist)) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	if (!playlist_currpos_get (infos, playlist, &pos)) {
		g_printf (_("Error: failed to get current position in playlist.\n"));
		return FALSE;
	}

	if (command_arg_positions_get (ctx, 0, &positions, pos)) {
		filter_by_pos = TRUE;
	} else if (command_arg_longstring_get_escaped (ctx, 0, &pattern)) {
		if (!xmmsv_coll_parse (pattern, &query)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
			g_free (pattern);
			return FALSE;
		}
	}

	/* Has filter, retrieve ids from intersection */
	if (query != NULL) {
		query = xmmsv_coll_intersect_with_playlist (query, playlist);
	}

	column_style = !configuration_get_boolean (config, "CLASSIC_LIST");
	if (column_style) {
		const gchar *playlist_marker;
		playlist_marker = configuration_get_string (config, "PLAYLIST_MARKER");
		coldisp = column_display_build (default_columns, playlist_marker, cli_infos_current_position (infos));
	} else {
		coldisp = cli_list_classic_column_display (infos);
	}

	if (filter_by_pos) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_list_entries, conn, playlist),
		                 FUNC_CALL_P (cli_list_print, infos, XMMS_PREV_VALUE, coldisp, column_style, cli_list_print_positions, positions));
	} else if (query != NULL) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_list_entries, conn, playlist),
		                 XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0),
		                 FUNC_CALL_P (cli_list_print, infos, XMMS_FIRST_VALUE, coldisp, column_style, cli_list_print_ids, XMMS_SECOND_VALUE));
		xmmsv_unref (query);
	} else {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_list_entries, conn, playlist),
		                 FUNC_CALL_P (cli_list_print, infos, XMMS_PREV_VALUE, coldisp, column_style, cli_list_print_ids, NULL));
	}

	if (filter_by_pos) {
		playlist_positions_free (positions);
	}

	g_free (pattern);

	return FALSE;
}

static gboolean
cli_add_guesspls (cli_infos_t *infos, const gchar *url)
{
	configuration_t *config = cli_infos_config (infos);

	if (!configuration_get_boolean (config, "GUESS_PLS")) {
		return FALSE;
	}

	if (g_str_has_suffix (url, ".m3u") || g_str_has_suffix (url, ".pls")) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
cli_add_guessfile (const gchar *pattern)
{
	char *p;
	struct stat filestat;

	/* if matches a local path, it's probably a file */
	if (stat (pattern, &filestat) == 0 &&
	    (S_ISREG(filestat.st_mode) || S_ISDIR(filestat.st_mode))) {
		return TRUE;
	}

	p = strpbrk (pattern, ":/~");

	if (!p) {
		/* Doesn't contain any of the chars above, not a file? */
		return FALSE;
	} else if (p[0] == ':' && p[1] == '/' && p[2] == '/') {
		/* Contains '://', likely a URL */
		return TRUE;
	} else if (p == pattern && p[0] == '/') {
		/* Starts with '/', should be an absolute path */
		return TRUE;
	} else if (p == pattern && p[0] == '~' && strchr (p, '/')) {
		/* Starts with '~' and contains '/', should be a home path */
		return TRUE;
	}

	return FALSE;
}

/**
 * Build a dict out of a number of key=value strings.
 *
 * @return a dict with 0..n attributes.
 */
static xmmsv_t *
cli_add_parse_attributes (command_context_t *ctx)
{
	const gchar **attributes;
	xmmsv_t *result;
	gint i;

	result = xmmsv_new_dict ();

	if (command_flag_stringarray_get (ctx, "attribute", &attributes)) {
		for (i = 0; attributes[i] != NULL; i++) {
			gchar **parts = g_strsplit (attributes[i], "=", 2);
			if (parts[0] != NULL && parts[1] != NULL) {
				xmmsv_dict_set_string (result, parts[0], parts[1]);
			}
			g_strfreev (parts);
		}
	}

	return result;
}

/**
 * Helper function for cli_add.
 *
 * Add the contents of a playlist file (specified by a url)
 * to a playlist at a given position.
 */
static void
cli_add_playlist_file (cli_infos_t *infos, const gchar *url,
                       const gchar *playlist, gint pos)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *decoded = decode_url (url);
	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_idlist_from_playlist_file, conn, decoded),
	                 XMMS_CALL_P (xmmsc_playlist_add_idlist, conn, playlist, XMMS_PREV_VALUE));
	g_free (decoded);
}

/**
 * Helper function for cli_add.
 *
 * Add a file specified by a url to a playlist at a given id.
 */
static void
cli_add_file (cli_infos_t *infos, const gchar *url,
              const gchar *playlist, gint pos, xmmsv_t *attrs)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *decoded = decode_url (url);
	XMMS_CALL (xmmsc_playlist_insert_full, conn, playlist, pos, decoded, attrs);
	g_free (decoded);
}

/**
 * Helper function for cli_add.
 *
 * Add a directory to a playlist recursively at a given position.
 */
static void
cli_add_dir (cli_infos_t *infos, const gchar *url,
             const gchar *playlist, gint pos)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	XMMS_CALL (xmmsc_playlist_rinsert_encoded, conn, playlist, pos, url);
}

/**
 * Helper function for cli_add.
 *
 * Process and add file arguments to a playlist at a given position.
 * The files may be regular or playlist files.
 *
 * @return whether media has been added to the playlist.
 */
static gboolean
cli_add_fileargs (cli_infos_t *infos, command_context_t *ctx,
                  const gchar *playlist, gint pos)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gint i;
	gboolean plsfile, norecurs, ret = FALSE;
	xmmsv_t *attributes;

	command_flag_boolean_get (ctx, "pls", &plsfile);
	command_flag_boolean_get (ctx, "non-recursive", &norecurs);
	attributes = cli_add_parse_attributes (ctx);

	for (i = 0; i < command_arg_count (ctx); i++) {
		GList *files, *it;
		const gchar *path;
		gchar *formatted, *encoded;

		command_arg_string_get (ctx, i, &path);
		formatted = format_url (path, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_DIR);
		if (formatted == NULL) {
			g_printf (_("Warning: Skipping invalid url '%s'.\n"), path);
			continue;
		}

		encoded = encode_url (formatted);
		files = matching_browse (conn, encoded);

		for (it = g_list_first (files); it != NULL; it = g_list_next (it)) {
			const gchar *url;
			gboolean is_directory;
			browse_entry_t *entry = it->data;

			browse_entry_get (entry, &url, &is_directory);

			if (plsfile || cli_add_guesspls (infos, url)) {
				if (xmmsv_dict_get_size (attributes) > 0) {
					g_printf (_("Warning: Skipping attributes together with playlist.\n"));
				}

				cli_add_playlist_file (infos, url, playlist, pos);
			} else if (norecurs || !is_directory) {
				cli_add_file (infos, url, playlist, pos, attributes);
			} else {
				if (xmmsv_dict_get_size (attributes) > 0) {
					g_printf (_("Warning: Skipping attributes together with directory.\n"));
				}

				cli_add_dir (infos, url, playlist, pos);
			}

			pos++; /* next insert at next pos, to keep order */
			browse_entry_free (entry);

			ret = TRUE;
		}

		g_free (encoded);
		g_free (formatted);
		g_list_free (files);
	}

	xmmsv_unref (attributes);
	return ret;
}

/**
 * Add a list of ids to a playlist, starting at a given position.
 *
 * TODO: xmmsc_playlist_insert_collection should return the number of entries
 *       it inserted, and then this function will become meaningless.
 */
static void
cli_add_collection (cli_infos_t *infos, xmmsv_t *ids, xmmsv_t *query,
                    const gchar *playlist, gint pos, gboolean *success)

{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	if (xmmsv_list_get_size (ids) > 0) {
		XMMS_CALL (xmmsc_playlist_insert_collection, conn, playlist, pos, query, NULL);
		*success = TRUE;
	}
}

/**
 * Helper function for cli_add.
 *
 * Process and add pattern arguments to a playlist at a given position.
 *
 * @return Whether any media has been added to the playlist.
 */
static gboolean
cli_add_pattern (cli_infos_t *infos, command_context_t *ctx,
                 const gchar *playlist, gint pos)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_t *query, *ordered_query;
	const gchar **order = NULL;
	gchar *pattern = NULL;
	gboolean success = FALSE;

	command_arg_longstring_get_escaped (ctx, 0, &pattern);

	if (!pattern) {
		g_printf (_("Error: you must provide a pattern to add!\n"));
	} else if (!xmmsc_coll_parse (pattern, &query)) {
		g_printf (_("Error: failed to parse the pattern!\n"));
	} else {
		if (command_flag_stringlist_get (ctx, "order", &order)) {
			xmmsv_t *orderval = xmmsv_make_stringlist ((gchar **) order, -1);
			ordered_query = xmmsv_coll_add_order_operators (query, orderval);

			xmmsv_unref (orderval);
		} else {
			ordered_query = xmmsv_coll_apply_default_order (query);
		}

		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, ordered_query, NULL, 0, 0),
		                 FUNC_CALL_P (cli_add_collection, infos, XMMS_PREV_VALUE, ordered_query, playlist, pos, &success));

		xmmsv_unref (ordered_query);
		xmmsv_unref (query);
	}

	g_free (pattern);
	g_free (order);

	return success;
}

gboolean
cli_add (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gint pos, i;
	const gchar *playlist;
	gboolean forceptrn, plsfile, fileargs, jump, added;

	/*
	  --file  Add a path from the local filesystem
	  --non-recursive  Do not add directories recursively.
	  --playlist  Add to the given playlist.
	  --next  Add after the current track.
	  --at  Add media at a given position in the playlist, or at a given offset from the current track.
	  --order Order media matched by pattern.
	*/

	/* FIXME: offsets not supported (need to identify positive offsets) :-( */
	if (!command_flag_string_get (ctx, "playlist", &playlist)) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	if (!cmd_flag_pos_get_playlist (infos, ctx, &pos, playlist)) {
		return FALSE;
	}

	command_flag_boolean_get (ctx, "pattern", &forceptrn);
	command_flag_boolean_get (ctx, "pls", &plsfile);
	command_flag_boolean_get (ctx, "file", &fileargs);
	command_flag_boolean_get (ctx, "jump", &jump);

	fileargs = fileargs || plsfile;
	if (forceptrn && fileargs) {
		g_printf (_("Error: --pattern is mutually exclusive with "
		            "--file and --pls!\n"));
		return FALSE;
	}

	if (!forceptrn) {
		/* if any of the arguments is a valid path, we treat them all as files */
		for (i = 0; !fileargs && i < command_arg_count (ctx); i++) {
			const gchar *path;

			command_arg_string_get (ctx, i, &path);
			fileargs = fileargs || cli_add_guessfile (path);
		}
	}

	if (fileargs) {
		added = cli_add_fileargs (infos, ctx, playlist, pos);
	} else {
		gboolean norecurs;
		xmmsv_t *attributes;

		command_flag_boolean_get (ctx, "non-recursive", &norecurs);
		if (norecurs) {
			g_printf (_("Error:"
			            "--non-recursive only applies when passing --file!\n"));
			return FALSE;
		}

		attributes = cli_add_parse_attributes (ctx);
		if (xmmsv_dict_get_size (attributes) > 0) {
			g_printf (_("Warning: Skipping attributes together with pattern.\n"));
		}

		xmmsv_unref (attributes);

		added = cli_add_pattern (infos, ctx, playlist, pos);
	}

	if (added && jump) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_set_next, conn, pos),
		                 XMMS_CALL_P (xmmsc_playback_tickle, conn));
	}

	return FALSE;
}

static void
cli_move_positions (gint curr, void *userdata)
{
	cli_move_positions_t *pack = (cli_move_positions_t *) userdata;

	/* Entries are moved in descending order, pack->inc is used as
	 * offset both for forward and backward moves, and reset
	 * inbetween. */

	if (curr < pack->pos) {
		/* moving forward */
		if (pack->inc >= 0) {
			pack->inc = -1; /* start inc at -1, decrement */
		}
		XMMS_CALL (xmmsc_playlist_move_entry, pack->sync,
		           pack->playlist, curr, pack->pos + pack->inc);
		pack->inc--;
	} else {
		/* moving backward */
		XMMS_CALL (xmmsc_playlist_move_entry, pack->sync,
		           pack->playlist, curr + pack->inc, pack->pos);
		pack->inc++;
	}
}

static void
cli_move_entries (xmmsv_t *matching, xmmsv_t *lisval, cli_infos_t *infos,
                  const gchar *playlist, gint pos)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_list_iter_t *it;
	gint curr, id, inc;
	gboolean up;

	/* store matching mediaids in a tree (faster lookup) */
	GTree *list = g_tree_new_from_xmmsv (matching);

	/* move matched playlist items */
	curr = 0;
	inc = 0;
	up = TRUE;

	xmmsv_get_list_iter (lisval, &it);
	while (xmmsv_list_iter_entry_int (it, &id)) {
		if (curr == pos) {
			up = FALSE;
		}
		if (g_tree_lookup (list, GINT_TO_POINTER (id)) != NULL) {
			if (up) {
				/* moving forward */
				XMMS_CALL (xmmsc_playlist_move_entry,
				           conn, playlist, curr - inc, pos - 1);
			} else {
				/* moving backward */
				XMMS_CALL (xmmsc_playlist_move_entry,
				           conn, playlist, curr, pos + inc);
			}
			inc++;
		}
		curr++;

		xmmsv_list_iter_next (it);
	}

	g_tree_destroy (list);
}

gboolean
cli_move (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	playlist_positions_t *positions;
	const gchar *playlist;
	xmmsv_t *query;
	gint pos;

	if (!command_flag_string_get (ctx, "playlist", &playlist)) {
		playlist = NULL;
	}

	if (!cmd_flag_pos_get_playlist (infos, ctx, &pos, playlist)) {
		g_printf (_("Error: you must provide a position to move entries to!\n"));
		return FALSE;
	}

	if (command_arg_positions_get (ctx, 0, &positions, cli_infos_current_position (infos))) {
		cli_move_positions_t udata = {
			.sync = conn,
			.playlist = playlist,
			.inc = 0,
			.pos = pos
		};
		playlist_positions_foreach (positions, cli_move_positions, FALSE, &udata);
		playlist_positions_free (positions);
	} else if (command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0),
		                 XMMS_CALL_P (xmmsc_playlist_list_entries, conn, playlist),
		                 FUNC_CALL_P (cli_move_entries, XMMS_FIRST_VALUE, XMMS_SECOND_VALUE, infos, playlist, pos));
		xmmsv_unref (query);
	}

	return FALSE;
}

static void
cli_remove_ids (cli_infos_t *infos, const gchar *playlist,
                xmmsv_t *matchval, xmmsv_t *plistval)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_list_iter_t *matchit, *plistit;
	gint plid, id, offset;

	offset = 0;

	xmmsv_get_list_iter (matchval, &matchit);
	xmmsv_get_list_iter (plistval, &plistit);

	/* Loop on the playlist */
	while (xmmsv_list_iter_entry_int (plistit, &plid)) {
		xmmsv_list_iter_first (matchit);

		/* Loop on the matched media */
		while (xmmsv_list_iter_entry_int (matchit, &id)) {
			/* If both match, remove */
			if (plid == id) {
				XMMS_CALL (xmmsc_playlist_remove_entry, conn, playlist,
				           xmmsv_list_iter_tell (plistit) - offset);
				offset++;
				break;
			}
			xmmsv_list_iter_next (matchit);
		}
		xmmsv_list_iter_next (plistit);
	}
}

static void
cli_remove_positions_each (gint pos, void *udata)
{
	cli_remove_positions_t *pack = (cli_remove_positions_t *) udata;
	XMMS_CALL (xmmsc_playlist_remove_entry, pack->sync, pack->playlist, pos);
}

gboolean
cli_remove (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	const gchar *playlist;
	xmmsv_t *query;
	playlist_positions_t *positions;

	if (!command_flag_string_get (ctx, "playlist", &playlist)) {
		playlist = cli_infos_active_playlist_name (infos);
	}

	if (command_arg_positions_get (ctx, 0, &positions, cli_infos_current_position (infos))) {
		cli_remove_positions_t udata = {
			.sync = conn,
			.playlist = playlist
		};
		playlist_positions_foreach (positions, cli_remove_positions_each, FALSE, &udata);
		playlist_positions_free (positions);
	} else if (command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		if (!playlist) {
			xmmsv_t *entries = cli_infos_active_playlist (infos);
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0),
			                 FUNC_CALL_P (cli_remove_ids, infos, playlist, XMMS_PREV_VALUE, entries));
		} else {
			query = xmmsv_coll_intersect_with_playlist (query, playlist);
			XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_query_ids, conn, query, NULL, 0, 0),
			                 XMMS_CALL_P (xmmsc_playlist_list_entries, conn, playlist),
			                 FUNC_CALL_P (cli_remove_ids, infos, playlist, XMMS_FIRST_VALUE, XMMS_SECOND_VALUE));
		}
		xmmsv_unref (query);
	}

	return FALSE;
}

static void
cli_pl_list_print (xmmsv_t *list, cli_infos_t *infos, gboolean show_hidden)
{
	const gchar *playlist, *s;
	xmmsv_list_iter_t *it;

	playlist = cli_infos_active_playlist_name (infos);

	xmmsv_get_list_iter (list, &it);
	while (xmmsv_list_iter_entry_string (it, &s)) {
		/* Skip hidden playlists if all is FALSE*/
		if ((*s != '_') || show_hidden) {
			/* Highlight active playlist */
			if (g_strcmp0 (s, playlist) == 0) {
				g_printf ("* %s\n", s);
			} else {
				g_printf ("  %s\n", s);
			}
		}
		xmmsv_list_iter_next (it);
	}
}

gboolean
cli_pl_list (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gboolean all;

	/* FIXME: support pattern argument (only display playlist containing matching media) */

	command_flag_boolean_get (ctx, "all", &all);

	XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_playlist_list, conn),
	                 FUNC_CALL_P (cli_pl_list_print, XMMS_PREV_VALUE, infos, all));

	return FALSE;
}

gboolean
cli_pl_switch (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		g_printf (_("Error: failed to read new playlist name!\n"));
		return FALSE;
	}

	XMMS_CALL (xmmsc_playlist_load, conn, playlist);

	g_free (playlist);

	return FALSE;
}

static gboolean
cli_pl_create_check_exists (cli_infos_t *infos, const gchar *playlist)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsc_result_t *res;
	xmmsv_t *val;
	gboolean retval = FALSE;

	res = xmmsc_coll_get (conn, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsc_result_wait (res);

	val = xmmsc_result_get_value (res);
	if (!xmmsv_is_error (val)) {
		retval = TRUE;
	}

	xmmsc_result_unref (res);

	return retval;
}

gboolean
cli_pl_create (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gboolean switch_to;
	gchar *newplaylist;
	const gchar *copy;

	if (!command_arg_longstring_get (ctx, 0, &newplaylist)) {
		g_printf (_("Error: failed to read new playlist name!\n"));
		return FALSE;
	}

	if (!command_flag_boolean_get (ctx, "switch", &switch_to)) {
		switch_to = FALSE;
	}

	/* FIXME: Prevent overwriting existing playlists! */
	if (cli_pl_create_check_exists (infos, newplaylist)) {
		g_printf (_("Error: playlist %s already exists!\n"), newplaylist);
		return FALSE;
	}

	if (command_flag_string_get (ctx, "playlist", &copy)) {
		/* Copy the given playlist. */
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, conn, copy, XMMS_COLLECTION_NS_PLAYLISTS),
		                 XMMS_CALL_P (xmmsc_coll_save, conn, XMMS_PREV_VALUE, newplaylist, XMMS_COLLECTION_NS_PLAYLISTS));
	} else {
		/* Simply create a new empty playlist */
		XMMS_CALL (xmmsc_playlist_create, conn, newplaylist);
	}

	if (switch_to) {
		XMMS_CALL (xmmsc_playlist_load, conn, newplaylist);
	}

	g_free (newplaylist);

	return FALSE;
}

gboolean
cli_pl_rename (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gboolean force;
	const gchar *oldname;
	gchar *newname;

	if (!command_flag_boolean_get (ctx, "force", &force)) {
		force = FALSE;
	}

	if (!command_arg_longstring_get (ctx, 0, &newname)) {
		g_printf (_("Error: failed to read new playlist name!\n"));
		return FALSE;
	}

	if (!command_flag_string_get (ctx, "playlist", &oldname)) {
		oldname = cli_infos_active_playlist_name (infos);
	}

	if (force) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, conn, newname, XMMS_COLLECTION_NS_PLAYLISTS),
		                 XMMS_CALL_P (xmmsc_coll_remove, conn, newname, XMMS_COLLECTION_NS_PLAYLISTS));
	}

	XMMS_CALL (xmmsc_coll_rename, conn, oldname, newname, XMMS_COLLECTION_NS_PLAYLISTS);

	g_free (newname);

	return FALSE;
}

gboolean
cli_pl_remove (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		g_printf (_("Error: failed to read the playlist name!\n"));
		return FALSE;
	}

	/* Do not remove active playlist! */
	if (strcmp (playlist, cli_infos_active_playlist_name (infos)) == 0) {
		g_printf (_("Error: you cannot remove the active playlist!\n"));
		g_free (playlist);
		return FALSE;
	}

	XMMS_CALL (xmmsc_playlist_remove, conn, playlist);

	g_free (playlist);

	return FALSE;
}

gboolean
cli_pl_clear (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		playlist = g_strdup (cli_infos_active_playlist_name (infos));
	}

	XMMS_CALL (xmmsc_playlist_clear, conn, playlist);

	g_free (playlist);

	return FALSE;
}

gboolean
cli_pl_shuffle (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		playlist = g_strdup (cli_infos_active_playlist_name (infos));
	}

	XMMS_CALL (xmmsc_playlist_shuffle, conn, playlist);
	g_free (playlist);

	return FALSE;
}

gboolean
cli_pl_sort (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_t *orderval;
	const gchar *playlist;

	if (!command_flag_string_get (ctx, "playlist", &playlist)) {
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	if (command_arg_count (ctx) == 0) {
		orderval = xmmsv_new_list ();
		xmmsv_list_append_string (orderval, "artist");
		xmmsv_list_append_string (orderval, "album");
		xmmsv_list_append_string (orderval, "tracknr");
	} else {
		gchar **order = command_argv_get (ctx);
		orderval = xmmsv_make_stringlist (order, -1);
	}

	XMMS_CALL (xmmsc_playlist_sort, conn, playlist, orderval);

	xmmsv_unref (orderval);

	return FALSE;
}

static void
cli_pl_config_print (xmmsv_t *coll, const gchar *name)
{
	const gchar *type, *upcoming, *history, *jumplist;
	xmmsv_t *operands, *operand;

	g_printf (_("name: %s\n"), name);

	if (xmmsv_coll_attribute_get_string (coll, "type", &type))
		g_printf (_("type: %s\n"), type);

	if (xmmsv_coll_attribute_get_string (coll, "history", &history))
		g_printf (_("history: %s\n"), history);

	if (xmmsv_coll_attribute_get_string (coll, "upcoming", &upcoming))
		g_printf (_("upcoming: %s\n"), upcoming);

	operands = xmmsv_coll_operands_get (coll);
	if (xmmsv_list_get (operands, 0, &operand)) {
		if (xmmsv_coll_is_type (operand, XMMS_COLLECTION_TYPE_REFERENCE)) {
			const gchar *input_ns = NULL;
			const gchar *input = NULL;

			xmmsv_coll_attribute_get_string (operand, "namespace", &input_ns);
			xmmsv_coll_attribute_get_string (operand, "reference", &input);

			g_printf (_("input: %s/%s\n"), input_ns, input);
		}
	}

	if (xmmsv_coll_attribute_get_string (coll, "jumplist", &jumplist))
		g_printf (_("jumplist: %s\n"), jumplist);
}

static void
cli_pl_config_apply (xmmsv_t *val, cli_infos_t *infos, const gchar *playlist,
                     gint history, gint upcoming, const gchar *typestr,
                     const gchar *input, const gchar *jumplist)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	xmmsv_t *newcoll = NULL;

	/* If no type string passed, and collection didn't have any, there's no point
	 * in configuring the other attributes.
	 */
	if (typestr == NULL && !xmmsv_coll_attribute_get_string (val, "type", &typestr))
		return;

	if (typestr) {
		xmmsv_coll_attribute_set_string (val, "type", typestr);
	}
	if (history >= 0) {
		xmmsv_coll_attribute_set_int (val, "history", history);
	}
	if (upcoming >= 0) {
		xmmsv_coll_attribute_set_int (val, "upcoming", upcoming);
	}
	if (input) {
		/* Replace previous operand. */
		newcoll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_REFERENCE);
		xmmsv_coll_attribute_set_string (newcoll, "reference", input);
		xmmsv_coll_attribute_set_string (newcoll, "namespace", XMMS_COLLECTION_NS_COLLECTIONS);
	} else if (typestr && strcmp (typestr, "pshuffle") == 0 &&
	           xmmsv_list_get_size (xmmsv_coll_operands_get (val)) == 0) {
		newcoll = xmmsv_new_coll (XMMS_COLLECTION_TYPE_UNIVERSE);
	}

	if (newcoll) {
		xmmsv_list_clear (xmmsv_coll_operands_get (val));
		xmmsv_coll_add_operand (val, newcoll);
		xmmsv_unref (newcoll);
	}
	if (jumplist) {
		/* FIXME: Check for the existence of the target ? */
		xmmsv_coll_attribute_set_string (val, "jumplist", jumplist);
	}

	XMMS_CALL (xmmsc_coll_save, conn, val, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
}

gboolean
cli_pl_config (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_infos_xmms_sync (infos);
	const gchar *playlist;
	gchar *ptr = NULL;
	gint history, upcoming;
	gboolean modif = FALSE;
	const gchar *input, *jumplist, *typestr;

	history = -1;
	upcoming = -1;
	input = NULL;
	jumplist = NULL;

	/* Convert type string to type id */
	if (command_flag_string_get (ctx, "type", &typestr)) {
		modif = TRUE;
	} else {
		typestr = NULL;
	}

	if (command_flag_int_get (ctx, "history", &history)) {
		modif = TRUE;
	}
	if (command_flag_int_get (ctx, "upcoming", &upcoming)) {
		modif = TRUE;
	}

	/* FIXME: extract namespace too */
	if (command_flag_string_get (ctx, "input", &input)) {
		modif = TRUE;
	}

	if (command_flag_string_get (ctx, "jumplist", &jumplist)) {
		modif = TRUE;
	}

	if (command_arg_longstring_get (ctx, 0, &ptr)) {
		playlist = ptr;
	} else {
		playlist = cli_infos_active_playlist_name (infos);
	}

	if (modif) {
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, conn, playlist, XMMS_COLLECTION_NS_PLAYLISTS),
		                 FUNC_CALL_P (cli_pl_config_apply, XMMS_PREV_VALUE, infos, playlist, history, upcoming, typestr, input, jumplist));
	} else {
		/* Display current config of the playlist. */
		XMMS_CALL_CHAIN (XMMS_CALL_P (xmmsc_coll_get, conn, playlist, XMMS_COLLECTION_NS_PLAYLISTS),
		                 FUNC_CALL_P (cli_pl_config_print, XMMS_PREV_VALUE, playlist));
	}

	if (ptr != NULL) {
		g_free (ptr);
	}

	return FALSE;
}
