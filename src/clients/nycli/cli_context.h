/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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


#include <glib.h>
#include <xmmsclient/xmmsclient.h>

#include "command_trie.h"
#include "configuration.h"
#include "status.h"

#define CLI_CLIENTNAME "xmms2-cli"

typedef struct cli_context_St cli_context_t;

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

cli_context_t *cli_context_init (void);
gboolean cli_context_connect (cli_context_t *ctx, gboolean autostart);
void cli_context_status_mode (cli_context_t *ctx, status_entry_t *entry);
void cli_context_status_mode_exit (cli_context_t *ctx);
void cli_context_alias_begin (cli_context_t *ctx);
void cli_context_alias_end (cli_context_t *ctx);
void cli_context_loop_suspend (cli_context_t *ctx);
void cli_context_loop_resume (cli_context_t *ctx);
void cli_context_loop_stop (cli_context_t *ctx);
void cli_context_free (cli_context_t *ctx);
void cli_context_loop (cli_context_t *ctx, gint argc, gchar **argv);

void cli_context_execute_command (cli_context_t *ctx, gchar *input);

xmmsc_connection_t *cli_context_xmms_sync (cli_context_t *ctx);
xmmsc_connection_t *cli_context_xmms_async (cli_context_t *ctx);
configuration_t *cli_context_config (cli_context_t *ctx);

GList *cli_context_command_names (cli_context_t *ctx);
GList *cli_context_alias_names (cli_context_t *ctx);

gboolean cli_context_in_mode (cli_context_t *ctx, execution_mode_t mode);
gboolean cli_context_in_status (cli_context_t *ctx, action_status_t status);

void cli_context_refresh_status (cli_context_t *ctx);
gint cli_context_refresh_interval (cli_context_t *ctx);

command_trie_match_type_t cli_context_find_command (cli_context_t *ctx, gchar ***argv, gint *argc, command_action_t **action);
command_trie_match_type_t cli_context_complete_command (cli_context_t *ctx, gchar ***argv, gint *argc, command_action_t **action, GList **suffixes);

void cli_context_cache_refresh (cli_context_t *ctx);
gboolean cli_context_cache_refreshing (cli_context_t *ctx);
status_entry_t *cli_context_status_entry (cli_context_t *ctx);

gint cli_context_current_position (cli_context_t *ctx);
gint cli_context_current_id (cli_context_t *ctx);
gint cli_context_playback_status (cli_context_t *ctx);
xmmsv_t *cli_context_active_playlist (cli_context_t *ctx);
const gchar *cli_context_active_playlist_name (cli_context_t *ctx);

#endif /* __CLI_INFOS_H__ */
