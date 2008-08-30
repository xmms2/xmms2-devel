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

#ifndef __CLI_INFOS_H__
#define __CLI_INFOS_H__

#include <xmmsclient/xmmsclient.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>

#include "main.h"

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

struct cli_infos_St {
	xmmsc_connection_t *conn;
	xmmsc_connection_t *sync;
	execution_mode_t mode;
	action_status_t status;
	command_trie_t *commands;
	GList *cmdnames;   /* List of command names, faster help. */
	GList *aliasnames;
	configuration_t *config;
	cli_cache_t *cache;
	status_entry_t *status_entry;
};

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

#endif /* __CLI_INFOS_H__ */
