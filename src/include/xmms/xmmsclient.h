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




#ifndef __XMMS_CLIENT_H__
#define __XMMS_CLIENT_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_playlist_entry_St { /* no use to share them between client and core */
	gchar *url;
	guint id;
	GHashTable *properties;
} xmmsc_playlist_entry_t;

typedef struct xmmsc_file_St {
	gchar *path;
	gboolean file; /**< set to true if entry is a regular file */
} xmmsc_file_t;

xmmsc_connection_t *xmmsc_init();
gboolean xmmsc_connect (xmmsc_connection_t *, const char *);
void xmmsc_deinit(xmmsc_connection_t *);

gchar *xmmsc_get_last_error (xmmsc_connection_t *c);
gchar *xmmsc_encode_path (gchar *path);
gchar *xmmsc_decode_path (const gchar *path);
int xmmsc_entry_format (char *target, int len, const char *fmt, GHashTable *table);

void xmmsc_quit(xmmsc_connection_t *);
void xmmsc_play_next(xmmsc_connection_t *);
void xmmsc_play_prev(xmmsc_connection_t *);
void xmmsc_playlist_shuffle(xmmsc_connection_t *);
void xmmsc_playlist_jump (xmmsc_connection_t *, guint);
void xmmsc_playlist_add (xmmsc_connection_t *, char *);
void xmmsc_playlist_remove (xmmsc_connection_t *, guint);
void xmmsc_playlist_clear (xmmsc_connection_t *c);
void xmmsc_playlist_save (xmmsc_connection_t *c, gchar *filename);
void xmmsc_playlist_entry_free (GHashTable *entry);
void xmmsc_playback_stop (xmmsc_connection_t *c);
void xmmsc_playback_start (xmmsc_connection_t *c);
void xmmsc_playback_pause (xmmsc_connection_t *c);
void xmmsc_playback_seek_ms (xmmsc_connection_t *c, guint milliseconds);
void xmmsc_playback_seek_samples (xmmsc_connection_t *c, guint samples);
void xmmsc_playback_current_id (xmmsc_connection_t *c);
void xmmsc_playlist_list (xmmsc_connection_t *c);
void xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, guint);
void xmmsc_playlist_sort (xmmsc_connection_t *c, char *property);
void xmmsc_configval_set (xmmsc_connection_t *c, gchar *key, gchar *val);
void xmmsc_file_list (xmmsc_connection_t *c, gchar *path);

void xmmsc_set_callback (xmmsc_connection_t *, gchar *, void (*)(void *,void*), void *);

void xmmsc_glib_setup_mainloop (xmmsc_connection_t *, GMainContext *);

/* sync */
int xmmscs_playback_current_id (xmmsc_connection_t *c);
GHashTable *xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, guint id);
unsigned int * xmmscs_playlist_list (xmmsc_connection_t *c);


#ifdef __cplusplus
}
#endif

#endif
