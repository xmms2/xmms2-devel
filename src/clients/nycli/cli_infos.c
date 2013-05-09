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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#include <xmmsclient/xmmsclient.h>

#include <xmms_configuration.h>

#include "readline.h"

#include "cli_infos.h"
#include "cli_cache.h"
#include "commands.h"
#include "cmdnames.h"
#include "configuration.h"
#include "command_trie.h"
#include "alias.h"
#include "status.h"

#define MAX_CACHE_REFRESH_LOOP 200

#define COMMAND_REQ_CHECK(action, reqmask) (((reqmask) & (action)->req) == (reqmask))

struct cli_infos_St {
	xmmsc_connection_t *conn;
	xmmsc_connection_t *sync;
	execution_mode_t mode;
	action_status_t status;
	command_trie_t *commands;
	GList *cmdnames;   /* List of command names, faster help. */
	GList *aliasnames;
	configuration_t *config;
	cli_cache_t *cache;
	status_entry_t *status_entry;
	gint alias_count;  /* For recursive aliases */
};

static gboolean
cli_infos_autostart (cli_infos_t *infos, gchar *path)
{
	gint ret = 0;

	/* Start the server if autostart enabled! */
	if (configuration_get_boolean (infos->config, "SERVER_AUTOSTART")
	    && !system ("xmms2-launcher")) {
		ret = xmmsc_connect (infos->conn, path);
	}

	return !!ret;
}

void
cli_infos_status_mode (cli_infos_t *infos, status_entry_t *entry)
{
	infos->status = CLI_ACTION_STATUS_REFRESH;
	infos->status_entry = entry;
	readline_status_mode (infos, status_get_keymap (entry));
	status_refresh (entry, TRUE, FALSE);
}

void
cli_infos_status_mode_exit (cli_infos_t *infos)
{
	infos->status = CLI_ACTION_STATUS_BUSY;
	infos->status_entry = NULL;
	cli_infos_loop_resume (infos);
}

void
cli_infos_alias_begin (cli_infos_t *infos)
{
	cli_infos_loop_suspend (infos);
	infos->status = CLI_ACTION_STATUS_ALIAS;
	infos->alias_count++;
}

void
cli_infos_alias_end (cli_infos_t *infos)
{
	infos->alias_count--;
	if (infos->status != CLI_ACTION_STATUS_FINISH &&
	    infos->status != CLI_ACTION_STATUS_REFRESH &&
	    infos->alias_count == 0) {
		infos->status = CLI_ACTION_STATUS_BUSY;
	}
	cli_infos_loop_resume (infos);
}

void
cli_infos_loop_suspend (cli_infos_t *infos)
{
	if (infos->status == CLI_ACTION_STATUS_ALIAS) {
		return;
	}
	if (infos->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_suspend (infos);
	}
	infos->status = CLI_ACTION_STATUS_BUSY;
}

void
cli_infos_loop_resume (cli_infos_t *infos)
{
	if (infos->status != CLI_ACTION_STATUS_BUSY) {
		return;
	}
	if (infos->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_resume (infos);
	}
	infos->status = CLI_ACTION_STATUS_READY;
}

void
cli_infos_loop_stop (cli_infos_t *infos)
{
	if (infos->mode == CLI_EXECUTION_MODE_SHELL) {
		rl_set_prompt (NULL);
	}
	infos->status = CLI_ACTION_STATUS_FINISH;
}

/* Called on server disconnection. We can keep the loop running. */
static gint
cli_infos_disconnect_callback (xmmsv_t *val, void *userdata)
{
	cli_infos_t *infos = (cli_infos_t *) userdata;

	xmmsc_unref (infos->conn);
	xmmsc_unref (infos->sync);

	infos->conn = NULL;
	infos->sync = NULL;

	if (infos->status == CLI_ACTION_STATUS_REFRESH) {
		status_refresh (infos->status_entry, FALSE, TRUE);
		readline_status_mode_exit ();
	}
	cli_infos_loop_resume (infos);

	return TRUE;
}

/* Called when client was disconnected. xmms2d disappeared */
static void
disconnect_callback (void *userdata)
{
	cli_infos_disconnect_callback (NULL, userdata);
}

gboolean
cli_infos_connect (cli_infos_t *infos, gboolean autostart)
{
	gchar *path;
	xmmsc_result_t *res;

	/* Open Async connection first */
	infos->conn = xmmsc_init (CLI_CLIENTNAME);
	if (!infos->conn) {
		g_printf (_("Could not init connection!\n"));
		return FALSE;
	}

	path = configuration_get_string (infos->config, "ipcpath");

	if (!xmmsc_connect (infos->conn, path)) {
		if (!autostart) {
			/* Failed to connect, but don't autostart */
			xmmsc_unref (infos->conn);
			infos->conn = NULL;
			return FALSE;
		} else if (!cli_infos_autostart (infos, path)) {
			/* Autostart failed, abort now */
			if (path) {
				g_printf (_("Could not connect to server at '%s'!\n"), path);
			} else {
				g_printf (_("Could not connect to server at default path!\n"));
			}
			xmmsc_unref (infos->conn);
			infos->conn = NULL;
			return FALSE;
		}
	}

	/* Sync connection */
	infos->sync = xmmsc_init (CLI_CLIENTNAME "-sync");
	if (!infos->sync) {
		g_printf (_("Could not init sync connection!\n"));
		return FALSE;
	}

	if (!xmmsc_connect (infos->sync, path)) {
		if (path) {
			g_printf (_("Could not connect to server at '%s'!\n"), path);
		} else {
			g_printf (_("Could not connect to server at default path!\n"));
		}

		xmmsc_unref (infos->conn);
		xmmsc_unref (infos->sync);

		infos->conn = NULL;
		infos->sync = NULL;

		return FALSE;
	}

	/* Reset the connection state on server quit */
	res = xmmsc_broadcast_quit (infos->conn);
	xmmsc_disconnect_callback_set (infos->conn, disconnect_callback, infos);
	xmmsc_result_notifier_set (res, &cli_infos_disconnect_callback, infos);
	xmmsc_result_unref (res);

	cli_cache_start (infos->cache, infos->conn);

	return TRUE;
}

static gboolean
register_command (command_trie_t *commands, GList **names, command_action_t *action)
{
	if (command_trie_insert (commands, action)) {
		gchar **namev;
		namev = g_strsplit (action->name, " ", 0);
		*names = cmdnames_prepend (*names, namev);
		g_strfreev (namev);
		return TRUE;
	} else {
		return FALSE;
	}
}

cli_infos_t *
cli_infos_init (void)
{
	cli_infos_t *infos;
	alias_define_t *aliaslist;
	gchar *filename;
	gint i;

	infos = g_new0 (cli_infos_t, 1);

	/* readline_init needs PROMPT */
	filename = configuration_get_filename ();
	infos->config = configuration_init (filename);
	g_free (filename);

	readline_init (infos);

	infos->status = CLI_ACTION_STATUS_READY;
	infos->commands = command_trie_alloc ();

	/* Register commands and command names */
	for (i = 0; commandlist[i]; ++i) {
		command_action_t *action = command_action_alloc ();
		commandlist[i] (action);
		if (!register_command (infos->commands, &infos->cmdnames, action)) {
			command_action_free (action);
		}
	}

	/* Register aliases with a default callback */
	aliaslist = alias_list (configuration_get_aliases (infos->config));
	for (i = 0; aliaslist[i].name; ++i) {
		command_action_t *action = command_action_alloc ();
		alias_setup (action, &aliaslist[i]);
		if (!register_command (infos->commands, &infos->aliasnames, action)) {
			command_action_free (action);
		}
	}
	alias_list_free (aliaslist);

	infos->alias_count = 0;
	infos->aliasnames = cmdnames_reverse (infos->aliasnames);
	infos->cmdnames = cmdnames_reverse (infos->cmdnames);
	infos->cache = cli_cache_init ();

	return infos;
}

void
cli_infos_free (cli_infos_t *infos)
{
	if (infos->conn) {
		xmmsc_unref (infos->conn);
	}
	if (infos->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_free ();
	}

	command_trie_free (infos->commands);
	cli_cache_free (infos->cache);
	configuration_free (infos->config);
	cmdnames_free (infos->cmdnames);

	g_free (infos);
}


static void cli_infos_event_loop_select (cli_infos_t *infos);
static void cli_infos_command_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv);
static void cli_infos_flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv);

void
cli_infos_execute_command (cli_infos_t *infos, gchar *input)
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
			cli_infos_command_dispatch (infos, argc, argv);
			g_strfreev (argv);
			if (listop && *listop) {
				g_printf ("\n");
				cli_infos_execute_command (infos, listop);
			}
		} else {
			g_printf (_("Error: %s\n"), error->message);
			g_error_free (error);
		}
	}
}

static gboolean
cli_infos_command_runnable (cli_infos_t *infos, command_action_t *action)
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
			cli_infos_event_loop_select (infos);
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
cli_infos_command_or_flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	/* First argument looks like a flag */
	if (in_argc > 0 && in_argv[0][0] == '-') {
		cli_infos_flag_dispatch (infos, in_argc, in_argv);
	} else {
		cli_infos_command_dispatch (infos, in_argc, in_argv);
	}
}

/* Dispatch actions according to program flags (NOT commands or
 * command options).
 */
static void
cli_infos_flag_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
{
	command_t *cmd;
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
	cmd = command_new (flagdefs, in_argc + 1, in_argv - 1);

	if (!cmd) {
		/* An error message has already been printed, so we just return. */
		return;
	}

	if (command_flag_boolean_get (cmd, "help", &check) && check) {
		if (command_arg_count (cmd) >= 1) {
			help_command (infos, cli_infos_command_names (infos),
			              command_argv_get (cmd), command_arg_count (cmd),
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
	} else if (command_flag_boolean_get (cmd, "version", &check) && check) {
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

	command_free (cmd);
}

static void
cli_infos_command_dispatch (cli_infos_t *infos, gint in_argc, gchar **in_argv)
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
		command_t *cmd;

		/* Include one command token as a workaround for the bug that
		 * the option parser does not parse commands starting with a
		 * flag properly (e.g. "-p foo arg1"). Will be skipped by the
		 * command utils. */
		cmd = command_new (action->argdefs, argc + 1, argv - 1);

		if (cmd) {
			if (command_flag_boolean_get (cmd, "help", &help) && help) {
				/* Help flag passed, bypass action and show help */
				/* FIXME(g): select aliasnames list if it's an alias */
				help_command (infos, cli_infos_command_names (infos),
				              in_argv, in_argc, CMD_TYPE_COMMAND);
			} else if (cli_infos_command_runnable (infos, action)) {
				/* All fine, run the command */
				command_name_set (cmd, action->name);
				cli_infos_loop_suspend (infos);
				need_io = action->callback (infos, cmd);
				if (!need_io) {
					cli_infos_loop_resume (infos);
				}
			}

			command_free (cmd);
		}
	} else {
		/* Call help to print the "no such command" error */
		help_command (infos, cli_infos_command_names (infos),
		              in_argv, in_argc, CMD_TYPE_COMMAND);
	}
}

static void
cli_infos_event_loop_select (cli_infos_t *infos)
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

void
cli_infos_loop (cli_infos_t *infos, gint argc, gchar **argv)
{
	/* Execute command, if connection status is ok */
	if (argc == 0) {
		gchar *filename = configuration_get_string (infos->config, "HISTORY_FILE");

		infos->mode = CLI_EXECUTION_MODE_SHELL;

		/* print welcome message before initialising readline */
		if (configuration_get_boolean (infos->config, "SHELL_START_MESSAGE")) {
			g_printf (_("Welcome to the XMMS2 CLI shell!\n"));
			g_printf (_("Type 'help' to list the available commands "
			            "and 'exit' (or CTRL-D) to leave the shell.\n"));
		}
		readline_resume (infos);

		read_history (filename);
		using_history ();

		while (!cli_infos_in_status (infos, CLI_ACTION_STATUS_FINISH)) {
			cli_infos_event_loop_select (infos);
		}

		write_history (filename);
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
		cli_infos_command_or_flag_dispatch (infos, argc , argv);

		while (cli_infos_in_status (infos, CLI_ACTION_STATUS_BUSY) ||
		       cli_infos_in_status (infos, CLI_ACTION_STATUS_REFRESH)) {
			cli_infos_event_loop_select (infos);
		}
	}
}

xmmsc_connection_t *
cli_infos_xmms_sync (cli_infos_t *infos)
{
	return infos->sync;
}

xmmsc_connection_t *
cli_infos_xmms_async (cli_infos_t *infos)
{
	return infos->conn;
}

configuration_t *
cli_infos_config (cli_infos_t *infos)
{
	return infos->config;
}

GList *
cli_infos_command_names (cli_infos_t *infos)
{
	return infos->cmdnames;
}

GList *
cli_infos_alias_names (cli_infos_t *infos)
{
	return infos->aliasnames;
}

gboolean
cli_infos_in_mode (cli_infos_t *infos, execution_mode_t mode)
{
	return infos->mode == mode;
}

gboolean
cli_infos_in_status (cli_infos_t *infos, action_status_t status)
{
	return infos->status == status;
}

void
cli_infos_refresh_status (cli_infos_t *infos)
{
	status_refresh (infos->status_entry, FALSE, FALSE);
}

gint
cli_infos_refresh_interval (cli_infos_t *infos)
{
	return status_get_refresh_interval (infos->status_entry);
}

command_trie_match_type_t
cli_infos_find_command (cli_infos_t *infos, gchar ***argv, gint *argc,
                        command_action_t **action)
{
	return cli_infos_complete_command (infos, argv, argc, action, NULL);
}

command_trie_match_type_t
cli_infos_complete_command (cli_infos_t *infos, gchar ***argv, gint *argc,
                            command_action_t **action, GList **suffixes)
{
	gboolean auto_complete = configuration_get_boolean (infos->config, "AUTO_UNIQUE_COMPLETE");
	return command_trie_find (infos->commands, argv, argc, auto_complete, action, suffixes);
}

void
cli_infos_cache_refresh (cli_infos_t *infos)
{
	cli_cache_refresh (infos->cache);
}

gboolean
cli_infos_cache_refreshing (cli_infos_t *infos)
{
	return !cli_cache_is_fresh (infos->cache);
}

status_entry_t *
cli_infos_status_entry (cli_infos_t *infos)
{
	return infos->status_entry;
}

gint
cli_infos_current_position (cli_infos_t *infos)
{
	return infos->cache->currpos;
}

gint
cli_infos_current_id (cli_infos_t *infos)
{
	return infos->cache->currid;
}

gint
cli_infos_playback_status (cli_infos_t *infos)
{
	return infos->cache->playback_status;
}

xmmsv_t *
cli_infos_active_playlist (cli_infos_t *infos)
{
	return infos->cache->active_playlist;
}

const gchar *
cli_infos_active_playlist_name (cli_infos_t *infos)
{
	return infos->cache->active_playlist_name;
}
