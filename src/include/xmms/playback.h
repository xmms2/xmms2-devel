/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundström, Anders Gustafsson
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

#ifndef __XMMS_PLAYBACK_H__
#define __XMMS_PLAYBACK_H__

struct xmms_playback_St;
typedef struct xmms_playback_St xmms_playback_t;

typedef enum {
	XMMS_PLAYBACK_PLAY,
	XMMS_PLAYBACK_STOP,
	XMMS_PLAYBACK_PAUSE,
} xmms_playback_status_t;


#include "xmms/core.h"
#include "xmms/playlist.h"

xmms_playback_t *xmms_playback_init (xmms_core_t *core, xmms_playlist_t *playlist);
xmms_playlist_entry_t *xmms_playback_entry (xmms_playback_t *playback);
xmms_playlist_t * xmms_playback_playlist_get (xmms_playback_t *playback);
void xmms_playback_playtime_set (xmms_playback_t *playback, guint time);
void xmms_playback_mediainfo_add_entry (xmms_playback_t *playback, xmms_playlist_entry_t *entry);
guint xmms_playback_currentid (xmms_playback_t *playback, xmms_error_t *err);
void xmms_playback_vis_spectrum (xmms_core_t *playback, gfloat *spec);
guint xmms_playback_current_playtime (xmms_playback_t *playback, xmms_error_t *err);

#endif /* __XMMS_PLAYBACK_H__ */

