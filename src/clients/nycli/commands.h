/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <xmmsclient/xmmsclient.h>

#include <unistd.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "main.h"

gboolean cli_play (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pause (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_stop (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_toggle(cli_infos_t *infos, command_context_t *ctx); /* <<<<<< */
gboolean cli_seek (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_status (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_prev (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_next (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_jump (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_search (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_info (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_list (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_add (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_remove (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_move (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_kill (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_exit (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_help (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_list (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_switch (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_create (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_rename (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_remove (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_clear (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_shuffle (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_sort (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_pl_config (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_coll_list (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_coll_show (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_coll_create (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_coll_rename (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_coll_remove (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_coll_config (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_import (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_remove (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_rehash (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_config (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_property (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_plugins (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_volume (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_stats (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_sync (cli_infos_t *infos, command_context_t *ctx);
gboolean cli_server_shutdown (cli_infos_t *infos, command_context_t *ctx);

void cli_play_setup (command_action_t *action);
void cli_pause_setup (command_action_t *action);
void cli_stop_setup (command_action_t *action);
void cli_toggle_setup(command_action_t *action); /* <<<<< */
void cli_seek_setup (command_action_t *action);
void cli_status_setup (command_action_t *action);
void cli_prev_setup (command_action_t *action);
void cli_next_setup (command_action_t *action);
void cli_jump_setup (command_action_t *action);
void cli_search_setup (command_action_t *action);
void cli_info_setup (command_action_t *action);
void cli_list_setup (command_action_t *action);
void cli_add_setup (command_action_t *action);
void cli_remove_setup (command_action_t *action);
void cli_exit_setup (command_action_t *action);
void cli_help_setup (command_action_t *action);
void cli_pl_list_setup (command_action_t *action);
void cli_pl_switch_setup (command_action_t *action);
void cli_pl_create_setup (command_action_t *action);
void cli_pl_rename_setup (command_action_t *action);
void cli_pl_remove_setup (command_action_t *action);
void cli_move_setup (command_action_t *action);
void cli_pl_clear_setup (command_action_t *action);
void cli_pl_shuffle_setup (command_action_t *action);
void cli_pl_sort_setup (command_action_t *action);
void cli_pl_config_setup (command_action_t *action);
void cli_coll_list_setup (command_action_t *action);
void cli_coll_show_setup (command_action_t *action);
void cli_coll_create_setup (command_action_t *action);
void cli_coll_rename_setup (command_action_t *action);
void cli_coll_remove_setup (command_action_t *action);
void cli_coll_config_setup (command_action_t *action);
void cli_server_import_setup (command_action_t *action);
void cli_server_remove_setup (command_action_t *action);
void cli_server_rehash_setup (command_action_t *action);
void cli_server_config_setup (command_action_t *action);
void cli_server_property_setup (command_action_t *action);
void cli_server_plugins_setup (command_action_t *action);
void cli_server_volume_setup (command_action_t *action);
void cli_server_stats_setup (command_action_t *action);
void cli_server_sync_setup (command_action_t *action);
void cli_server_shutdown_setup (command_action_t *action);

void help_command (cli_infos_t *infos, GList *cmdnames, gchar **cmd, gint num_args, cmd_type_t cmdtype);

static const command_setup_func commandlist[] =
{
	cli_play_setup,
	cli_pause_setup,
	cli_stop_setup,
	cli_toggle_setup,
	cli_seek_setup,
	cli_status_setup,
	cli_prev_setup,
	cli_next_setup,
	cli_jump_setup,
	cli_info_setup,
	cli_search_setup,
	cli_list_setup,
	cli_add_setup,
	cli_remove_setup,
	cli_move_setup,
	cli_exit_setup,
	cli_help_setup,
	cli_pl_list_setup,
	cli_pl_switch_setup,
	cli_pl_create_setup,
	cli_pl_rename_setup,
	cli_pl_remove_setup,
	cli_pl_clear_setup,
	cli_pl_shuffle_setup,
	cli_pl_sort_setup,
	cli_pl_config_setup,
    cli_coll_list_setup,
	cli_coll_show_setup,
	cli_coll_create_setup,
	cli_coll_rename_setup,
	cli_coll_remove_setup,
	cli_coll_config_setup,
	cli_server_import_setup,
	cli_server_remove_setup,
	cli_server_rehash_setup,
	cli_server_config_setup,
	cli_server_property_setup,
	cli_server_plugins_setup,
	cli_server_volume_setup,
	cli_server_stats_setup,
	cli_server_sync_setup,
	cli_server_shutdown_setup,
	NULL
};

#endif /* __COMMANDS_H__ */
