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

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>

#include "cli_infos.h"
#include "cli_cache.h"
#include "status.h"
#include "commands.h"
#include "command_trie.h"
#include "command_utils.h"
#include "readline.h"
#include "configuration.h"


static void loop_select (cli_infos_t *infos);


void
command_argument_free (void *x)
{
	g_free (x);
}

command_context_t *
command_context_init (gint argc, gchar **argv)
{
	command_context_t *ctx;
	ctx = g_new0 (command_context_t, 1);

	/* Register a hashtable to receive flag values and pass them on */
	ctx->argc = argc;
	ctx->argv = argv;
	ctx->flags = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                    g_free, command_argument_free);

	return ctx;
}

void
command_context_free (command_context_t *ctx)
{
	g_hash_table_destroy (ctx->flags);
	g_free (ctx);
}


gboolean
command_runnable (cli_infos_t *infos, command_action_t *action)
{
	gint n = 0;

	/* Require connection, abort on failure */
	if (COMMAND_REQ_CHECK(action, COMMAND_REQ_CONNECTION) && !infos->conn) {
		gboolean autostart;
		autostart = !COMMAND_REQ_CHECK(action, COMMAND_REQ_NO_AUTOSTART);
		if (!cli_infos_connect (infos, autostart) && autostart) {
			return FALSE;
		}
	}

	/* Get the cache ready if needed */
	if (COMMAND_REQ_CHECK(action, COMMAND_REQ_CACHE)) {
		while (!cli_cache_is_fresh (infos->cache)) {
			/* Obviously, there is a problem with updating the cache, abort */
			if (n == MAX_CACHE_REFRESH_LOOP) {
				g_printf (_("Failed to update the cache!"));
				return FALSE;
			}
			loop_select (infos);
			n++;
		}
	}

	return TRUE;
}

void
command_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	command_action_t *action;
	command_trie_match_type_t match;
	gint argc;
	gchar **argv;

	gboolean auto_complete;

	/* The arguments will be updated by command_trie_find. */
	argc = in_argc;
	argv = in_argv;
	auto_complete = configuration_get_boolean (infos->config,
	                                           "AUTO_UNIQUE_COMPLETE");
	match = command_trie_find (infos->commands, &argv, &argc,
	                            auto_complete, &action);

	if (match == COMMAND_TRIE_MATCH_ACTION) {

		/* FIXME: problem if flag is the first element!  */

		/* FIXME: look at the error! */
		GOptionContext *context;
		GError *error = NULL;
		gint i;
		gboolean help;
		gboolean need_io;
		command_context_t *ctx;

		/* Include one command token as a workaround for the bug that
		 * the option parser does not parse commands starting with a
		 * flag properly (e.g. "-p foo arg1"). Will be skipped by the
		 * command utils. */
		ctx = command_context_init (argc + 1, argv - 1);

		for (i = 0; action->argdefs && action->argdefs[i].long_name; ++i) {
			command_argument_t *arg = g_new (command_argument_t, 1);

			switch (action->argdefs[i].arg) {
			case G_OPTION_ARG_NONE:
				arg->type = COMMAND_ARGUMENT_TYPE_BOOLEAN;
				arg->value.vbool = FALSE;
				action->argdefs[i].arg_data = &arg->value.vbool;
				break;

			case G_OPTION_ARG_INT:
				arg->type = COMMAND_ARGUMENT_TYPE_INT;
				arg->value.vint = -1;
				action->argdefs[i].arg_data = &arg->value.vint;
				break;

			case G_OPTION_ARG_STRING:
				arg->type = COMMAND_ARGUMENT_TYPE_STRING;
				arg->value.vstring = NULL;
				action->argdefs[i].arg_data = &arg->value.vstring;
				break;

			default:
				g_printf (_("Trying to register a flag '%s' of invalid type!"),
				          action->argdefs[i].long_name);
				break;
			}

			/* FIXME: check for duplicates */
			g_hash_table_insert (ctx->flags,
			                     g_strdup (action->argdefs[i].long_name), arg);
		}

		context = g_option_context_new (NULL);
		g_option_context_set_help_enabled (context, FALSE);  /* runs exit(0)! */
		g_option_context_add_main_entries (context, action->argdefs, NULL);
		g_option_context_parse (context, &ctx->argc, &ctx->argv, &error);
		g_option_context_free (context);

		if (command_flag_boolean_get (ctx, "help", &help) && help) {
			/* Help flag passed, bypass action and show help */
			help_command (infos, in_argv, in_argc);
		} else if (command_runnable (infos, action)) {
			/* All fine, run the command */
			cli_infos_loop_suspend (infos);
			need_io = action->callback (infos, ctx);
			if (!need_io) {
				cli_infos_loop_resume (infos);
			}
		}

		command_context_free (ctx);
	} else {
		/* Call help to print the "no such command" error */
		help_command (infos, in_argv, in_argc);
	}
}

static void
loop_select (cli_infos_t *infos)
{
	fd_set rfds, wfds;
	gint modfds;
	gint xmms2fd;
	gint maxfds = 0;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	/* Listen to xmms2 if connected */
	if (infos->conn) {
		xmms2fd = xmmsc_io_fd_get (infos->conn);
		if (xmms2fd == -1) {
			g_printf (_("Error: failed to retrieve XMMS2 file descriptor!"));
			return;
		}

		FD_SET(xmms2fd, &rfds);
		if (xmmsc_io_want_out (infos->conn)) {
			FD_SET(xmms2fd, &wfds);
		}

		if (maxfds < xmms2fd) {
			maxfds = xmms2fd;
		}
	}

	/* Listen to readline in shell mode */
	if (infos->mode == CLI_EXECUTION_MODE_SHELL &&
	    (infos->status == CLI_ACTION_STATUS_READY ||
		 infos->status == CLI_ACTION_STATUS_REFRESH)) {
		FD_SET(STDINFD, &rfds);
		if (maxfds < STDINFD) {
			maxfds = STDINFD;
		}
	}

	if (infos->status == CLI_ACTION_STATUS_REFRESH) {
		struct timeval refresh;
		refresh.tv_sec = infos->status_entry->refresh;
		refresh.tv_usec = 0;
		modfds = select (maxfds + 1, &rfds, &wfds, NULL, &refresh);
	} else {
		modfds = select (maxfds + 1, &rfds, &wfds, NULL, NULL);
	}

	if(modfds < 0) {
		g_printf (_("Error: invalid I/O result!"));
		return;
	}
	else if(modfds != 0) {
		/* Get/send data to xmms2 */
		if (infos->conn) {
			if(FD_ISSET(xmms2fd, &rfds)) {
				xmmsc_io_in_handle (infos->conn);
			}
			if(FD_ISSET(xmms2fd, &wfds)) {
				xmmsc_io_out_handle (infos->conn);
			}
		}

		/* User input found, read it */
		if (infos->mode == CLI_EXECUTION_MODE_SHELL
		    && FD_ISSET(STDINFD, &rfds)) {
			rl_callback_read_char ();
		}

	}

	/* Status -refresh
	   Ask theefer: use callbacks for update and -refresh only for print? */
	if (infos->status == CLI_ACTION_STATUS_REFRESH) {
		status_update_all (infos, infos->status_entry);
		status_print_entry (infos->status_entry);
	}
}

void
loop_once (cli_infos_t *infos, gint argc, gchar **argv)
{
	command_dispatch (infos, argc, argv);

	while (infos->status == CLI_ACTION_STATUS_BUSY ||
	       infos->status == CLI_ACTION_STATUS_REFRESH) {
		loop_select (infos);
	}
}

void
loop_run (cli_infos_t *infos)
{
	while (infos->status != CLI_ACTION_STATUS_FINISH) {
		loop_select (infos);
	}
}



gint
main (gint argc, gchar **argv)
{
	cli_infos_t *cli_infos;

	cli_infos = cli_infos_init (argc - 1, argv + 1);

	/* Execute command, if connection status is ok */
	if (cli_infos) {
		if (cli_infos->mode == CLI_EXECUTION_MODE_INLINE) {
			loop_once (cli_infos, argc - 1, argv + 1);
		} else {
			loop_run (cli_infos);
		}
	}

	cli_infos_free (cli_infos);

	return 0;
}
