/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
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

#ifndef __STATUS_H__
#define __STATUS_H__

#include "main.h"

typedef void (*status_free_func_t) (gpointer udata);
typedef void (*status_refresh_func_t) (cli_infos_t *infos, gpointer udata,
                                       gboolean first, gboolean last);

status_entry_t *status_init (status_free_func_t free_func,
                             status_refresh_func_t refresh_func,
                             const keymap_entry_t map[],
                             gpointer udata, gint refresh);
void status_refresh (cli_infos_t *infos, status_entry_t *entry,
                     gboolean first, gboolean last);
void status_free (status_entry_t *entry);
gint status_get_refresh_interval (const status_entry_t *entry);
const keymap_entry_t *status_get_keymap (const status_entry_t *entry);

#endif /* __STATUS_H__ */
