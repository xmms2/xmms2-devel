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
gboolean cli_stop (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_status (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_prev (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_next (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_info (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_quit (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_help (cli_infos_t *infos, command_context_t *ctx);


/* FIXME: omgfulhack. Later on, implement commands as "plugins", cf xforms -- tru */
static argument_t flags_none[] = {
	{ NULL }
};

static argument_t flags_stop[] = {
	{ "tracks", 'n', 0, G_OPTION_ARG_INT, NULL, "Number of tracks after which to stop playback.", "num" },
	{ "time",   't', 0, G_OPTION_ARG_INT, NULL, "Duration after which to stop playback.", "time" },
	{ NULL }
};

static argument_t flags_status[] = {
	{ "refresh", 'r', 0, G_OPTION_ARG_INT, NULL, "Delay between each refresh of the status. If 0, the status is only printed once (default).", "time" },
	{ "format",  'f', 0, G_OPTION_ARG_STRING, NULL, "Format string used to display status.", "format" },
	{ NULL }
};

static command_action_t commands[] =
{
	{ "play", &cli_play,   TRUE, flags_none },
	{ "pause", &cli_pause, TRUE, flags_none },
	{ "stop", &cli_stop,   TRUE, flags_stop },
	{ "status", &cli_status, TRUE, flags_status },
	{ "prev", &cli_prev,   TRUE, flags_none },
	{ "next", &cli_next,   TRUE, flags_none },
	{ "info", &cli_info,   TRUE, flags_none },
	{ "quit", &cli_quit,   FALSE, flags_none },
	{ "exit", &cli_exit,   FALSE, flags_none },
	{ "help", &cli_help,   FALSE, flags_none },
	{ NULL }
};

#endif /* __COMMANDS_H__ */
