/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
command_run (cli_infos_t *infos, gchar *input)
{
	while (input && *input == ' ') ++input;

	if (input == NULL) {
		if (infos->status != CLI_ACTION_STATUS_ALIAS) {
			/* End of stream, quit */
			cli_infos_loop_stop (infos);
			g_printf ("\n");
		}
	} else if (*input != 0) {
		gint argc;
		gchar **argv, *listop;
		GError *error = NULL;

		if ((listop = strchr (input, ';'))) {
			*listop++ = '\0';
		}

		if (g_shell_parse_argv (input, &argc, &argv, &error)) {
			if (infos->status != CLI_ACTION_STATUS_ALIAS) {
				add_history (input);
			}
			command_dispatch (infos, argc, argv);
			g_strfreev (argv);
			if (listop && *listop) {
				g_printf ("\n");
				command_run (infos, listop);
			}
		} else {
			if (g_error_matches (error, G_SHELL_ERROR,
			                     G_SHELL_ERROR_BAD_QUOTING)) {
				g_printf (_("Error: Mismatched quote\n"));
			} else if (g_error_matches (error, G_SHELL_ERROR,
			                            G_SHELL_ERROR_FAILED)) {
				g_printf (_("Error: Invalid input\n"));
			}
			g_error_free (error);
			/* FIXME: Handle errors */
		}
	}
}

static void
command_argument_free (void *x)
{
	command_argument_t *arg = (command_argument_t *)x;

	if (arg->type == COMMAND_ARGUMENT_TYPE_STRING && arg->value.vstring) {
		g_free (arg->value.vstring);
	}
	g_free (arg);
}

static command_context_t *
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

static void
command_context_free (command_context_t *ctx)
{
	g_hash_table_destroy (ctx->flags);
	g_free (ctx->name);
	g_free (ctx);
}


static gboolean
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
		/* If executing an alias have to refresh manually */
		if (infos->status == CLI_ACTION_STATUS_ALIAS) {
			cli_cache_refresh (infos);
		}
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

/* Parse the argv array with GOptionContext, using the given argument
 * definitions, and return a command context structure containing
 * argument values.
 * Note: The lib doesn't like argv starting with a flag, so keep a
 * token before that to avoid problems.
 */
static command_context_t *
init_context_from_args (argument_t *argdefs, gint argc, gchar **argv)
{
	command_context_t *ctx;
	GOptionContext *context;
	GError *error = NULL;
	gint i;

	ctx = command_context_init (argc, argv);

	for (i = 0; argdefs && argdefs[i].long_name; ++i) {
		command_argument_t *arg = g_new (command_argument_t, 1);

		switch (argdefs[i].arg) {
		case G_OPTION_ARG_NONE:
			arg->type = COMMAND_ARGUMENT_TYPE_BOOLEAN;
			arg->value.vbool = FALSE;
			argdefs[i].arg_data = &arg->value.vbool;
			break;

		case G_OPTION_ARG_INT:
			arg->type = COMMAND_ARGUMENT_TYPE_INT;
			arg->value.vint = -1;
			argdefs[i].arg_data = &arg->value.vint;
			break;

		case G_OPTION_ARG_STRING:
			arg->type = COMMAND_ARGUMENT_TYPE_STRING;
			arg->value.vstring = NULL;
			argdefs[i].arg_data = &arg->value.vstring;
			break;

		default:
			g_printf (_("Trying to register a flag '%s' of invalid type!"),
			          argdefs[i].long_name);
			break;
		}

		g_hash_table_insert (ctx->flags,
		                     g_strdup (argdefs[i].long_name), arg);
	}

	context = g_option_context_new (NULL);
	g_option_context_set_help_enabled (context, FALSE);  /* runs exit(0)! */
	g_option_context_set_ignore_unknown_options (context, TRUE);
	g_option_context_add_main_entries (context, argdefs, NULL);
	g_option_context_parse (context, &ctx->argc, &ctx->argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printf (_("Error: %s\n"), error->message);
		g_error_free (error);
		command_context_free (ctx);
		return NULL;
	}

	/* strip --, check for unknown options before it */
	/* FIXME: We do not parse options elsewhere, do we? */
	for (i = 0; i < ctx->argc; i++) {
		if (strcmp (ctx->argv[i], "--") == 0) {
			break;
		}
		if (ctx->argv[i][0] == '-' && ctx->argv[i][1] != '\0' &&
		    !(ctx->argv[i][1] >= '0' && ctx->argv[i][1] <= '9')) {

			g_printf (_("Error: Unknown option '%s'\n"), ctx->argv[i]);
			command_context_free (ctx);
			return NULL;
		}
	}
	if (i != ctx->argc) {
		for (i++; i < ctx->argc; i++) {
			argv[i-1] = argv[i];
		}
		ctx->argc--;
	}

	return ctx;
}

/* Switch function which should only be called with 'raw' (i.e. inline
 * mode, not shell mode) argv/argc.  If appropriate, it enters
 * flag_dispatch to parse program flags.
 */
static void
command_or_flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	/* First argument looks like a flag */
	if (in_argc > 0 && in_argv[0][0] == '-') {
		flag_dispatch (infos, in_argc, in_argv);
	} else {
		command_dispatch (infos, in_argc, in_argv);
	}
}

/* Dispatch actions according to program flags (NOT commands or
 * command options).
 */
void
flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	command_context_t *ctx;
	gboolean check;

	argument_t flagdefs[] = {
		{ "help",    'h', 0, G_OPTION_ARG_NONE, NULL,
		             _("Display this help and exit."), NULL },
		{ "version", 'v', 0, G_OPTION_ARG_NONE, NULL,
		             _("Output version information and exit."), NULL },
		{ NULL }
	};

	/* Include one command token as a workaround for the bug that
	 * the option parser does not parse commands starting with a
	 * flag properly (e.g. "-p foo arg1"). Will be skipped by the
	 * command utils. */
	ctx = init_context_from_args (flagdefs, in_argc + 1, in_argv - 1);

	if (!ctx) {
		/* An error message has already been printed, so we just return. */
		return;
	}

	if (command_flag_boolean_get (ctx, "help", &check) && check) {
		if (ctx->argc > 1) {
			help_command (infos, infos->cmdnames, ctx->argv + 1, ctx->argc - 1,
			              CMD_TYPE_COMMAND);
		} else {
			/* FIXME: explain -h and -v flags here (reuse help_command code?) */
			g_printf (_("usage: nyxmms2 [<command> [args]]\n\n"));
			g_printf (_("NyCLI, the awesome command-line XMMS2 client from the future, "
			          "v" XMMS2_CLI_VERSION ".\n\n"));
			g_printf (_("If given a command, runs it inline and exit.\n"));
			g_printf (_("If not, enters a shell-like interface to execute commands.\n\n"));
			g_printf (_("Type 'help <command>' for detailed help about a command.\n"));
		}
	} else if (command_flag_boolean_get (ctx, "version", &check) && check) {
		g_printf (_("NyCLI version " XMMS2_CLI_VERSION "\n"));
		g_printf (_("Copyright (C) 2008 XMMS2 Team\n"));
		g_printf (_("This is free software; see the source for copying conditions.\n"));
		g_printf (_("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A\n"
		          "PARTICULAR PURPOSE.\n"));
		/* FIXME: compiled against? use RL_READLINE_VERSION? */
		g_printf (_(" Using readline version %s\n"), rl_library_version);
	} else {
		/* Call help to print the "no such command" error */
		/* FIXME: Could be a more helpful "invalid flag"?*/
		help_command (infos, infos->cmdnames, in_argv, in_argc, CMD_TYPE_COMMAND);
	}

	command_context_free (ctx);
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
	in_argv = g_memdup (in_argv, sizeof (gchar *) * in_argc);
	argc = in_argc;
	argv = in_argv;

	auto_complete = configuration_get_boolean (infos->config,
	                                           "AUTO_UNIQUE_COMPLETE");
	match = command_trie_find (infos->commands, &argv, &argc,
	                           auto_complete, &action, NULL);

	if (match == COMMAND_TRIE_MATCH_ACTION) {

		gboolean help;
		gboolean need_io;
		command_context_t *ctx;

		/* Include one command token as a workaround for the bug that
		 * the option parser does not parse commands starting with a
		 * flag properly (e.g. "-p foo arg1"). Will be skipped by the
		 * command utils. */
		ctx = init_context_from_args (action->argdefs, argc + 1, argv - 1);

		if (ctx) {
			if (command_flag_boolean_get (ctx, "help", &help) && help) {
				/* Help flag passed, bypass action and show help */
				/* FIXME(g): select aliasnames list if it's an alias */
				help_command (infos, infos->cmdnames, in_argv, in_argc, CMD_TYPE_COMMAND);
			} else if (command_runnable (infos, action)) {
				/* All fine, run the command */
				ctx->name = g_strdup (action->name);
				cli_infos_loop_suspend (infos);
				need_io = action->callback (infos, ctx);
				if (!need_io) {
					cli_infos_loop_resume (infos);
				}
			}

			command_context_free (ctx);
		}
	} else {
		/* Call help to print the "no such command" error */
		help_command (infos, infos->cmdnames, in_argv, in_argc, CMD_TYPE_COMMAND);
	}

	g_free (in_argv);
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

	/* Listen to readline in shell mode or status mode */
	if ((infos->mode == CLI_EXECUTION_MODE_SHELL &&
	     infos->status == CLI_ACTION_STATUS_READY) ||
	     infos->status == CLI_ACTION_STATUS_REFRESH) {
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

	if (modfds < 0 && errno != EINTR) {
		g_printf (_("Error: invalid I/O result!"));
		return;
	} else if (modfds != 0) {
		/* Get/send data to xmms2 */
		if (infos->conn) {
			if (FD_ISSET(xmms2fd, &rfds) &&
			    !xmmsc_io_in_handle (infos->conn)) {
				return;
			}

			if (FD_ISSET(xmms2fd, &wfds) &&
			    !xmmsc_io_out_handle (infos->conn)) {
				return;
			}
		}

		/* User input found, read it */
		if ((infos->mode == CLI_EXECUTION_MODE_SHELL ||
		     infos->status == CLI_ACTION_STATUS_REFRESH) &&
		    FD_ISSET(STDINFD, &rfds)) {
			rl_callback_read_char ();
		}
	}

	/* Status -refresh
	   Ask theefer: use callbacks for update and -refresh only for print? */
	if (infos->status == CLI_ACTION_STATUS_REFRESH) {
		status_update_all (infos, infos->status_entry);
		g_printf ("\r");
		status_print_entry (infos->status_entry);
	}
}

static void
loop_once (cli_infos_t *infos, gint argc, gchar **argv)
{
	command_or_flag_dispatch (infos, argc, argv);

	while (infos->status == CLI_ACTION_STATUS_BUSY ||
	       infos->status == CLI_ACTION_STATUS_REFRESH) {
		loop_select (infos);
	}
}

static void
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
			gchar *filename;

			filename = configuration_get_string (cli_infos->config,
			                                     "HISTORY_FILE");

			read_history (filename);
			loop_run (cli_infos);
			write_history (filename);
		}
	}

	cli_infos_free (cli_infos);

	return 0;
}
