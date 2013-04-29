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

#include <readline/readline.h>
#include <readline/history.h>

#include "cli_infos.h"
#include "status.h"

void readline_init (cli_infos_t *infos);
void readline_suspend (cli_infos_t *infos);
void readline_resume (cli_infos_t *infos);
void readline_status_mode (cli_infos_t *infos, const keymap_entry_t map[]);
void readline_status_mode_exit (void);
void readline_free (void);
