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

#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <glib.h>

#include "cli_context.h"
#include "command.h"

typedef enum {
	CMD_TYPE_COMMAND,
	CMD_TYPE_ALIAS,
} cmd_type_t;

gboolean cli_play (cli_context_t *ctx, command_t *cmd);
gboolean cli_pause (cli_context_t *ctx, command_t *cmd);
gboolean cli_stop (cli_context_t *ctx, command_t *cmd);
gboolean cli_toggle(cli_context_t *ctx, command_t *cmd); /* <<<<<< */
gboolean cli_seek (cli_context_t *ctx, command_t *cmd);
gboolean cli_current (cli_context_t *ctx, command_t *cmd);
gboolean cli_prev (cli_context_t *ctx, command_t *cmd);
gboolean cli_next (cli_context_t *ctx, command_t *cmd);
gboolean cli_jump (cli_context_t *ctx, command_t *cmd);
gboolean cli_search (cli_context_t *ctx, command_t *cmd);
gboolean cli_info (cli_context_t *ctx, command_t *cmd);
gboolean cli_list (cli_context_t *ctx, command_t *cmd);
gboolean cli_add (cli_context_t *ctx, command_t *cmd);
gboolean cli_remove (cli_context_t *ctx, command_t *cmd);
gboolean cli_move (cli_context_t *ctx, command_t *cmd);
gboolean cli_kill (cli_context_t *ctx, command_t *cmd);
gboolean cli_exit (cli_context_t *ctx, command_t *cmd);
gboolean cli_help (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_list (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_switch (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_create (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_rename (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_remove (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_clear (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_shuffle (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_sort (cli_context_t *ctx, command_t *cmd);
gboolean cli_pl_config (cli_context_t *ctx, command_t *cmd);
gboolean cli_coll_list (cli_context_t *ctx, command_t *cmd);
gboolean cli_coll_show (cli_context_t *ctx, command_t *cmd);
gboolean cli_coll_create (cli_context_t *ctx, command_t *cmd);
gboolean cli_coll_rename (cli_context_t *ctx, command_t *cmd);
gboolean cli_coll_remove (cli_context_t *ctx, command_t *cmd);
gboolean cli_coll_config (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_browse (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_import (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_remove (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_rehash (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_config (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_property (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_plugins (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_volume (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_stats (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_sync (cli_context_t *ctx, command_t *cmd);
gboolean cli_server_shutdown (cli_context_t *ctx, command_t *cmd);

void cli_play_setup (command_action_t *action);
void cli_pause_setup (command_action_t *action);
void cli_stop_setup (command_action_t *action);
void cli_toggle_setup(command_action_t *action); /* <<<<< */
void cli_seek_setup (command_action_t *action);
void cli_current_setup (command_action_t *action);
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
void cli_server_browse_setup (command_action_t *action);
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

void help_command (cli_context_t *ctx, GList *cmdnames, gchar **cmd, gint num_args, cmd_type_t cmdtype);

static const command_setup_func commandlist[] =
{
	cli_add_setup,
	cli_current_setup,
	cli_exit_setup,
	cli_help_setup,
	cli_info_setup,
	cli_jump_setup,
	cli_list_setup,
	cli_move_setup,
	cli_next_setup,
	cli_pause_setup,
	cli_play_setup,
	cli_prev_setup,
	cli_remove_setup,
	cli_stop_setup,
	cli_search_setup,
	cli_seek_setup,
	cli_toggle_setup,
	cli_coll_config_setup,
	cli_coll_create_setup,
	cli_coll_list_setup,
	cli_coll_remove_setup,
	cli_coll_rename_setup,
	cli_coll_show_setup,
	cli_pl_clear_setup,
	cli_pl_config_setup,
	cli_pl_create_setup,
	cli_pl_list_setup,
	cli_pl_remove_setup,
	cli_pl_rename_setup,
	cli_pl_shuffle_setup,
	cli_pl_sort_setup,
	cli_pl_switch_setup,
	cli_server_browse_setup,
	cli_server_config_setup,
	cli_server_import_setup,
	cli_server_plugins_setup,
	cli_server_property_setup,
	cli_server_rehash_setup,
	cli_server_remove_setup,
	cli_server_shutdown_setup,
	cli_server_stats_setup,
	cli_server_sync_setup,
	cli_server_volume_setup,
	NULL
};

#endif /* __COMMANDS_H__ */
