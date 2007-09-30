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

#define CLI_SIMPLE_SETUP(setupcmd, name, cmd, req, usage, desc) \
	void \
	setupcmd (command_action_t *action) \
	{ command_action_fill (action, name, cmd, req, NULL, usage, desc); }

CLI_SIMPLE_SETUP(cli_play_setup, "play", cli_play, COMMAND_REQ_CONNECTION, NULL,
                 _("Start playback."))
CLI_SIMPLE_SETUP(cli_pause_setup, "pause", cli_pause, COMMAND_REQ_CONNECTION, NULL,
                 _("Pause playback."))
CLI_SIMPLE_SETUP(cli_seek_setup, "seek", cli_seek, COMMAND_REQ_CONNECTION, _("<time|offset>"),
                 _("Seek to a relative or absolute position."))
CLI_SIMPLE_SETUP(cli_prev_setup, "prev", cli_prev, COMMAND_REQ_CONNECTION, _("[offset]"),
                 _("Jump to previous song."))
CLI_SIMPLE_SETUP(cli_next_setup, "next", cli_next, COMMAND_REQ_CONNECTION, _("[offset]"),
                 _("Jump to next song."))
CLI_SIMPLE_SETUP(cli_info_setup, "info", cli_info, COMMAND_REQ_CONNECTION, _("<pattern>"),
                 _("Display all the properties for all media matching the pattern."))
CLI_SIMPLE_SETUP(cli_quit_setup, "quit", cli_quit,
                 COMMAND_REQ_CONNECTION | COMMAND_REQ_NO_AUTOSTART, NULL,
                 _("Terminate the server."))
CLI_SIMPLE_SETUP(cli_exit_setup, "exit", cli_exit, COMMAND_REQ_NONE, NULL, _("Exit the shell-like interface."))
CLI_SIMPLE_SETUP(cli_help_setup, "help", cli_help, COMMAND_REQ_NONE, _("[command]"),
                 _("List all commands, or help on one command."))

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
	xmmsc_result_t *res, *plres;
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
help_command (cli_infos_t *infos, gchar *cmd)
{
	command_action_t *action;
	gint i, k;
	gint padding, max_flag_len = 0;

	action = command_trie_find (infos->commands, cmd);
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
		g_printf (_("Unknown command: '%s'\n"), cmd);
		g_printf (_("Type 'help' for the list of commands.\n"));
	}
}

gboolean
cli_help (cli_infos_t *infos, command_context_t *ctx)
{
	gint i;

	/* No argument, display the list of commands */
	if (command_arg_count (ctx) == 0) {
		help_all_commands (infos);
	} else if (command_arg_count (ctx) == 1) {
		gchar *cmd;

		if (command_arg_string_get (ctx, 0, &cmd)) {
			help_command (infos, cmd);
		} else {
			g_printf (_("Error while parsing the argument!\n"));
		}
	} else {
		g_printf (_("To get help for a command, type 'help <command>'.\n"));
	}

	/* No data pending */
	return FALSE;
}
