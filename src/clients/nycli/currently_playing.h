/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#ifndef __CURRENTLY_PLAYING_H__
#define __CURRENTLY_PLAYING_H__

#include <glib.h>

#include "cli_context.h"
#include "status.h"

status_entry_t *currently_playing_init (cli_context_t *ctx, const gchar *format, gint refresh);

#endif /* __CURRENTLY_PLAYING_H__ */
