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




#ifndef __XMMS_CORE_H__
#define __XMMS_CORE_H__

struct xmms_core_St;
typedef struct xmms_core_St xmms_core_t;

#include "xmms/playlist.h"
#include "xmms/object.h"
#include "xmms/playback.h"
#include "xmms/output.h"
#include "xmms/decoder.h"
#include "xmms/mediainfo.h"
#include "xmms/effect.h"
#include "xmms/config.h"

xmms_core_t *xmms_core_init (xmms_playlist_t *playlist);
void xmms_core_start (xmms_core_t *core);
void xmms_core_quit (xmms_core_t *core, xmms_error_t *err);

void xmms_core_set_playlist (xmms_core_t *core, xmms_playlist_t *playlist);
void xmms_core_output_set (xmms_core_t *core, xmms_output_t *output);
void xmms_core_flush_set (xmms_core_t *core, gboolean b);
void xmms_core_decoder_stop (xmms_core_t *core);
xmms_output_t *xmms_core_output_get (xmms_core_t *core);
xmms_decoder_t * xmms_core_decoder_get (xmms_core_t *core);
xmms_playlist_t * xmms_core_playlist_get (xmms_core_t *core);
xmms_playback_t * xmms_core_playback_get (xmms_core_t *core);

#endif
