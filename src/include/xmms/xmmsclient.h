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

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_playlist_entry_St { /* no use to share them between client and core */
	char *url;
	unsigned int id;
	x_hash_t *properties;
} xmmsc_playlist_entry_t;

typedef struct xmmsc_file_St {
	char *path;
	int file; /**< set to true if entry is a regular file */
} xmmsc_file_t;

xmmsc_connection_t *xmmsc_init();
int xmmsc_connect (xmmsc_connection_t *, const char *);
void xmmsc_deinit(xmmsc_connection_t *);

char *xmmsc_get_last_error (xmmsc_connection_t *c);
char *xmmsc_encode_path (char *path);
char *xmmsc_decode_path (const char *path);
int xmmsc_entry_format (char *target, int len, const char *fmt, x_hash_t *table);

void xmmsc_quit(xmmsc_connection_t *);

void xmmsc_playlist_shuffle(xmmsc_connection_t *);
void xmmsc_playlist_add (xmmsc_connection_t *, char *);
void xmmsc_playlist_remove (xmmsc_connection_t *, unsigned int);
void xmmsc_playlist_clear (xmmsc_connection_t *c);
void xmmsc_playlist_save (xmmsc_connection_t *c, char *filename);
void xmmsc_playlist_entry_free (x_hash_t *entry);
void xmmsc_playlist_list (xmmsc_connection_t *c);
void xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, unsigned int);
void xmmsc_playlist_sort (xmmsc_connection_t *c, char *property);

void xmmsc_playback_stop (xmmsc_connection_t *c);
void xmmsc_playback_start (xmmsc_connection_t *c);
void xmmsc_playback_pause (xmmsc_connection_t *c);
void xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds);
void xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples);
void xmmsc_playback_current_id (xmmsc_connection_t *c);
void xmmsc_playback_next(xmmsc_connection_t *);
void xmmsc_playback_prev(xmmsc_connection_t *);
void xmmsc_playback_jump (xmmsc_connection_t *c, unsigned int id);

void xmmsc_configval_set (xmmsc_connection_t *c, char *key, char *val);
void xmmsc_file_list (xmmsc_connection_t *c, char *path);

void xmmsc_set_callback (xmmsc_connection_t *, char *, void (*)(void *,void*), void *);
void xmmsc_configval_list (xmmsc_connection_t *c);
void xmmsc_configval_get (xmmsc_connection_t *c, char *key);

/*void xmmsc_glib_setup_mainloop (xmmsc_connection_t *, GMainContext *);*/

/* sync */
int xmmscs_playback_current_id (xmmsc_connection_t *c);
x_hash_t *xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id);
unsigned int * xmmscs_playlist_list (xmmsc_connection_t *c);
x_list_t * xmmscs_configval_list (xmmsc_connection_t *c);
char *xmmscs_configval_get (xmmsc_connection_t *c, char *key);
int xmmsc_playback_current_playtime (xmmsc_connection_t *c);


#ifdef __cplusplus
}
#endif

#endif
