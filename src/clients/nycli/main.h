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

#ifndef __MAIN_H__
#define __MAIN_H__

#include <glib.h>

#include "cli_infos.h"

#define CLI_CLIENTNAME "xmms2-cli"

/* FIXME: shall be loaded from config when config exists */
#define MAX_CACHE_REFRESH_LOOP 200
#define MAX_INT_VALUE_BUFFER_SIZE 64

#define COMMAND_REQ_CHECK(action, reqmask) (((reqmask) & (action)->req) == (reqmask))

void command_run (cli_infos_t *infos, gchar *input);

#endif /* __MAIN_H__ */
