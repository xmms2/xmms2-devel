/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundstr�m, Anders Gustafsson
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

#include "xmms/object.h"
#include "xmms/output.h"
#include "xmms/decoder.h"
#include "xmms/mediainfo.h"
#include "xmms/effect.h"
#include "xmms/config.h"

typedef enum {
	XMMS_CORE_PLAYBACK_RUNNING,
	XMMS_CORE_PLAYBACK_STOPPED,
	XMMS_CORE_PLAYBACK_PAUSED
} xmms_core_playback_status_t;

struct xmms_core_St;
typedef struct xmms_core_St xmms_core_t;

void xmms_core_output_set (xmms_output_t *output);
void xmms_core_config_set (gchar *key, gchar *value);

void xmms_core_playtime_set (guint time);
void xmms_core_play_next ();
void xmms_core_play_prev ();
void xmms_core_playlist_addurl (gchar *nurl);
void xmms_core_mediainfo_add_entry (guint id);
void xmms_core_playlist_jump (guint id);
void xmms_core_playlist_remove (guint id);
void xmms_core_playlist_shuffle ();
void xmms_core_playlist_clear ();
void xmms_core_playlist_save (gchar *filename);
void xmms_core_playlist_sort (gchar *property);
void xmms_core_playlist_mode_set (xmms_playlist_mode_t mode);
xmms_playlist_mode_t xmms_core_playlist_mode_get ();
xmms_playlist_t *xmms_core_get_playlist ();
xmms_config_t *xmms_core_config_get (xmms_core_t *core);

void xmms_core_quit ();

void xmms_core_playlist_mediainfo_changed (guint id);
/*void xmms_core_set_mediainfo (xmms_playlist_entry_t *entry);*/
void xmms_core_set_playlist (xmms_playlist_t *playlist);
void xmms_core_information (gint loglevel, gchar *information);
gboolean xmms_core_get_mediainfo (xmms_playlist_entry_t *entry);
xmms_playlist_entry_t *xmms_core_playlist_entry_mediainfo (guint id);
gchar *xmms_core_get_url ();
gint xmms_core_get_id (xmms_core_t *core);
void xmms_core_vis_spectrum (gfloat *spec);


void xmms_core_playback_stop (xmms_core_t *core);
void xmms_core_playback_pause (xmms_core_t *core);
void xmms_core_playback_start (xmms_core_t *core);
void xmms_core_playback_seek_ms (xmms_core_t *core, guint32 milliseconds);
void xmms_core_playback_seek_samples (xmms_core_t *core, guint32 samples);

void xmms_core_init ();
void xmms_core_start ();

#endif
