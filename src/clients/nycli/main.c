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

#include <locale.h>
#include <stdlib.h>

#include <glib.h>

#include "cli_infos.h"

gint
main (gint argc, gchar **argv)
{
	cli_infos_t *infos;

	setlocale (LC_ALL, "");

	infos = cli_infos_init ();
	cli_infos_loop (infos, argc - 1, argv + 1);
	cli_infos_free (infos);

	return EXIT_SUCCESS;
}
