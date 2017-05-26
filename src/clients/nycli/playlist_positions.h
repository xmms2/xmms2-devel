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

#ifndef __PLAYLIST_POSITIONS_H__
#define __PLAYLIST_POSITIONS_H__

#include <glib.h>

typedef struct playlist_positions_St playlist_positions_t;
typedef void (*playlist_positions_func) (gint atom, void *userdata);

gboolean playlist_positions_parse (const gchar *expr, playlist_positions_t **p, gint currpos);
void playlist_positions_foreach (playlist_positions_t *positions, playlist_positions_func f, gboolean forward, void *userdata);
gboolean playlist_positions_get_single (playlist_positions_t *positions, gint *pos);
void playlist_positions_free (playlist_positions_t *positions);


#endif /* __PLAYLIST_POSITIONS_H__ */
