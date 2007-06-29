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

gboolean cli_play_setup (command_trie_t *infos);
gboolean cli_pause_setup (command_trie_t *infos);
gboolean cli_stop_setup (command_trie_t *infos);
gboolean cli_status_setup (command_trie_t *infos);
gboolean cli_prev_setup (command_trie_t *infos);
gboolean cli_next_setup (command_trie_t *infos);
gboolean cli_info_setup (command_trie_t *infos);
gboolean cli_quit_setup (command_trie_t *infos);
gboolean cli_exit_setup (command_trie_t *infos);
gboolean cli_help_setup (command_trie_t *infos);

static command_setup_func commandlist[] =
{
	cli_play_setup,
	cli_pause_setup,
	cli_stop_setup,
	cli_status_setup,
	cli_prev_setup,
	cli_next_setup,
	cli_info_setup,
	cli_quit_setup,
	cli_exit_setup,
	cli_help_setup,
	NULL
};

#endif /* __COMMANDS_H__ */
