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
#include <stdlib.h>

#include "main.h"


typedef enum {
	CLI_EXECUTION_MODE_INLINE,
	CLI_EXECUTION_MODE_SHELL
} execution_mode_t;

struct cli_infos_St {
	xmmsc_connection_t *conn;
	execution_mode_t mode;
	gboolean running;
	command_trie_t *commands;
	GKeyFile *config;
};

cli_infos_t* cli_infos_init (gint argc, gchar **argv);
gboolean cli_infos_connect (cli_infos_t *infos);
void cli_infos_loop_stop (cli_infos_t *infos);
void cli_infos_free (cli_infos_t *infos);

#endif /* __CLI_INFOS_H__ */
