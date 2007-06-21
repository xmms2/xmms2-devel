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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>

#include "main.h"


gboolean cli_play (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pause (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_status (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_quit (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx);


static command_t commands[] =
{
	{ "play", &cli_play, TRUE, NULL },
	{ "pause", &cli_pause, TRUE, NULL },
	{ "status", &cli_status, TRUE, {
		{ "refresh", 'r', 0, G_OPTION_ARG_INT, NULL, "Delay between each refresh of the status. If 0, the status is only printed once (default).", "time" },
		{ "format",  'f', 0, G_OPTION_ARG_STRING, NULL, "Format string used to display status.", "format" },
		{ NULL }
	} },
	{ "quit", &cli_quit, FALSE, NULL },
	{ "exit", &cli_exit, FALSE, NULL },
	{ NULL }
};

#endif /* __COMMANDS_H__ */
