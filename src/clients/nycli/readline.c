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
		GError *error;

		if (g_shell_parse_argv (input, &argc, &argv, &error)) {
			add_history (input);
			command_dispatch (readline_cli_infos, argc, argv);
		} else {
			/* FIXME: Handle errors */
		}
	}

}

void
readline_init (cli_infos_t *infos)
{
	readline_cli_infos = infos;
	rl_callback_handler_install (PROMPT, &readline_callback);
}

void
readline_suspend (cli_infos_t *infos)
{
	rl_callback_handler_remove ();
}

void
readline_resume (cli_infos_t *infos)
{
	rl_callback_handler_install (PROMPT, &readline_callback);
}

void
readline_free ()
{
	rl_callback_handler_remove ();
}
