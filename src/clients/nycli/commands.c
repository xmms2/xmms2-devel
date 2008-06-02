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

#include "commands.h"

#include "cli_infos.h"
#include "cli_cache.h"
#include "command_trie.h"
#include "command_utils.h"
#include "callbacks.h"
#include "column_display.h"
#include "udata_packs.h"


/* Setup commands */

#define CLI_SIMPLE_SETUP(name, cmd, req, usage, desc) \
	void \
	cmd##_setup (command_action_t *action) \
	{ command_action_fill (action, name, cmd, req, NULL, usage, desc); }

CLI_SIMPLE_SETUP("play", cli_play,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Start playback."))
CLI_SIMPLE_SETUP("pause", cli_pause,
                 COMMAND_REQ_CONNECTION,
                 NULL,
                 _("Pause playback."))
CLI_SIMPLE_SETUP("toggle", cli_toggle, /* <<<<< */
				 COMMAND_REQ_CONNECTION,
				 NULL,
				 _("Toggle playback."))
CLI_SIMPLE_SETUP("seek", cli_seek,
                 COMMAND_REQ_CONNECTION,
                 _("<time|offset>"),
                 _("Seek to a relative or absolute position."))
CLI_SIMPLE_SETUP("prev", cli_prev,
                 COMMAND_REQ_CONNECTION,
                 _("[offset]"),
                 _("Jump to previous song."))
CLI_SIMPLE_SETUP("next", cli_next,
                 COMMAND_REQ_CONNECTION,
                 _("[offset]"),
                 _("Jump to next song."))
CLI_SIMPLE_SETUP("info", cli_info,
                 COMMAND_REQ_CONNECTION,
                 _("<pattern>"),
                 _("Display all the properties for all media matching the pattern."))
CLI_SIMPLE_SETUP("quit", cli_quit,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_NO_AUTOSTART,
                 NULL,
                 _("Terminate the server."))
CLI_SIMPLE_SETUP("exit", cli_exit,
                 COMMAND_REQ_NONE,
                 NULL,
                 _("Exit the shell-like interface."))
CLI_SIMPLE_SETUP("help", cli_help,
                 COMMAND_REQ_NONE,
                 _("[command]"),
                 _("List all commands, or help on one command."))

/* CLI_SIMPLE_SETUP("playlist list", cli_pl_list, */
/*                  COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, */
/*                  _("[pattern]"), */
/*                  _("List all playlist.")) */
CLI_SIMPLE_SETUP("playlist switch", cli_pl_switch,
                 COMMAND_REQ_CONNECTION,
                 _("<playlist>"),
                 _("Change the active playlist."))
CLI_SIMPLE_SETUP("playlist remove", cli_pl_remove,
                 COMMAND_REQ_CONNECTION,
                 _("<playlist>"),
                 _("Remove the given playlist."))
CLI_SIMPLE_SETUP("playlist clear", cli_pl_clear,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE,
                 _("[playlist]"),
                 _("Clear a playlist.  By default, clear the active playlist."))
CLI_SIMPLE_SETUP("playlist shuffle", cli_pl_shuffle,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE,
                 _("[playlist]"),
                 _("Shuffle a playlist.  By default, shuffle the active playlist."))

/* FIXME: Add all playlist commands */
/* FIXME: macro for setup with flags (+ use ##x for f/f_setup?) */

void
cli_stop_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "tracks", 'n', 0, G_OPTION_ARG_INT, NULL, _("Number of tracks after which to stop playback."), "tracks" },
		{ "time",   't', 0, G_OPTION_ARG_INT, NULL, _("Duration after which to stop playback."), "time" },
		{ NULL }
	};
	command_action_fill (action, "stop", &cli_stop, COMMAND_REQ_CONNECTION, flags,
	                     _("[-n <tracks> | -t <time>]"),
	                     "Stop playback.");
}

void
cli_jump_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "backward", 'b', 0, G_OPTION_ARG_NONE, NULL, _("Jump backward to the first track matching the pattern backwards"), NULL },
		{ NULL }
	};
	command_action_fill (action, "jump", &cli_jump, COMMAND_REQ_CONNECTION, flags,
	                     _("[-b] <pattern>"),
	                     _("Jump to the first media maching the pattern."));
}

void
cli_search_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "playlist",   'p', 0, G_OPTION_ARG_STRING, NULL, _("Search in the given playlist."), "name" },
		{ "collection", 'c', 0, G_OPTION_ARG_STRING, NULL, _("Search in the given collection."), "name" },
		{ "order",   'o', 0, G_OPTION_ARG_STRING, NULL, _("List of properties to order by (prefix by '-' for reverse ordering)."), "prop1[,prop2...]" },
		{ "columns", 'l', 0, G_OPTION_ARG_STRING, NULL, _("List of properties to use as columns."), "prop1[,prop2...]" },
		{ NULL }
	};
	command_action_fill (action, "search", &cli_search, COMMAND_REQ_CONNECTION, flags,
	                     _("[-p <name> | -c <name>] [-o <prop1[,prop2...]>] [-l <prop1[,prop2...]>] <pattern>"),
	                     _("Search and print all media matching the pattern. Search can be restricted\n"
	                       "to a collection or a playlist by using the corresponding flag."));
}

void
cli_list_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "playlist",   'p', 0, G_OPTION_ARG_STRING, NULL, _("List the given playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "list", &cli_list, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <name>] [pattern]"),
	                     _("List the contents of a playlist (the active playlist by default). If a\n"
	                       "pattern is provided, contents are further filtered and only the matching\n"
	                       "media are displayed."));
}

void
cli_add_setup (command_action_t *action)
{
	/* FIXME: support collection ? */
	/* FIXME: allow ordering ? */
	const argument_t flags[] = {
		{ "file", 'f', 0, G_OPTION_ARG_NONE, NULL, _("Treat the arguments as file paths instead of a pattern."), "path" },
		{ "non-recursive", 'N', 0, G_OPTION_ARG_NONE, NULL, _("Do not add directories recursively."), NULL },
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Add to the given playlist."), "name" },
		{ "next", 'n', 0, G_OPTION_ARG_NONE, NULL, _("Add after the current track."), NULL },
		{ "at", 'a', 0, G_OPTION_ARG_INT, NULL, _("Add media at a given position in the playlist, or at a given offset from the current track."), "pos|offset" },
		{ NULL }
	};
	command_action_fill (action, "add", &cli_add, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-f [-N]] [-p <playlist> | -c <collection>] [-n | -a <pos|offset>] [pattern | paths]"),
	                     _("Add the matching media or files to a playlist."));
}

void
cli_remove_setup (command_action_t *action)
{
	/* FIXME: support collection ? */
	const argument_t flags[] = {
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Remove from the given playlist, instead of the active playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "remove", &cli_remove, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <playlist>] <pattern>"),
	                     _("Remove the matching media from a playlist."));
}

void
cli_status_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "refresh", 'r', 0, G_OPTION_ARG_INT, NULL, _("Delay between each refresh of the status. If 0, the status is only printed once (default)."), "time" },
		{ "format",  'f', 0, G_OPTION_ARG_STRING, NULL, _("Format string used to display status."), "format" },
		{ NULL }
	};
	command_action_fill (action, "status", &cli_status, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-r <time>] [-f <format>]"),
	                     _("Display playback status, either continuously or once."));
}

void
cli_pl_create_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Copy the content of the playlist into the new playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "playlist create", &cli_pl_create, COMMAND_REQ_CONNECTION, flags,
	                     _("[-p <playlist>] <name>"),
	                     _("Change the active playlist."));
}

void
cli_pl_rename_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "playlist", 'p', 0, G_OPTION_ARG_STRING, NULL, _("Rename the given playlist."), "name" },
		{ NULL }
	};
	command_action_fill (action, "playlist rename", &cli_pl_rename, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-p <playlist>] <newname>"),
	                     _("Rename a playlist.  By default, rename the active playlist."));
}

void
cli_pl_sort_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "order",    'o', 0, G_OPTION_ARG_STRING, NULL, _("List of properties to sort by (prefix by '-' for reverse sorting)."), "prop1[,prop2...]" },
		{ NULL }
	};
	command_action_fill (action, "playlist sort", &cli_pl_sort, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-o <order>] [playlist]"),
	                     _("Sort a playlist.  By default, sort the active playlist."));
}

void
cli_pl_config_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "type",    't', 0, G_OPTION_ARG_STRING, NULL, _("Change the type of the playlist: list, queue, pshuffle."), "type" },
		{ "history", 's', 0, G_OPTION_ARG_INT, NULL, _("Size of the history of played tracks (for queue, pshuffle)."), "n" },
		{ "upcoming",'u', 0, G_OPTION_ARG_INT, NULL, _("Number of upcoming tracks to maintain (for pshuffle)."), "n" },
		{ "input",   'i', 0, G_OPTION_ARG_STRING, NULL, _("Input collection for the playlist (for pshuffle). Default to 'All Media'."), "coll" },
		{ NULL }
	};
	command_action_fill (action, "playlist config", &cli_pl_config, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
	                     _("[-t <type>] [-s <history>] [-u <upcoming>] [-i <coll>] [playlist]"),
	                     _("Configure a playlist by changing its type, attributes, etc.\nBy default, configure the active playlist."));
}


/* CLI_SIMPLE_SETUP("playlist list", cli_pl_list, */
/*                  COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, */
/*                  _("[pattern]"), */
/*                  _("List all playlist.")) */


void
cli_pl_list_setup (command_action_t *action)
{
	const argument_t flags[] = {
		{ "all",  'a', 0, G_OPTION_ARG_NONE, NULL, _("Include hidden playlists."), NULL },
		{ NULL }
	};
	command_action_fill (action, "playlist list", &cli_pl_list, COMMAND_REQ_CONNECTION | COMMAND_REQ_CACHE, flags,
						 _("[-a] [pattern]"),
						 _("List all playlists."));
} 

void
fill_column_display (cli_infos_t *infos, column_display_t *disp,
                     const gchar **columns)
{
	gint i;
	gchar *nextsep = NULL;

	for (i = 0; columns[i]; ++i) {
		/* Separator between columns */
		if (nextsep) {
			column_display_add_separator (disp, nextsep);
		}
		nextsep = "| ";

		/* FIXME: Allow flags to change alignment */

		if (strcmp (columns[i], "id") == 0) {
			column_display_add_property (disp, columns[i], columns[i], 5, TRUE,
			                             COLUMN_DEF_ALIGN_LEFT);
		} else if (strcmp (columns[i], "pos") == 0) {
			column_display_add_special (disp, "pos", NULL, 5,
			                            COLUMN_DEF_ALIGN_RIGHT,
			                            column_display_render_position);
			nextsep = "/";
		} else if (strcmp (columns[i], "curr") == 0) {
			column_display_add_special (disp, "",
			                            GINT_TO_POINTER(infos->cache->currpos),
			                            2, COLUMN_DEF_ALIGN_LEFT,
			                            column_display_render_highlight);
			nextsep = FALSE;
		} else {
			column_display_add_property (disp, columns[i], columns[i], 20,
			                             FALSE, COLUMN_DEF_ALIGN_LEFT);
		}
	}
}

column_display_t *
create_column_display (cli_infos_t *infos, command_context_t *ctx,
                       const gchar **default_columns)
{
	const gchar **columns = NULL;
	column_display_t *coldisp;

	coldisp = column_display_init (infos);
	command_flag_stringlist_get (ctx, "columns", &columns);
	if (columns) {
		fill_column_display (infos, coldisp, columns);
	} else {
		fill_column_display (infos, coldisp, default_columns);
	}

	g_free (columns);

	return coldisp;
}


/* Define commands */

gboolean
cli_play (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_start (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean
cli_pause (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_pause (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean
cli_stop (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gint n;

	/* FIXME: Support those flags */
	if (command_flag_int_get (ctx, "tracks", &n) && n != 0) {
		g_printf (_("--tracks flag not supported yet!\n"));
	}
	if (command_flag_int_get (ctx, "time", &n) && n != 0) {
		g_printf (_("--time flag not supported yet!\n"));
	}

	res = xmmsc_playback_stop (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

/* <<<<< */
gboolean 
cli_toggle  (cli_infos_t *infos, command_context_t *ctx)
{
  uint32_t status;
  xmmsc_result_t *res;

  res = xmmsc_playback_status (infos->conn);
  xmmsc_result_wait (res);

  if (xmmsc_result_iserror (res)) {
    g_printf (_("Error: Couldn't get playback status: %s"),
		 xmmsc_result_get_error (res));
  }

  if (!xmmsc_result_get_uint(res, &status)) {
    g_printf (_("Error: Broken resultset"));
  }

  if (status == XMMS_PLAYBACK_STATUS_PLAY) {
    cli_pause(infos, ctx);
  } else {
    cli_play(infos, ctx);
  }
	
  xmmsc_result_unref(res);

  return TRUE;
}

gboolean
cli_seek (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	command_arg_time_t t;

	if (command_arg_time_get (ctx, 0, &t)) {
		if (t.type == COMMAND_ARG_TIME_OFFSET) {
			res = xmmsc_playback_seek_ms_rel (infos->conn, t.value.offset * 1000);
		} else {
			res = xmmsc_playback_seek_ms (infos->conn, t.value.pos * 1000);
		}

		xmmsc_result_notifier_set (res, cb_done, infos);
		xmmsc_result_unref (res);
	} else {
		g_printf (_("Error: failed to parse the time argument!\n"));
		return FALSE;
	}

	return TRUE;
}

gboolean
cli_status (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	guint currid;
	gchar *f;
	gint r;

	/* FIXME: Support advanced flags */
	if (command_flag_int_get (ctx, "refresh", &r)) {
		g_printf (_("--refresh flag not supported yet!\n"));
	}

	if (command_flag_string_get (ctx, "format", &f)) {
		g_printf (_("--format flag not supported yet!\n"));
	}

	currid = g_array_index (infos->cache->active_playlist, guint,
	                        infos->cache->currpos);

	res = xmmsc_medialib_get_info (infos->conn, currid);
	xmmsc_result_notifier_set (res, cb_entry_print_status, infos);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean
cli_prev (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gint n;
	gint offset = 1;

	if (command_arg_int_get (ctx, 0, &n)) {
		offset = n;
	}

	res = xmmsc_playlist_set_next_rel (infos->conn, - offset);
	xmmsc_result_notifier_set (res, cb_tickle, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean
cli_next (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gint n;
	gint offset = 1;

	if (command_arg_int_get (ctx, 0, &n)) {
		offset = n;
	}

	res = xmmsc_playlist_set_next_rel (infos->conn, offset);
	xmmsc_result_notifier_set (res, cb_tickle, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean
cli_jump (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	xmmsc_coll_t *query;
	gboolean backward;

	if (!command_flag_boolean_get (ctx, "backward", &backward)) {
		backward = FALSE;
	}

	if (command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		/* FIXME: benchmark if efficient to reduce query to Active playlist */
		res = xmmsc_coll_query_ids (infos->conn, query, NULL, 0, 0);
		if (backward) {
			xmmsc_result_notifier_set (res, cb_list_jump_back, infos);
		} else {
			xmmsc_result_notifier_set (res, cb_list_jump, infos);
		}
		xmmsc_result_unref (res);
		xmmsc_coll_unref (query);
	}

	return TRUE;
}

gboolean
cli_search (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_coll_t *query;
	xmmsc_result_t *res;
	column_display_t *coldisp;
	const gchar **order = NULL;
	const gchar *default_columns[] = { "id", "artist", "album", "title", NULL };

	/* FIXME: Support arguments -p and -c */

	if (command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		coldisp = create_column_display (infos, ctx, default_columns);
		command_flag_stringlist_get (ctx, "order", &order);

		res = xmmsc_coll_query_ids (infos->conn, query, order, 0, 0);
		xmmsc_result_notifier_set (res, cb_list_print_row, coldisp);
		xmmsc_result_unref (res);
		xmmsc_coll_unref (query);
	}

	g_free (order);

	return TRUE;
}

gboolean
cli_list (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *pattern = NULL;
	xmmsc_coll_t *query = NULL;
	xmmsc_result_t *res;
	column_display_t *coldisp;
	gchar *playlist = NULL;
	const gchar *default_columns[] = { "curr", "pos", "id", "artist", "album",
	                                   "title", NULL };

	command_arg_longstring_get (ctx, 0, &pattern);
	if (pattern) {
		if (!xmmsc_coll_parse (pattern, &query)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
			g_free (pattern);
			return FALSE;
		}
		/* FIXME: support filtering */
	}

	/* Default to active playlist (from cache) */
	command_flag_string_get (ctx, "playlist", &playlist);
	if (!playlist
	    || strcmp (playlist, infos->cache->active_playlist_name) == 0) {
		/* FIXME: Optim by reading data from cache */
		playlist = XMMS_ACTIVE_PLAYLIST;
	}

	coldisp = create_column_display (infos, ctx, default_columns);

	res = xmmsc_playlist_list_entries (infos->conn, playlist);
	xmmsc_result_notifier_set (res, cb_list_print_row, coldisp);
	xmmsc_result_unref (res);
	/* FIXME: if not null, xmmsc_coll_unref (query); */

	g_free (pattern);

	return TRUE;
}

gboolean
cli_info (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_coll_t *query;
	xmmsc_result_t *res;

	if (command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		res = xmmsc_coll_query_ids (infos->conn, query, NULL, 0, 0);
		xmmsc_result_notifier_set (res, cb_list_print_info, infos);
		xmmsc_result_unref (res);
		xmmsc_coll_unref (query);
	}

	return TRUE;
}


static gboolean
cmd_flag_pos_get (cli_infos_t *infos, command_context_t *ctx, gint *pos) {
	gboolean next;
	gint at;
	gboolean at_isset;

	command_flag_boolean_get (ctx, "next", &next);
	at_isset = command_flag_int_get (ctx, "at", &at);

	if (next && at_isset) {
		g_printf (_("Error: --next and --at are mutually exclusive!\n"));
		return FALSE;
	} else if (next) {
		*pos = infos->cache->currpos + 1;
	} else if (at_isset) {
		/* FIXME: handle relative values ? */
		if (at > 0 && at > infos->cache->active_playlist->len) { /* int vs uint */
			g_printf (_("Error: specified position is outside the playlist!\n"));
			return FALSE;
		} else {
			*pos = at - 1;  /* playlist ids start at 0 */
		}
	} else {
		/* No flag given, just enqueue */
		*pos = infos->cache->active_playlist->len;
	}

	return TRUE;
}

/* Transform a path (possibly absolute or relative) into a valid XMMS2
 * path with protocol prefix. The resulting string must be freed
 * manually.
 */
static gchar *
make_valid_url (gchar *path)
{
	gchar *p;
	gchar *url;
	gchar *pwd;

	/* Check if path matches "^[a-z]+://" */
	for (p = path; *p >= 'a' && *p <= 'z'; ++p);
	if (*p == ':' && *(++p) == '/' && *(++p) == '/') {
		url = g_strdup (path);
	} else if (*path == '/') {
		/* Absolute url, just prepend file:// protocol */
		url = g_strconcat ("file://", path, NULL);
	} else {
		/* Relative url, prepend file:// protocol and PWD */
		pwd = getenv ("PWD");
		url = g_strconcat ("file://", pwd, "/", path, NULL);
	}

	return url;
}


gboolean
cli_add (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *pattern = NULL;
	gchar *playlist = NULL;
	xmmsc_coll_t *query;
	xmmsc_result_t *res;
	gint pos;
	gchar *path, *fullpath;
	gboolean fileargs;
	gboolean norecurs;
	pack_infos_playlist_pos_t *pack;
	gint i, count;
	gboolean success = TRUE;

/*
--file  Add a path from the local filesystem
--non-recursive  Do not add directories recursively.
--playlist  Add to the given playlist.
--next  Add after the current track.
--at  Add media at a given position in the playlist, or at a given offset from the current track.
*/

	command_flag_string_get (ctx, "playlist", &playlist);
	if (!playlist) {
		playlist = NULL;
	}

	/* FIXME: pos is wrong in non-active playlists (next/offsets are invalid)! */
	/* FIXME: offsets not supported (need to identify positive offsets) :-( */
	if (!cmd_flag_pos_get (infos, ctx, &pos)) {
		success = FALSE;
		goto finish;
	}

	command_flag_boolean_get (ctx, "file", &fileargs);
	command_flag_boolean_get (ctx, "non-recursive", &norecurs);
	command_arg_longstring_get (ctx, 0, &pattern);

	/* We need either a file or a pattern! */
	if (!pattern) {
		g_printf (_("Error: you must provide a pattern or files to add!\n"));
		success = FALSE;
		goto finish;
	}

	if (fileargs) {
		/* FIXME: expend / glob? */
		for (i = 0, count = command_arg_count (ctx); i < count; ++i) {
			command_arg_string_get (ctx, i, &path);
			fullpath = make_valid_url (path);

			if (norecurs) {
				res = xmmsc_playlist_insert_url (infos->conn, playlist, pos, fullpath);
				if (i == count - 1) {
					/* Finish after last add */
					xmmsc_result_notifier_set (res, cb_done, infos);
				}
				xmmsc_result_unref (res);
			} else {
				/* FIXME: oops, there is no rinsert */
				g_printf (_("Error: no playlist_rinsert, implement it! doing non-recursive..\n"));
				res = xmmsc_playlist_insert_url (infos->conn, playlist, pos, fullpath);
				xmmsc_result_notifier_set (res, cb_done, infos);
				xmmsc_result_unref (res);
			}

			g_free (fullpath);
		}
	} else {
		if (norecurs) {
			g_printf (_("Error: --non-recursive only applies when passing --file!\n"));
			success = FALSE;
			goto finish;
		}

		if (!xmmsc_coll_parse (pattern, &query)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
			success = FALSE;
			goto finish;
		} else {
			pack = pack_infos_playlist_pos (infos, playlist, pos);
			res = xmmsc_coll_query_ids (infos->conn, query, NULL, 0, 0);
			xmmsc_result_notifier_set (res, cb_add_list, pack);
			xmmsc_result_unref (res);
			xmmsc_coll_unref (query);
		}
	}

	finish:

	g_free (pattern);

	return success;
}



gboolean
cli_remove (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *playlist = NULL;
	xmmsc_coll_t *query;
	xmmsc_result_t *res, *plres;

	command_flag_string_get (ctx, "playlist", &playlist);
	if (!playlist
	    || strcmp (playlist, infos->cache->active_playlist_name) == 0) {
		playlist = NULL;
	}

	if (command_arg_pattern_get (ctx, 0, &query, TRUE)) {
		res = xmmsc_coll_query_ids (infos->conn, query, NULL, 0, 0);
		if (!playlist) {
			/* Optimize by reading active playlist from cache */
			xmmsc_result_notifier_set (res, cb_remove_cached_list, infos);
		} else {
			plres = xmmsc_playlist_list_entries (infos->conn, playlist);
			register_double_callback (res, plres, cb_remove_list,
			                          pack_infos_playlist (infos, playlist));
			xmmsc_result_unref (plres);
		}
		xmmsc_result_unref (res);
		xmmsc_coll_unref (query);
	}

	return TRUE;
}

gboolean
cli_pl_list (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gboolean all;

	/* FIXME: support pattern argument (only display playlist containing matching media) */
	/* FIXME: --all flag to show hidden playlists */

	res = xmmsc_playlist_list (infos->conn);
	command_flag_boolean_get (ctx, "all", &all);

	if (all) {
		xmmsc_result_notifier_set (res, cb_list_print_all_playlists, infos);
	} else {
		xmmsc_result_notifier_set (res, cb_list_print_playlists, infos);
	}
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean
cli_pl_switch (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		g_printf (_("Error: failed to read new playlist name!\n"));
		return FALSE;
	}

	res = xmmsc_playlist_load (infos->conn, playlist);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	g_free (playlist);

	return TRUE;
}

gboolean
cli_pl_create (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *newplaylist, *copy;

	if (!command_arg_longstring_get (ctx, 0, &newplaylist)) {
		g_printf (_("Error: failed to read new playlist name!\n"));
		return FALSE;
	}

	/* FIXME: Prevent overwriting existing playlists! */

	if (command_flag_string_get (ctx, "playlist", &copy)) {
		/* Copy the given playlist. */
		res = xmmsc_coll_get (infos->conn, copy, XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_notifier_set (res, cb_copy_playlist,
		                           pack_infos_playlist (infos, newplaylist));
		xmmsc_result_unref (res);
	} else {
		/* Simply create a new empty playlist */
		res = xmmsc_playlist_create (infos->conn, newplaylist);
		xmmsc_result_notifier_set (res, cb_done, infos);
		xmmsc_result_unref (res);
	}

	g_free (newplaylist);

	return TRUE;
}

gboolean
cli_pl_rename (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *oldname, *newname;

	if (!command_arg_longstring_get (ctx, 0, &newname)) {
		g_printf (_("Error: failed to read new playlist name!\n"));
		return FALSE;
	}

	if (!command_flag_string_get (ctx, "playlist", &oldname)) {
		oldname = infos->cache->active_playlist_name;
	}

	res = xmmsc_coll_rename (infos->conn, oldname, newname,
	                         XMMS_COLLECTION_NS_PLAYLISTS);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	g_free (newname);

	return TRUE;
}

gboolean
cli_pl_remove (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		g_printf (_("Error: failed to read the playlist name!\n"));
		return FALSE;
	}

	/* Do not remove active playlist! */
	if (strcmp (playlist, infos->cache->active_playlist_name) == 0) {
		g_printf (_("Error: you cannot remove the active playlist!\n"));
		g_free (playlist);
		return FALSE;
	}

	res = xmmsc_playlist_remove (infos->conn, playlist);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	g_free (playlist);

	return TRUE;
}

gboolean
cli_pl_clear (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		playlist = infos->cache->active_playlist_name;
	}

	res = xmmsc_playlist_clear (infos->conn, playlist);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	g_free (playlist);

	return TRUE;
}

gboolean
cli_pl_shuffle (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *playlist;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		playlist = infos->cache->active_playlist_name;
	}

	res = xmmsc_playlist_shuffle (infos->conn, playlist);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	g_free (playlist);

	return TRUE;
}

gboolean
cli_pl_sort (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *playlist;
	const gchar **order = NULL;

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		playlist = infos->cache->active_playlist_name;
	}

	if (!command_flag_stringlist_get (ctx, "order", &order)) {
		/* FIXME: Default ordering */
		g_free (playlist);
		return FALSE;
	}

	res = xmmsc_playlist_sort (infos->conn, playlist, order);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	g_free (playlist);
	g_free (order);

	return TRUE;
}

gboolean
cli_pl_config (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *playlist;
	gint history, upcoming;
	xmmsc_coll_type_t type;
	gchar *typestr, *input;
	pack_infos_playlist_config_t *pack;
	gboolean modif = FALSE;

	history = -1;
	upcoming = -1;
	type = -1;
	input = NULL;

	/* Convert type string to type id */
	if (command_flag_string_get (ctx, "type", &typestr)) {
		if (strcmp (typestr, "list") == 0) {
			type = XMMS_COLLECTION_TYPE_IDLIST;
		} else if (strcmp (typestr, "queue") == 0) {
			type = XMMS_COLLECTION_TYPE_QUEUE;
		} else if (strcmp (typestr, "pshuffle") == 0) {
			type = XMMS_COLLECTION_TYPE_PARTYSHUFFLE;
		} else {
			g_printf ("Invalid playlist type: '%s'!\n", typestr);
			return FALSE;
		}
		modif = TRUE;
	}

	if (command_flag_int_get (ctx, "history", &history)) {
		if (type != XMMS_COLLECTION_TYPE_QUEUE &&
		    type != XMMS_COLLECTION_TYPE_PARTYSHUFFLE) {
			g_printf ("--history flag only valid for "
			          "queue and pshuffle playlists!\n");
			return FALSE;
		}
		modif = TRUE;
	}
	if (command_flag_int_get (ctx, "upcoming", &upcoming)) {
		if (type != XMMS_COLLECTION_TYPE_PARTYSHUFFLE) {
			g_printf ("--upcoming flag only valid for pshuffle playlists!\n");
			return FALSE;
		}
		modif = TRUE;
	}

	/* FIXME: extract namespace too */
	if (command_flag_string_get (ctx, "input", &input)) {
		if (type != XMMS_COLLECTION_TYPE_PARTYSHUFFLE) {
			g_printf ("--input flag only valid for pshuffle playlists!\n");
			return FALSE;
		}
		modif = TRUE;
	} else if (type == XMMS_COLLECTION_TYPE_PARTYSHUFFLE) {
		/* Default to All Media if no input provided. */
		/* FIXME: Don't overwrite with this if already a pshuffle! */
		input = "All Media";
	}

	if (!command_arg_longstring_get (ctx, 0, &playlist)) {
		playlist = infos->cache->active_playlist_name;
	}

	if (modif) {
		/* Send the previous coll_t for update. */
		pack = pack_infos_playlist_config (infos, playlist, history, upcoming,
		                                   type, input);
		res = xmmsc_coll_get (infos->conn, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_notifier_set (res, cb_configure_playlist, pack);
		xmmsc_result_unref (res);
	} else {
		/* Display current config of the playlist. */
		res = xmmsc_coll_get (infos->conn, playlist, XMMS_COLLECTION_NS_PLAYLISTS);
		xmmsc_result_notifier_set (res, cb_playlist_print_config,
		                           pack_infos_playlist (infos, playlist));
		xmmsc_result_notifier_set (res, cb_done, infos);
		xmmsc_result_unref (res);
	}

	return TRUE;
}


/* The loop is resumed in the disconnect callback */
gboolean
cli_quit (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;

	if (infos->conn) {
		res = xmmsc_quit (infos->conn);
		xmmsc_result_unref (res);
	} else {
		return FALSE;
	}

	return TRUE;
}

gboolean
cli_exit (cli_infos_t *infos, command_context_t *ctx)
{
	cli_infos_loop_stop (infos);

	return TRUE;
}


static void
help_short_command (gpointer elem, gpointer udata)
{
	gchar *cmdname = (gchar *)elem;
	/* FIXME: if contains space, change to <subcommand>, then allow 'help playlist' */
	g_printf ("   %s\n", cmdname);
}

static void
help_all_commands (cli_infos_t *infos)
{
	g_printf (_("usage: nyxmms2 <command> [args]\n\n"));
	g_printf (_("Available commands:\n"));
	g_list_foreach (infos->cmdnames, help_short_command, NULL);
	g_printf (_("\nType 'help <command>' for detailed help about a command.\n"));
}

void
help_command (cli_infos_t *infos, gchar **cmd, gint num_args)
{
	command_action_t *action;
	gint i, k;
	gint padding, max_flag_len = 0;

	gchar **argv = cmd;
	gint argc = num_args;

	action = command_trie_find (infos->commands, &argv, &argc,
	                            AUTO_UNIQUE_COMPLETE);
	if (action) {
		g_printf (_("usage: %s"), action->name);
		if (action->usage) {
			g_printf (" %s", action->usage);
		}
		g_printf ("\n\n  %s\n\n", action->description);
		if (action->argdefs && action->argdefs[0].long_name) {
			/* Find length of longest option */
			for (i = 0; action->argdefs[i].long_name; ++i) {
				if (max_flag_len < strlen (action->argdefs[i].long_name)) {
					max_flag_len = strlen (action->argdefs[i].long_name);
				}
			}

			g_printf (_("Valid options:\n"));
			for (i = 0; action->argdefs[i].long_name; ++i) {
				padding = max_flag_len - strlen (action->argdefs[i].long_name) + 2;

				if (action->argdefs[i].short_name) {
					g_printf ("  -%c, ", action->argdefs[i].short_name);
				} else {
					g_printf ("      ");
				}

				g_printf ("--%s", action->argdefs[i].long_name);

				for (k = 0; k < padding; ++k) {
					g_printf (" ");
				}
				g_printf ("%s\n", action->argdefs[i].description);
				/* FIXME: align multi-line */
			}
		}
	} else {
		/* FIXME: Better handle help for subcommands! */
		g_printf (_("Unknown command: '"));
		for (i = 0; i < num_args; ++i) {
			if (i > 0) g_printf (" ");
			g_printf ("%s", cmd[i]);
		}
		g_printf (_("'\n"));
		g_printf (_("Type 'help' for the list of commands.\n"));
	}
}

gboolean
cli_help (cli_infos_t *infos, command_context_t *ctx)
{
	gint num_args;

	num_args = command_arg_count (ctx);

	/* No argument, display the list of commands */
	if (num_args == 0) {
		help_all_commands (infos);
	} else {
		help_command (infos, command_argv_get (ctx), num_args);
	}

	/* No data pending */
	return FALSE;
}
