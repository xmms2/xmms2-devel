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
#include "configuration.h"

#include "utils.h"
#include "status.h"
#include "cli_infos.h"

static gchar *readline_keymap;
static cli_infos_t *readline_cli_infos;

static void
readline_callback (gchar *input)
{
	while (input && *input == ' ') ++input;

	if (input == NULL) {
		/* End of stream, quit */
		cli_infos_loop_stop (readline_cli_infos);
		g_printf ("\n");
	} else if (*input != 0) {
		gint argc;
		gchar **argv;
		GError *error = NULL;

		if (g_shell_parse_argv (input, &argc, &argv, &error)) {
			add_history (input);
			command_dispatch (readline_cli_infos, argc, argv);
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
readline_status_callback (gchar *input)
{
	Keymap active;

	active = rl_get_keymap ();

	rl_set_keymap (rl_get_keymap_by_name (readline_keymap));

	rl_discard_keymap (active);
	
/* 	rl_unbind_key (110); */
/* 	rl_bind_key (110, rl_insert); */

/* 	rl_unbind_key (112); */
/* 	rl_bind_key (112, rl_insert); */

/* 	rl_unbind_key (116); */
/* 	rl_bind_key (116, rl_insert); */

	rl_callback_handler_remove ();

	readline_cli_infos->status = CLI_ACTION_STATUS_BUSY;
	cli_infos_loop_resume (readline_cli_infos);
}

static gint
readline_status_next (gint count, gint key)
{
 	status_set_next_rel (readline_cli_infos, 1);
	return 0;
}

static gint
readline_status_prev (gint count, gint key)
{
 	status_set_next_rel (readline_cli_infos, -1);
	return 0;
}

static gint
readline_status_toggle (gint count, gint key)
{
	g_printf ("TOGGLE PLAYBACK\n");
	return 0;
}

void
readline_init (cli_infos_t *infos)
{
	readline_cli_infos = infos;
	rl_callback_handler_install (configuration_get_string (infos->config,
	                                                       "PROMPT"),
	                             &readline_callback);
}

void
readline_suspend (cli_infos_t *infos)
{
	rl_callback_handler_remove ();
}

void
readline_resume (cli_infos_t *infos)
{
	rl_callback_handler_install (configuration_get_string (infos->config,
	                                                       "PROMPT"),
	                             &readline_callback);
}

void
readline_status_mode (cli_infos_t *infos)
{
	Keymap stkmap;

	rl_callback_handler_install (NULL, &readline_status_callback);

	/* Backup current keymap-name */
	readline_keymap = g_strdup (rl_get_keymap_name (rl_get_keymap ()));

	/* New keymap for status mode */
	stkmap = rl_make_bare_keymap ();
	
	rl_bind_key_in_map ('\n', rl_newline, stkmap);
	rl_bind_key_in_map ('\r', rl_newline, stkmap);
	rl_bind_key_in_map ('n', readline_status_next, stkmap);
	rl_bind_key_in_map ('p', readline_status_prev, stkmap);
	rl_bind_key_in_map ('t', readline_status_toggle, stkmap);

	rl_set_keymap (stkmap);
}

void
readline_screen_size (gint *rows, gint *columns)
{
	rl_get_screen_size (rows, columns);
}

void
readline_free ()
{
	rl_callback_handler_remove ();
}
