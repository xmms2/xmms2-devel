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

#include "xmms/xmmsclient-result.h"

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

xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *);

xmmsc_result_t *xmmsc_playlist_shuffle(xmmsc_connection_t *);
xmmsc_result_t *xmmsc_playlist_add (xmmsc_connection_t *, char *);
xmmsc_result_t *xmmsc_playlist_remove (xmmsc_connection_t *, unsigned int);
xmmsc_result_t *xmmsc_playlist_clear (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_save (xmmsc_connection_t *c, char *filename);
xmmsc_result_t *xmmsc_playlist_list (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, unsigned int);
xmmsc_result_t *xmmsc_playlist_sort (xmmsc_connection_t *c, char *property);
xmmsc_result_t *xmmsc_playlist_entry_changed (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_changed (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_set_next (xmmsc_connection_t *c, unsigned int type, int moment);
void xmmsc_playlist_entry_free (x_hash_t *entry);

xmmsc_result_t *xmmsc_playback_stop (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_start (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_pause (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_current_id (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_next(xmmsc_connection_t *);
xmmsc_result_t *xmmsc_playback_prev(xmmsc_connection_t *);
xmmsc_result_t *xmmsc_playback_status (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_playtime (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_statistics (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_jump (xmmsc_connection_t *c, unsigned int id);
xmmsc_result_t *xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds);
xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples);

xmmsc_result_t *xmmsc_configval_set (xmmsc_connection_t *c, char *key, char *val);
xmmsc_result_t *xmmsc_file_list (xmmsc_connection_t *c, char *path);

void xmmsc_set_callback (xmmsc_connection_t *, char *, void (*)(void *,void*), void *);
xmmsc_result_t *xmmsc_configval_list (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_configval_get (xmmsc_connection_t *c, char *key);

xmmsc_result_t *xmmsc_output_mixer_set (xmmsc_connection_t *c, int left, int right);
xmmsc_result_t *xmmsc_output_mixer_get (xmmsc_connection_t *c);

/*void xmmsc_glib_setup_mainloop (xmmsc_connection_t *, GMainContext *);*/

/* sync */
unsigned int xmmscs_playback_current_id (xmmsc_connection_t *c);
x_hash_t *xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id);
x_list_t *xmmscs_playlist_list (xmmsc_connection_t *c);
x_list_t *xmmscs_configval_list (xmmsc_connection_t *c);
char *xmmscs_configval_get (xmmsc_connection_t *c, char *key);
int xmmscs_playback_playtime (xmmsc_connection_t *c);

#define XMMS_CALLBACK_SET(conn,meth,callback,udata) {\
	xmmsc_result_t *res = meth (conn); \
	xmmsc_result_notifier_set (res, callback, udata);\
	xmmsc_result_unref (res);\
}
	

/* API ERROR CODES */

#define XMMS_ERROR_API_UNRECOVERABLE 1
#define XMMS_ERROR_API_RESULT_NOT_SANE 2

/* end ERROR CODES */

typedef enum {
	XMMSC_PLAYLIST_ADD,
	XMMSC_PLAYLIST_SET_POS,
	XMMSC_PLAYLIST_SHUFFLE,
	XMMSC_PLAYLIST_REMOVE,
	XMMSC_PLAYLIST_CLEAR,
	XMMSC_PLAYLIST_MOVE,
	XMMSC_PLAYLIST_SORT
} xmmsc_playlist_changed_actions_t;

typedef enum {
	XMMSC_PLAYBACK_PLAY,
	XMMSC_PLAYBACK_STOP,
	XMMSC_PLAYBACK_PAUSE,
} xmms_playback_status_t;


#ifdef _cplusplus
}
#endif

#endif
