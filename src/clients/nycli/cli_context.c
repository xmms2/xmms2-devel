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

#include "cli_context.h"
#include "cli_cache.h"
#include "commands.h"
#include "cmdnames.h"
#include "configuration.h"
#include "command_trie.h"
#include "alias.h"
#include "status.h"

#define MAX_CACHE_REFRESH_LOOP 200

#define COMMAND_REQ_CHECK(action, reqmask) (((reqmask) & (action)->req) == (reqmask))

struct cli_context_St {
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
cli_context_autostart (cli_context_t *ctx, gchar *path)
{
	gint ret = 0;

	/* Start the server if autostart enabled! */
	if (configuration_get_boolean (ctx->config, "SERVER_AUTOSTART")
	    && !system ("xmms2-launcher")) {
		ret = xmmsc_connect (ctx->conn, path);
	}

	return !!ret;
}

void
cli_context_status_mode (cli_context_t *ctx, status_entry_t *entry)
{
	ctx->status = CLI_ACTION_STATUS_REFRESH;
	ctx->status_entry = entry;
	readline_status_mode (ctx, status_get_keymap (entry));
	status_refresh (entry, TRUE, FALSE);
}

void
cli_context_status_mode_exit (cli_context_t *ctx)
{
	ctx->status = CLI_ACTION_STATUS_BUSY;
	ctx->status_entry = NULL;
	cli_context_loop_resume (ctx);
}

void
cli_context_alias_begin (cli_context_t *ctx)
{
	cli_context_loop_suspend (ctx);
	ctx->status = CLI_ACTION_STATUS_ALIAS;
	ctx->alias_count++;
}

void
cli_context_alias_end (cli_context_t *ctx)
{
	ctx->alias_count--;
	if (ctx->status != CLI_ACTION_STATUS_FINISH &&
	    ctx->status != CLI_ACTION_STATUS_REFRESH &&
	    ctx->alias_count == 0) {
		ctx->status = CLI_ACTION_STATUS_BUSY;
	}
	cli_context_loop_resume (ctx);
}

void
cli_context_loop_suspend (cli_context_t *ctx)
{
	if (ctx->status == CLI_ACTION_STATUS_ALIAS) {
		return;
	}
	if (ctx->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_suspend (ctx);
	}
	ctx->status = CLI_ACTION_STATUS_BUSY;
}

void
cli_context_loop_resume (cli_context_t *ctx)
{
	if (ctx->status != CLI_ACTION_STATUS_BUSY) {
		return;
	}
	if (ctx->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_resume (ctx);
	}
	ctx->status = CLI_ACTION_STATUS_READY;
}

void
cli_context_loop_stop (cli_context_t *ctx)
{
	if (ctx->mode == CLI_EXECUTION_MODE_SHELL) {
		rl_set_prompt (NULL);
	}
	ctx->status = CLI_ACTION_STATUS_FINISH;
}

/* Called on server disconnection. We can keep the loop running. */
static gint
cli_context_disconnect_callback (xmmsv_t *val, void *userdata)
{
	cli_context_t *ctx = (cli_context_t *) userdata;

	xmmsc_unref (ctx->conn);
	xmmsc_unref (ctx->sync);

	ctx->conn = NULL;
	ctx->sync = NULL;

	if (ctx->status == CLI_ACTION_STATUS_REFRESH) {
		status_refresh (ctx->status_entry, FALSE, TRUE);
		readline_status_mode_exit ();
	}
	cli_context_loop_resume (ctx);

	return TRUE;
}

/* Called when client was disconnected. xmms2d disappeared */
static void
disconnect_callback (void *userdata)
{
	cli_context_disconnect_callback (NULL, userdata);
}

gboolean
cli_context_connect (cli_context_t *ctx, gboolean autostart)
{
	gchar *path;
	xmmsc_result_t *res;

	/* Open Async connection first */
	ctx->conn = xmmsc_init (CLI_CLIENTNAME);
	if (!ctx->conn) {
		g_printf (_("Could not init connection!\n"));
		return FALSE;
	}

	path = configuration_get_string (ctx->config, "ipcpath");

	if (!xmmsc_connect (ctx->conn, path)) {
		if (!autostart) {
			/* Failed to connect, but don't autostart */
			xmmsc_unref (ctx->conn);
			ctx->conn = NULL;
			return FALSE;
		} else if (!cli_context_autostart (ctx, path)) {
			/* Autostart failed, abort now */
			if (path) {
				g_printf (_("Could not connect to server at '%s'!\n"), path);
			} else {
				g_printf (_("Could not connect to server at default path!\n"));
			}
			xmmsc_unref (ctx->conn);
			ctx->conn = NULL;
			return FALSE;
		}
	}

	/* Sync connection */
	ctx->sync = xmmsc_init (CLI_CLIENTNAME "-sync");
	if (!ctx->sync) {
		g_printf (_("Could not init sync connection!\n"));
		return FALSE;
	}

	if (!xmmsc_connect (ctx->sync, path)) {
		if (path) {
			g_printf (_("Could not connect to server at '%s'!\n"), path);
		} else {
			g_printf (_("Could not connect to server at default path!\n"));
		}

		xmmsc_unref (ctx->conn);
		xmmsc_unref (ctx->sync);

		ctx->conn = NULL;
		ctx->sync = NULL;

		return FALSE;
	}

	/* Reset the connection state on server quit */
	res = xmmsc_broadcast_quit (ctx->conn);
	xmmsc_disconnect_callback_set (ctx->conn, disconnect_callback, ctx);
	xmmsc_result_notifier_set (res, &cli_context_disconnect_callback, ctx);
	xmmsc_result_unref (res);

	cli_cache_start (ctx->cache, ctx->conn);

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

cli_context_t *
cli_context_init (void)
{
	cli_context_t *ctx;
	alias_define_t *aliaslist;
	gchar *filename;
	gint i;

	ctx = g_new0 (cli_context_t, 1);

	/* readline_init needs PROMPT */
	filename = configuration_get_filename ();
	ctx->config = configuration_init (filename);
	g_free (filename);

	readline_init (ctx);

	ctx->status = CLI_ACTION_STATUS_READY;
	ctx->commands = command_trie_alloc ();

	/* Register commands and command names */
	for (i = 0; commandlist[i]; ++i) {
		command_action_t *action = command_action_alloc ();
		commandlist[i] (action);
		if (!register_command (ctx->commands, &ctx->cmdnames, action)) {
			command_action_free (action);
		}
	}

	/* Register aliases with a default callback */
	aliaslist = alias_list (configuration_get_aliases (ctx->config));
	for (i = 0; aliaslist[i].name; ++i) {
		command_action_t *action = command_action_alloc ();
		alias_setup (action, &aliaslist[i]);
		if (!register_command (ctx->commands, &ctx->aliasnames, action)) {
			command_action_free (action);
		}
	}
	alias_list_free (aliaslist);

	ctx->alias_count = 0;
	ctx->aliasnames = cmdnames_reverse (ctx->aliasnames);
	ctx->cmdnames = cmdnames_reverse (ctx->cmdnames);
	ctx->cache = cli_cache_init ();

	return ctx;
}

void
cli_context_free (cli_context_t *ctx)
{
	if (ctx->conn) {
		xmmsc_unref (ctx->conn);
	}
	if (ctx->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_free ();
	}

	command_trie_free (ctx->commands);
	cli_cache_free (ctx->cache);
	configuration_free (ctx->config);
	cmdnames_free (ctx->cmdnames);

	g_free (ctx);
}


static void cli_context_event_loop_select (cli_context_t *ctx);
static void cli_context_command_dispatch (cli_context_t *ctx, gint in_argc, gchar **in_argv);
static void cli_context_flag_dispatch (cli_context_t *ctx, gint in_argc, gchar **in_argv);

void
cli_context_execute_command (cli_context_t *ctx, gchar *input)
{
	while (input && *input == ' ') ++input;

	if (input == NULL) {
		if (!cli_context_in_status (ctx, CLI_ACTION_STATUS_ALIAS)) {
			/* End of stream, quit */
			cli_context_loop_stop (ctx);
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
			if (!cli_context_in_status (ctx, CLI_ACTION_STATUS_ALIAS)) {
				add_history (input);
			}
			cli_context_command_dispatch (ctx, argc, argv);
			g_strfreev (argv);
			if (listop && *listop) {
				g_printf ("\n");
				cli_context_execute_command (ctx, listop);
			}
		} else {
			g_printf (_("Error: %s\n"), error->message);
			g_error_free (error);
		}
	}
}

static gboolean
cli_context_command_runnable (cli_context_t *ctx, command_action_t *action)
{
	xmmsc_connection_t *conn = cli_context_xmms_async (ctx);
	gint n = 0;

	/* Require connection, abort on failure */
	if (COMMAND_REQ_CHECK(action, COMMAND_REQ_CONNECTION) && !conn) {
		gboolean autostart;
		autostart = !COMMAND_REQ_CHECK(action, COMMAND_REQ_NO_AUTOSTART);
		if (!cli_context_connect (ctx, autostart) && autostart) {
			return FALSE;
		}
	}

	/* Get the cache ready if needed */
	if (COMMAND_REQ_CHECK(action, COMMAND_REQ_CACHE)) {
		/* If executing an alias have to refresh manually */
		if (cli_context_in_status (ctx, CLI_ACTION_STATUS_ALIAS)) {
			cli_context_cache_refresh (ctx);
		}
		while (cli_context_cache_refreshing (ctx)) {
			/* Obviously, there is a problem with updating the cache, abort */
			if (n == MAX_CACHE_REFRESH_LOOP) {
				g_printf (_("Failed to update the cache!"));
				return FALSE;
			}
			cli_context_event_loop_select (ctx);
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
cli_context_command_or_flag_dispatch (cli_context_t *ctx, gint in_argc, gchar **in_argv)
{
	/* First argument looks like a flag */
	if (in_argc > 0 && in_argv[0][0] == '-') {
		cli_context_flag_dispatch (ctx, in_argc, in_argv);
	} else {
		cli_context_command_dispatch (ctx, in_argc, in_argv);
	}
}

/* Dispatch actions according to program flags (NOT commands or
 * command options).
 */
static void
cli_context_flag_dispatch (cli_context_t *ctx, gint in_argc, gchar **in_argv)
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
			help_command (ctx, cli_context_command_names (ctx),
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
		help_command (ctx, cli_context_command_names (ctx),
		              in_argv, in_argc, CMD_TYPE_COMMAND);
	}

	command_free (cmd);
}

static void
cli_context_command_dispatch (cli_context_t *ctx, gint in_argc, gchar **in_argv)
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
	match = cli_context_find_command (ctx, &argv, &argc, &action);
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
				help_command (ctx, cli_context_command_names (ctx),
				              in_argv, in_argc, CMD_TYPE_COMMAND);
			} else if (cli_context_command_runnable (ctx, action)) {
				/* All fine, run the command */
				command_name_set (cmd, action->name);
				cli_context_loop_suspend (ctx);
				need_io = action->callback (ctx, cmd);
				if (!need_io) {
					cli_context_loop_resume (ctx);
				}
			}

			command_free (cmd);
		}
	} else {
		/* Call help to print the "no such command" error */
		help_command (ctx, cli_context_command_names (ctx),
		              in_argv, in_argc, CMD_TYPE_COMMAND);
	}
}

static void
cli_context_event_loop_select (cli_context_t *ctx)
{
	xmmsc_connection_t *conn = cli_context_xmms_async (ctx);
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
	if ((cli_context_in_mode (ctx, CLI_EXECUTION_MODE_SHELL) &&
	     cli_context_in_status (ctx, CLI_ACTION_STATUS_READY)) ||
	    cli_context_in_status (ctx, CLI_ACTION_STATUS_REFRESH)) {
		FD_SET(STDIN_FILENO, &rfds);
		if (maxfds < STDIN_FILENO) {
			maxfds = STDIN_FILENO;
		}
	}

	if (cli_context_in_status (ctx, CLI_ACTION_STATUS_REFRESH)) {
		struct timeval refresh;
		refresh.tv_sec = cli_context_refresh_interval (ctx);
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
		if ((cli_context_in_mode (ctx, CLI_EXECUTION_MODE_SHELL) ||
		     cli_context_in_status (ctx, CLI_ACTION_STATUS_REFRESH)) &&
		    FD_ISSET(STDIN_FILENO, &rfds)) {
			rl_callback_read_char ();
		}
	}

	/* Status -refresh
	   Ask theefer: use callbacks for update and -refresh only for print?
	   Nesciens: Yes, please!
	*/
	if (cli_context_in_status (ctx, CLI_ACTION_STATUS_REFRESH)) {
		cli_context_refresh_status (ctx);
	}
}

void
cli_context_loop (cli_context_t *ctx, gint argc, gchar **argv)
{
	/* Execute command, if connection status is ok */
	if (argc == 0) {
		gchar *filename = configuration_get_string (ctx->config, "HISTORY_FILE");

		ctx->mode = CLI_EXECUTION_MODE_SHELL;

		/* print welcome message before initialising readline */
		if (configuration_get_boolean (ctx->config, "SHELL_START_MESSAGE")) {
			g_printf (_("Welcome to the XMMS2 CLI shell!\n"));
			g_printf (_("Type 'help' to list the available commands "
			            "and 'exit' (or CTRL-D) to leave the shell.\n"));
		}
		readline_resume (ctx);

		read_history (filename);
		using_history ();

		while (!cli_context_in_status (ctx, CLI_ACTION_STATUS_FINISH)) {
			cli_context_event_loop_select (ctx);
		}

		write_history (filename);
	} else {
		ctx->mode = CLI_EXECUTION_MODE_INLINE;
		cli_context_command_or_flag_dispatch (ctx, argc , argv);

		while (cli_context_in_status (ctx, CLI_ACTION_STATUS_BUSY) ||
		       cli_context_in_status (ctx, CLI_ACTION_STATUS_REFRESH)) {
			cli_context_event_loop_select (ctx);
		}
	}
}

xmmsc_connection_t *
cli_context_xmms_sync (cli_context_t *ctx)
{
	return ctx->sync;
}

xmmsc_connection_t *
cli_context_xmms_async (cli_context_t *ctx)
{
	return ctx->conn;
}

configuration_t *
cli_context_config (cli_context_t *ctx)
{
	return ctx->config;
}

GList *
cli_context_command_names (cli_context_t *ctx)
{
	return ctx->cmdnames;
}

GList *
cli_context_alias_names (cli_context_t *ctx)
{
	return ctx->aliasnames;
}

gboolean
cli_context_in_mode (cli_context_t *ctx, execution_mode_t mode)
{
	return ctx->mode == mode;
}

gboolean
cli_context_in_status (cli_context_t *ctx, action_status_t status)
{
	return ctx->status == status;
}

void
cli_context_refresh_status (cli_context_t *ctx)
{
	status_refresh (ctx->status_entry, FALSE, FALSE);
}

gint
cli_context_refresh_interval (cli_context_t *ctx)
{
	return status_get_refresh_interval (ctx->status_entry);
}

command_trie_match_type_t
cli_context_find_command (cli_context_t *ctx, gchar ***argv, gint *argc,
                        command_action_t **action)
{
	return cli_context_complete_command (ctx, argv, argc, action, NULL);
}

command_trie_match_type_t
cli_context_complete_command (cli_context_t *ctx, gchar ***argv, gint *argc,
                            command_action_t **action, GList **suffixes)
{
	gboolean auto_complete = configuration_get_boolean (ctx->config, "AUTO_UNIQUE_COMPLETE");
	return command_trie_find (ctx->commands, argv, argc, auto_complete, action, suffixes);
}

void
cli_context_cache_refresh (cli_context_t *ctx)
{
	cli_cache_refresh (ctx->cache);
}

gboolean
cli_context_cache_refreshing (cli_context_t *ctx)
{
	return !cli_cache_is_fresh (ctx->cache);
}

status_entry_t *
cli_context_status_entry (cli_context_t *ctx)
{
	return ctx->status_entry;
}

gint
cli_context_current_position (cli_context_t *ctx)
{
	return ctx->cache->currpos;
}

gint
cli_context_current_id (cli_context_t *ctx)
{
	return ctx->cache->currid;
}

gint
cli_context_playback_status (cli_context_t *ctx)
{
	return ctx->cache->playback_status;
}

xmmsv_t *
cli_context_active_playlist (cli_context_t *ctx)
{
	return ctx->cache->active_playlist;
}

const gchar *
cli_context_active_playlist_name (cli_context_t *ctx)
{
	return ctx->cache->active_playlist_name;
}
