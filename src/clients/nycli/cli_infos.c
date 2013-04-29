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

#include "readline.h"

#include "main.h"
#include "cli_infos.h"
#include "cli_cache.h"
#include "commands.h"
#include "cmdnames.h"
#include "configuration.h"
#include "command_trie.h"
#include "alias.h"
#include "status.h"

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
cli_infos_init (gint argc, gchar **argv)
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

	if (argc == 0) {
		infos->mode = CLI_EXECUTION_MODE_SHELL;
		/* print welcome message before initialising readline */
		if (configuration_get_boolean (infos->config, "SHELL_START_MESSAGE")) {
			g_printf (_("Welcome to the XMMS2 CLI shell!\n"));
			g_printf (_("Type 'help' to list the available commands "
			            "and 'exit' (or CTRL-D) to leave the shell.\n"));
		}
		readline_resume (infos);
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
	}

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
