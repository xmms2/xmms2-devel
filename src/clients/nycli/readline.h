/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2017 XMMS2 Team
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

#include "cli_context.h"
#include "status.h"

void readline_init (cli_context_t *ctx);
void readline_suspend (cli_context_t *ctx);
void readline_resume (cli_context_t *ctx);
void readline_status_mode (cli_context_t *ctx, const keymap_entry_t map[]);
void readline_status_mode_exit (void);
void readline_free (void);
