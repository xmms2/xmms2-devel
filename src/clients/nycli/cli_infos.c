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
#include "commands.h"
#include "command_trie.h"


static gboolean cli_infos_autostart (cli_infos_t *infos, gchar *path);
static void cli_infos_disconnect_callback (int flag, void *userdata);


gboolean
cli_infos_autostart (cli_infos_t *infos, gchar *path)
{
	gint ret = 0;

	/* FIXME: Should we wait or something? seems like the conn isn't good right after that */
	if (DEBUG_AUTOSTART && !system ("xmms2-launcher")) {
		ret = xmmsc_connect (infos->conn, path);
	}

	return !!ret;
}

void
cli_infos_loop_stop (cli_infos_t *infos)
{
	rl_set_prompt (NULL);
	infos->running = FALSE;
}

/* Called on server disconnection. We can keep the loop running. */
void
cli_infos_disconnect_callback (int flag, void *userdata)
{
	cli_infos_t *infos = (cli_infos_t *) userdata;
	printf ("Server disconnected!\n");
	infos->conn = NULL;
	/* FIXME: issueing "quit" seems to start endless crap instead. */
}

gboolean
cli_infos_connect (cli_infos_t *infos)
{
	gchar *path;
	gint ret;

	infos->conn = xmmsc_init (CLI_CLIENTNAME);
	if (!infos->conn) {
		printf ("Could not init connection!\n");
		return FALSE;
	}

	path = getenv ("XMMS_PATH");
	ret = xmmsc_connect (infos->conn, path);
	if (!ret && !cli_infos_autostart (infos, path)) {
		if (path) {
			printf ("Could not connect to server at '%s'!\n", path);
		} else {
			printf ("Could not connect to server at default path!\n");
		}
		return FALSE;
	}

	xmmsc_ipc_disconnect_set (infos->conn, &cli_infos_disconnect_callback, infos);

	return TRUE;
}

cli_infos_t*
cli_infos_init (gint argc, gchar **argv)
{
	cli_infos_t *infos;

	infos = g_new0 (cli_infos_t, 1);

	if (argc == 0) {
		infos->mode = CLI_EXECUTION_MODE_SHELL;
	} else {
		infos->mode = CLI_EXECUTION_MODE_INLINE;
	}

	infos->running = FALSE;
	infos->commands = command_trie_alloc ();
	command_trie_fill (infos->commands, commands);

	infos->config = g_key_file_new ();

	return infos;
}

void
cli_infos_free (cli_infos_t *infos)
{
	if (infos->conn) {
		xmmsc_unref (infos->conn);
	}
	command_trie_free (infos->commands);
	g_key_file_free (infos->config);
}
