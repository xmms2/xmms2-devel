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

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>

#include "main.h"
#include "cli_infos.h"
#include "cli_cache.h"
#include "status.h"
#include "commands.h"
#include "command_trie.h"
#include "command_utils.h"
#include "readline.h"
#include "configuration.h"

#include <xmms_configuration.h>

static void loop_select (cli_infos_t *infos);
static void command_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv);
static void flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv);

void
command_run (cli_infos_t *infos, gchar *input)
{
	while (input && *input == ' ') ++input;

	if (input == NULL) {
		if (!cli_infos_in_status (infos, CLI_ACTION_STATUS_ALIAS)) {
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
			if (!cli_infos_in_status (infos, CLI_ACTION_STATUS_ALIAS)) {
				add_history (input);
			}
			command_dispatch (infos, argc, argv);
			g_strfreev (argv);
			if (listop && *listop) {
				g_printf ("\n");
				command_run (infos, listop);
			}
		} else {
			g_printf (_("Error: %s\n"), error->message);
			g_error_free (error);
		}
	}
}


static gboolean
command_runnable (cli_infos_t *infos, command_action_t *action)
{
	xmmsc_connection_t *conn = cli_infos_xmms_async (infos);
	gint n = 0;

	/* Require connection, abort on failure */
	if (COMMAND_REQ_CHECK(action, COMMAND_REQ_CONNECTION) && !conn) {
		gboolean autostart;
		autostart = !COMMAND_REQ_CHECK(action, COMMAND_REQ_NO_AUTOSTART);
		if (!cli_infos_connect (infos, autostart) && autostart) {
			return FALSE;
		}
	}

	/* Get the cache ready if needed */
	if (COMMAND_REQ_CHECK(action, COMMAND_REQ_CACHE)) {
		/* If executing an alias have to refresh manually */
		if (cli_infos_in_status (infos, CLI_ACTION_STATUS_ALIAS)) {
			cli_infos_cache_refresh (infos);
		}
		while (cli_infos_cache_refreshing (infos)) {
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
static void
flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	command_context_t *ctx;
	gboolean check;

	GOptionEntry flagdefs[] = {
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
	ctx = command_context_new (flagdefs, in_argc + 1, in_argv - 1);

	if (!ctx) {
		/* An error message has already been printed, so we just return. */
		return;
	}

	if (command_flag_boolean_get (ctx, "help", &check) && check) {
		if (command_arg_count (ctx) >= 1) {
			help_command (infos, cli_infos_command_names (infos),
			              command_argv_get (ctx), command_arg_count (ctx),
			              CMD_TYPE_COMMAND);
		} else {
			/* FIXME: explain -h and -v flags here (reuse help_command code?) */
			g_printf (_("usage: xmms2 [<command> [args]]\n\n"));
			g_printf (_("XMMS2 CLI, the awesome command-line XMMS2 client from the future, "
			          "v" XMMS_VERSION ".\n\n"));
			g_printf (_("If given a command, runs it inline and exit.\n"));
			g_printf (_("If not, enters a shell-like interface to execute commands.\n\n"));
			g_printf (_("Type 'help <command>' for detailed help about a command.\n"));
		}
	} else if (command_flag_boolean_get (ctx, "version", &check) && check) {
		g_printf (_("XMMS2 CLI version " XMMS_VERSION "\n"));
		g_printf (_("Copyright (C) 2008 XMMS2 Team\n"));
		g_printf (_("This is free software; see the source for copying conditions.\n"));
		g_printf (_("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A\n"
		          "PARTICULAR PURPOSE.\n"));
		/* FIXME: compiled against? use RL_READLINE_VERSION? */
		g_printf (_(" Using readline version %s\n"), rl_library_version);
	} else {
		/* Call help to print the "no such command" error */
		/* FIXME: Could be a more helpful "invalid flag"?*/
		help_command (infos, cli_infos_command_names (infos),
		              in_argv, in_argc, CMD_TYPE_COMMAND);
	}

	command_context_free (ctx);
}

static void
command_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	command_action_t *action;
	command_trie_match_type_t match;
	gint argc;
	gchar *tmp_argv[in_argc+1];
	gchar **argv;

	/* The arguments will be updated by command_trie_find and
	 * init_context_from_args, so we make a copy. */
	memcpy (tmp_argv, in_argv, in_argc * sizeof (gchar *));
	tmp_argv[in_argc] = NULL;

	argc = in_argc;
	argv = tmp_argv;

	/* This updates argv and argc so that they start at the first non-command
	 * token. */
	match = cli_infos_find_command (infos, &argv, &argc, &action);
	if (match == COMMAND_TRIE_MATCH_ACTION) {
		gboolean help;
		gboolean need_io;
		command_context_t *ctx;

		/* Include one command token as a workaround for the bug that
		 * the option parser does not parse commands starting with a
		 * flag properly (e.g. "-p foo arg1"). Will be skipped by the
		 * command utils. */
		ctx = command_context_new (action->argdefs, argc + 1, argv - 1);

		if (ctx) {
			if (command_flag_boolean_get (ctx, "help", &help) && help) {
				/* Help flag passed, bypass action and show help */
				/* FIXME(g): select aliasnames list if it's an alias */
				help_command (infos, cli_infos_command_names (infos),
				              in_argv, in_argc, CMD_TYPE_COMMAND);
			} else if (command_runnable (infos, action)) {
				/* All fine, run the command */
				command_name_set (ctx, action->name);
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
		help_command (infos, cli_infos_command_names (infos),
		              in_argv, in_argc, CMD_TYPE_COMMAND);
	}
}

static void
loop_select (cli_infos_t *infos)
{
	xmmsc_connection_t *conn = cli_infos_xmms_async (infos);
	fd_set rfds, wfds;
	gint modfds;
	gint xmms2fd;
	gint maxfds = 0;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	/* Listen to xmms2 if connected */
	if (conn) {
		xmms2fd = xmmsc_io_fd_get (conn);
		if (xmms2fd == -1) {
			g_printf (_("Error: failed to retrieve XMMS2 file descriptor!"));
			return;
		}

		FD_SET(xmms2fd, &rfds);
		if (xmmsc_io_want_out (conn)) {
			FD_SET(xmms2fd, &wfds);
		}

		if (maxfds < xmms2fd) {
			maxfds = xmms2fd;
		}
	}

	/* Listen to readline in shell mode or status mode */
	if ((cli_infos_in_mode (infos, CLI_EXECUTION_MODE_SHELL) &&
	     cli_infos_in_status (infos, CLI_ACTION_STATUS_READY)) ||
	     cli_infos_in_status (infos, CLI_ACTION_STATUS_REFRESH)) {
		FD_SET(STDIN_FILENO, &rfds);
		if (maxfds < STDIN_FILENO) {
			maxfds = STDIN_FILENO;
		}
	}

	if (cli_infos_in_status (infos, CLI_ACTION_STATUS_REFRESH)) {
		struct timeval refresh;
		refresh.tv_sec = cli_infos_refresh_interval (infos);
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
		if (conn) {
			if (FD_ISSET(xmms2fd, &rfds) &&
			    !xmmsc_io_in_handle (conn)) {
				return;
			}

			if (FD_ISSET(xmms2fd, &wfds) &&
			    !xmmsc_io_out_handle (conn)) {
				return;
			}
		}

		/* User input found, read it */
		if ((cli_infos_in_mode (infos, CLI_EXECUTION_MODE_SHELL) ||
		     cli_infos_in_status (infos, CLI_ACTION_STATUS_REFRESH)) &&
		    FD_ISSET(STDIN_FILENO, &rfds)) {
			rl_callback_read_char ();
		}
	}

	/* Status -refresh
	   Ask theefer: use callbacks for update and -refresh only for print?
	   Nesciens: Yes, please!
	*/
	if (cli_infos_in_status (infos, CLI_ACTION_STATUS_REFRESH)) {
		cli_infos_refresh_status (infos);
	}
}

static void
loop_once (cli_infos_t *infos, gint argc, gchar **argv)
{
	command_or_flag_dispatch (infos, argc, argv);

	while (cli_infos_in_status (infos, CLI_ACTION_STATUS_BUSY) ||
	       cli_infos_in_status (infos, CLI_ACTION_STATUS_REFRESH)) {
		loop_select (infos);
	}
}

static void
loop_run (cli_infos_t *infos)
{
	while (!cli_infos_in_status (infos, CLI_ACTION_STATUS_FINISH)) {
		loop_select (infos);
	}
}



gint
main (gint argc, gchar **argv)
{
	cli_infos_t *cli_infos;

	setlocale (LC_ALL, "");

	cli_infos = cli_infos_init (argc - 1, argv + 1);

	/* Execute command, if connection status is ok */
	if (cli_infos) {
		if (cli_infos_in_mode (cli_infos, CLI_EXECUTION_MODE_INLINE)) {
			loop_once (cli_infos, argc - 1, argv + 1);
		} else {
			configuration_t *config = cli_infos_config (cli_infos);
			gchar *filename;

			filename = configuration_get_string (config, "HISTORY_FILE");

			read_history (filename);
			using_history ();
			loop_run (cli_infos);
			write_history (filename);
		}
	}

	cli_infos_free (cli_infos);

	return 0;
}
