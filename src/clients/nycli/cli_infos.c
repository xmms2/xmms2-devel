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

#include "readline.h"

#include "cli_infos.h"
#include "cli_cache.h"
#include "commands.h"
#include "command_trie.h"


gboolean
cli_infos_autostart (cli_infos_t *infos, gchar *path)
{
	gint ret = 0;

	/* Start the server if autostart enabled! */
	if (DEBUG_AUTOSTART && !system ("xmms2-launcher")) {
		ret = xmmsc_connect (infos->conn, path);
	}

	return !!ret;
}

void cli_infos_loop_suspend (cli_infos_t *infos)
{
	if (infos->mode == CLI_EXECUTION_MODE_SHELL) {
		readline_suspend (infos);
	}
	infos->status = CLI_ACTION_STATUS_BUSY;
}

void
cli_infos_loop_resume (cli_infos_t *infos)
{
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
void
cli_infos_disconnect_callback (xmmsc_result_t *res, void *userdata)
{
	cli_infos_t *infos = (cli_infos_t *) userdata;

	xmmsc_unref(infos->conn);
	xmmsc_unref(infos->sync);

	infos->conn = NULL;
	infos->sync = NULL;

	xmmsc_result_unref (res);
	cli_infos_loop_resume (infos);
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

	path = getenv ("XMMS_PATH");
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
	xmmsc_result_notifier_set (res, &cli_infos_disconnect_callback, infos);
	xmmsc_result_unref (res);

	cli_cache_start (infos);

	return TRUE;
}

cli_infos_t *
cli_infos_init (gint argc, gchar **argv)
{
	cli_infos_t *infos;
	gint i;

	infos = g_new0 (cli_infos_t, 1);

	if (argc == 0) {
		infos->mode = CLI_EXECUTION_MODE_SHELL;
		readline_init (infos);
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
	}

	infos->status = CLI_ACTION_STATUS_READY;
	infos->commands = command_trie_alloc ();

	/* Register commands and command names */
	for (i = 0; commandlist[i]; ++i) {
		command_action_t *action = command_action_alloc ();
		commandlist[i] (action);
		if (command_trie_insert (infos->commands, action)) {
			infos->cmdnames = g_list_prepend (infos->cmdnames, action->name);
		} else {
			command_action_free (action);
		}
	}
	infos->cmdnames = g_list_reverse (infos->cmdnames);

	infos->config = g_key_file_new ();
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
	g_key_file_free (infos->config);
	cli_cache_free (infos->cache);

	g_free (infos);
}
