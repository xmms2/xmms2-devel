/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include "cli_context.h"

gint
main (gint argc, gchar **argv)
{
	cli_context_t *ctx;

	setlocale (LC_ALL, "");

	ctx = cli_context_init ();
	cli_context_loop (ctx, argc - 1, argv + 1);
	cli_context_free (ctx);

	return EXIT_SUCCESS;
}
