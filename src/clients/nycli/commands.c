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


static gboolean command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v);
static gboolean command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v);


/* FIXME: Can we differentiate between unset, set and default value? :-/ */
gboolean
command_flag_int_get (command_context_t *ctx, const gchar *name, gint *v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_INT) {
		*v = arg->value.vint;
		retval = TRUE;
	}

	return retval;
}

gboolean
command_flag_string_get (command_context_t *ctx, const gchar *name, gchar **v)
{
	command_argument_t *arg;
	gboolean retval = FALSE;

	arg = (command_argument_t *) g_hash_table_lookup (ctx->flags, name);
	if (arg && arg->type == COMMAND_ARGUMENT_TYPE_STRING) {
		*v = arg->value.vstring;
		retval = TRUE;
	}

	return retval;
}

gint
command_arg_count (command_context_t *ctx)
{
	return ctx->argc - 1;
}

gboolean
command_arg_int_get (command_context_t *ctx, gint at, gint *v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = strtol (ctx->argv[at + 1], NULL, 10);
		retval = TRUE;
	}

	return retval;
}

gboolean
command_arg_string_get (command_context_t *ctx, gint at, gchar **v)
{
	gboolean retval = FALSE;

	if (at < command_arg_count (ctx)) {
		*v = ctx->argv[at + 1];
		retval = TRUE;
	}

	return retval;
}

/* Dummy callback that resets the action status as finished. */
void
cb_done (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;
	cli_infos_loop_resume (infos);
}

void
cb_tickle (xmmsc_result_t *res, void *udata)
{
	cli_infos_t *infos = (cli_infos_t *) udata;

	if (!xmmsc_result_iserror (res)) {
		xmmsc_playback_tickle (infos->conn);
	} else {
		printf ("Server error: %s\n", xmmsc_result_get_error (res));
	}

	cli_infos_loop_resume (infos);
}


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
		printf ("--tracks flag not supported yet!\n");
	}
	if (command_flag_int_get (ctx, "time", &n) && n != 0) {
		printf ("--time flag not supported yet!\n");
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
		printf ("refresh=%d\n", r);
	}

	if (command_flag_string_get (ctx, "format", &f)) {
		printf ("format='%s'\n", f);
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
help_all_commands ()
{
	gint i;

	printf ("usage: nyxmms2 COMMAND [ARGS]\n\n");
	for (i = 0; commands[i].name; ++i) {
		printf ("   %s\n", commands[i].name);
	}
	printf ("\nType 'help COMMAND' for detailed help about a command.\n");
}

void
help_command (cli_infos_t *infos, gchar *cmd)
{
	command_action_t *action;
	gint i;

	action = command_trie_find (infos->commands, cmd);
	if (action) {
		/* FIXME: show REAL name (get it somewhere), usage, description */
		printf ("%s\n\n", cmd);
		if (action->argdefs && action->argdefs[0].long_name) {
			printf ("Valid options:\n");
			for (i = 0; action->argdefs[i].long_name; ++i) {
				if (action->argdefs[i].short_name) {
					printf ("  -%c, ", action->argdefs[i].short_name);
				} else {
					printf ("      ");
				}
				printf ("--%s", action->argdefs[i].long_name);
				printf ("  %s\n", action->argdefs[i].description);
				/* FIXME: align, show arg_description, etc */
			}
		}
	} else {
		printf ("Unknown command: '%s'\n", cmd);
		printf ("Type 'help' for the list of commands.\n");
	}
}

gboolean cli_help (cli_infos_t *infos, command_context_t *ctx)
{
	gint i;

	/* No argument, display the list of commands */
	if (command_arg_count (ctx) == 0) {
		help_all_commands ();
	} else if (command_arg_count (ctx) == 1) {
		/* FIXME: find command, if found display help, else show error */
		gchar *cmd;

		if (command_arg_string_get (ctx, 0, &cmd)) {
			help_command (infos, cmd);
		} else {
			printf ("Error while parsing the argument!\n");
		}
	} else {
		printf ("To get help for a command, type 'help COMMAND'.\n");
	}

	cli_infos_loop_resume (infos);

	return TRUE;
}
