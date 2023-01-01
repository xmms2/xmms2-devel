/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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

#ifndef __XMMS_PLAYLIST_UPDATER_H__
#define __XMMS_PLAYLIST_UPDATER_H__

#include <xmmspriv/xmms_playlist.h>

typedef struct xmms_playlist_updater_St xmms_playlist_updater_t;

xmms_playlist_updater_t * xmms_playlist_updater_init (xmms_playlist_t *pls);
void xmms_playlist_updater_push (xmms_playlist_updater_t *updater, const gchar *plname);

#endif
