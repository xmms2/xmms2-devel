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



column_display_t *
create_column_display (cli_infos_t *infos, command_context_t *ctx)
{
	const gchar **columns = NULL;
	column_display_t *coldisp;

	coldisp = column_display_init (infos);
	command_flag_stringlist_get (ctx, "columns", &columns);
	if (columns) {
		column_display_fill (coldisp, columns);
	} else {
		column_display_fill_default (coldisp);
	}

	/* FIXME: woops, cannot prefix order by '-' as this is a flag! user another? '!' ? */

	return coldisp;
}


/* Define commands */

gboolean cli_play (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_start (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean cli_pause (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_pause (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean cli_stop (cli_infos_t *infos, command_context_t *ctx)
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

gboolean cli_seek (cli_infos_t *infos, command_context_t *ctx)
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
		cli_infos_loop_resume (infos);
	}

	return TRUE;
}

gboolean cli_status (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	guint currid;
	gchar *f;
	gint r;

	/* FIXME: Support advanced flags */
	if (command_flag_int_get (ctx, "refresh", &r)) {
		g_printf ("refresh=%d\n", r);
	}

	if (command_flag_string_get (ctx, "format", &f)) {
		g_printf ("format='%s'\n", f);
	}

	currid = g_array_index (infos->cache->active_playlist, guint,
	                        infos->cache->currpos);
	res = xmmsc_medialib_get_info (infos->conn, currid);
	xmmsc_result_notifier_set (res, cb_entry_print_status, infos);
	xmmsc_result_notifier_set (res, cb_done, infos);
	xmmsc_result_unref (res);

	return TRUE;
}

gboolean cli_prev (cli_infos_t *infos, command_context_t *ctx)
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

gboolean cli_next (cli_infos_t *infos, command_context_t *ctx)
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

gboolean cli_jump (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	gchar *pattern = NULL;
	xmmsc_coll_t *query;
	gboolean backward;

	if (!command_flag_boolean_get (ctx, "backward", &backward)) {
		backward = FALSE;
	}

	command_arg_longstring_get (ctx, 0, &pattern);
	if (!pattern) {
		g_printf (_("Error: you must provide a pattern!\n"));
		cli_infos_loop_resume (infos);
	} else if (!xmmsc_coll_parse (pattern, &query)) {
		g_printf (_("Error: failed to parse the pattern!\n"));
		cli_infos_loop_resume (infos);
	} else {
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

gboolean cli_search (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *pattern = NULL;
	xmmsc_coll_t *query;
	xmmsc_result_t *res;
	column_display_t *coldisp;
	const gchar **order = NULL;

	/* FIXME: Support arguments -p and -c */

	command_arg_longstring_get (ctx, 0, &pattern);
	if (!pattern) {
		g_printf (_("Error: you must provide a pattern!\n"));
		cli_infos_loop_resume (infos);
	} else if (!xmmsc_coll_parse (pattern, &query)) {
		g_printf (_("Error: failed to parse the pattern!\n"));
		cli_infos_loop_resume (infos);
	} else {
		coldisp = create_column_display (infos, ctx);
		command_flag_stringlist_get (ctx, "order", &order);

		res = xmmsc_coll_query_ids (infos->conn, query, order, 0, 0);
		xmmsc_result_notifier_set (res, cb_list_print_row, coldisp);
		xmmsc_result_unref (res);
		xmmsc_coll_unref (query);
	}

	if (pattern) {
		g_free (pattern);
	}

	return TRUE;
}

gboolean cli_list (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *pattern = NULL;
	xmmsc_coll_t *query = NULL;
	xmmsc_result_t *res;
	column_display_t *coldisp;
	gchar *playlist = NULL;

	command_arg_longstring_get (ctx, 0, &pattern);
	if (pattern) {
		if (!xmmsc_coll_parse (pattern, &query)) {
			g_printf (_("Error: failed to parse the pattern!\n"));
			cli_infos_loop_resume (infos);
			return;
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

	coldisp = create_column_display (infos, ctx);

	res = xmmsc_playlist_list_entries (infos->conn, playlist);
	xmmsc_result_notifier_set (res, cb_list_print_row, coldisp);
	xmmsc_result_unref (res);
	/* FIXME: if not null, xmmsc_coll_unref (query); */

	if (pattern) {
		g_free (pattern);
	}

	return TRUE;
}

gboolean cli_info (cli_infos_t *infos, command_context_t *ctx)
{
	gchar *pattern = NULL;
	xmmsc_coll_t *query;
	xmmsc_result_t *res;

	command_arg_longstring_get (ctx, 0, &pattern);
	if (!pattern) {
		g_printf (_("Error: you must provide a pattern!\n"));
		cli_infos_loop_resume (infos);
	} else if (!xmmsc_coll_parse (pattern, &query)) {
		g_printf (_("Error: failed to parse the pattern!\n"));
		cli_infos_loop_resume (infos);
	} else {
		res = xmmsc_coll_query_ids (infos->conn, query, NULL, 0, 0);
		xmmsc_result_notifier_set (res, cb_list_print_info, infos);
		xmmsc_result_unref (res);
		xmmsc_coll_unref (query);
	}

	if (pattern) {
		g_free (pattern);
	}

	return TRUE;
}

/* The loop is resumed in the disconnect callback */
gboolean cli_quit (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;

	if (infos->conn) {
		res = xmmsc_quit (infos->conn);
		xmmsc_result_unref (res);
	} else {
		cli_infos_loop_resume (infos);
	}

	return TRUE;
}

gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx)
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
	gint i;

	action = command_trie_find (infos->commands, cmd);
	if (action) {
		g_printf (_("usage: %s"), action->name);
		if (action->usage) {
			g_printf (" %s", action->usage);
		}
		g_printf ("\n\n  %s\n\n", action->description);
		if (action->argdefs && action->argdefs[0].long_name) {
			g_printf (_("Valid options:\n"));
			for (i = 0; action->argdefs[i].long_name; ++i) {
				if (action->argdefs[i].short_name) {
					g_printf ("  -%c, ", action->argdefs[i].short_name);
				} else {
					g_printf ("      ");
				}
				g_printf ("--%s", action->argdefs[i].long_name);
				g_printf ("  %s\n", action->argdefs[i].description);
				/* FIXME: align, show arg_description, etc */
			}
		}
	} else {
		g_printf (_("Unknown command: '%s'\n"), cmd);
		g_printf (_("Type 'help' for the list of commands.\n"));
	}
}

gboolean cli_help (cli_infos_t *infos, command_context_t *ctx)
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

	cli_infos_loop_resume (infos);

	return TRUE;
}
