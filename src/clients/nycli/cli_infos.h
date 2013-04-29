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

#ifndef __CLI_INFOS_H__
#define __CLI_INFOS_H__

typedef struct cli_infos_St cli_infos_t;

#include <glib.h>
#include <xmmsclient/xmmsclient.h>

#include "status.h"
#include "cli_cache.h"
#include "command_trie.h"
#include "configuration.h"

typedef enum {
	CLI_EXECUTION_MODE_INLINE,
	CLI_EXECUTION_MODE_SHELL
} execution_mode_t;

typedef enum {
	CLI_ACTION_STATUS_READY,
	CLI_ACTION_STATUS_BUSY,
	CLI_ACTION_STATUS_FINISH,
	CLI_ACTION_STATUS_REFRESH,
	CLI_ACTION_STATUS_ALIAS
} action_status_t;

cli_infos_t* cli_infos_init (gint argc, gchar **argv);
gboolean cli_infos_connect (cli_infos_t *infos, gboolean autostart);
void cli_infos_status_mode (cli_infos_t *infos, status_entry_t *entry);
void cli_infos_status_mode_exit (cli_infos_t *infos);
void cli_infos_alias_begin (cli_infos_t *infos);
void cli_infos_alias_end (cli_infos_t *infos);
void cli_infos_loop_suspend (cli_infos_t *infos);
void cli_infos_loop_resume (cli_infos_t *infos);
void cli_infos_loop_stop (cli_infos_t *infos);
void cli_infos_free (cli_infos_t *infos);

xmmsc_connection_t *cli_infos_xmms_sync (cli_infos_t *infos);
xmmsc_connection_t *cli_infos_xmms_async (cli_infos_t *infos);
configuration_t *cli_infos_config (cli_infos_t *infos);

GList *cli_infos_command_names (cli_infos_t *infos);
GList *cli_infos_alias_names (cli_infos_t *infos);

gboolean cli_infos_in_mode (cli_infos_t *infos, execution_mode_t mode);
gboolean cli_infos_in_status (cli_infos_t *infos, action_status_t status);

void cli_infos_refresh_status (cli_infos_t *infos);
gint cli_infos_refresh_interval (cli_infos_t *infos);

command_trie_match_type_t cli_infos_find_command (cli_infos_t *infos, gchar ***argv, gint *argc, command_action_t **action);
command_trie_match_type_t cli_infos_complete_command (cli_infos_t *infos, gchar ***argv, gint *argc, command_action_t **action, GList **suffixes);

void cli_infos_cache_refresh (cli_infos_t *infos);
gboolean cli_infos_cache_refreshing (cli_infos_t *infos);
status_entry_t *cli_infos_status_entry (cli_infos_t *infos);

gint cli_infos_current_position (cli_infos_t *infos);
gint cli_infos_current_id (cli_infos_t *infos);
gint cli_infos_playback_status (cli_infos_t *infos);
xmmsv_t *cli_infos_active_playlist (cli_infos_t *infos);
const gchar *cli_infos_active_playlist_name (cli_infos_t *infos);

#endif /* __CLI_INFOS_H__ */
