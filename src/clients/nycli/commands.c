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
#include "command_trie.h"
#include "command_utils.h"
#include "callbacks.h"


/* Setup commands */

#define CLI_SIMPLE_SETUP(setupcmd, name, cmd, needconn) \
	gboolean \
	setupcmd (command_trie_t *trie) \
	{ command_trie_insert (trie, name, cmd, needconn, NULL); }

CLI_SIMPLE_SETUP(cli_play_setup, "play", cli_play, TRUE)
CLI_SIMPLE_SETUP(cli_pause_setup, "pause", cli_pause, TRUE)
CLI_SIMPLE_SETUP(cli_prev_setup, "prev", cli_prev, TRUE)
CLI_SIMPLE_SETUP(cli_next_setup, "next", cli_next, TRUE)
CLI_SIMPLE_SETUP(cli_info_setup, "info", cli_info, TRUE)
CLI_SIMPLE_SETUP(cli_quit_setup, "quit", cli_quit, FALSE)
CLI_SIMPLE_SETUP(cli_exit_setup, "exit", cli_exit, FALSE)
CLI_SIMPLE_SETUP(cli_help_setup, "help", cli_help, FALSE)

gboolean
cli_stop_setup (command_trie_t *trie)
{
	const argument_t flags[] = {
		{ "tracks", 'n', 0, G_OPTION_ARG_INT, NULL, _("Number of tracks after which to stop playback."), "num" },
		{ "time",   't', 0, G_OPTION_ARG_INT, NULL, _("Duration after which to stop playback."), "time" },
		{ NULL }
	};
	command_trie_insert (trie, "stop", &cli_stop, TRUE, flags);
}

gboolean
cli_status_setup (command_trie_t *trie)
{
	const argument_t flags[] = {
		{ "refresh", 'r', 0, G_OPTION_ARG_INT, NULL, _("Delay between each refresh of the status. If 0, the status is only printed once (default)."), "time" },
		{ "format",  'f', 0, G_OPTION_ARG_STRING, NULL, _("Format string used to display status."), "format" },
		{ NULL }
	};
	command_trie_insert (trie, "status", &cli_status, TRUE, flags);
}


/* Define commands */

gboolean cli_play (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_start (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
	return TRUE;
}

gboolean cli_pause (cli_infos_t *infos, command_context_t *ctx)
{
	xmmsc_result_t *res;
	res = xmmsc_playback_pause (infos->conn);
	xmmsc_result_notifier_set (res, cb_done, infos);
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

	return TRUE;
}

gboolean cli_status (cli_infos_t *infos, command_context_t *ctx)
{
	command_argument_t *arg;
	gchar *f;
	gint r;

	if (command_flag_int_get (ctx, "refresh", &r)) {
		g_printf ("refresh=%d\n", r);
	}

	if (command_flag_string_get (ctx, "format", &f)) {
		g_printf ("format='%s'\n", f);
	}

	cli_infos_loop_resume (infos);

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
		xmmsc_coll_unref (query);
	}

	if (pattern) {
		g_free (pattern);
	}

	return TRUE;
}


gboolean cli_quit (cli_infos_t *infos, command_context_t *ctx)
{
	/* FIXME: Actually we need a connection. We just don't want to
	 * start it for nothing. */
	xmmsc_quit (infos->conn);

	cli_infos_loop_resume (infos);

	return TRUE;
}

gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx)
{
	cli_infos_loop_stop (infos);

	return TRUE;
}


void
help_all_commands (cli_infos_t *infos)
{
	gint i;

	g_printf (_("usage: nyxmms2 COMMAND [ARGS]\n\n"));
	/* FIXME: we need the list of commands, 'd be easier
	for (i = 0; commands[i].name; ++i) {
		g_printf ("   %s\n", commands[i].name);
	}
	*/
	g_printf (_("\nType 'help COMMAND' for detailed help about a command.\n"));
}

void
help_command (cli_infos_t *infos, gchar *cmd)
{
	command_action_t *action;
	gint i;

	action = command_trie_find (infos->commands, cmd);
	if (action) {
		/* FIXME: show REAL name (get it somewhere), usage, description */
		g_printf ("%s\n\n", cmd);
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
		/* FIXME: find command, if found display help, else show error */
		gchar *cmd;

		if (command_arg_string_get (ctx, 0, &cmd)) {
			help_command (infos, cmd);
		} else {
			g_printf (_("Error while parsing the argument!\n"));
		}
	} else {
		g_printf (_("To get help for a command, type 'help COMMAND'.\n"));
	}

	cli_infos_loop_resume (infos);

	return TRUE;
}
