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

#include "internal/xhash-int.h"
#include "internal/xlist-int.h"
#include "xmms/ipc_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_result_St xmmsc_result_t;

xmmsc_connection_t *xmmsc_init (char *clientname);
int xmmsc_connect (xmmsc_connection_t *, const char *);
void xmmsc_deinit (xmmsc_connection_t *);
void xmmsc_lock_set (xmmsc_connection_t *conn, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *));

char *xmmsc_get_last_error (xmmsc_connection_t *c);
char *xmmsc_encode_path (char *path);
char *xmmsc_decode_path (const char *path);
int xmmsc_entry_format (char *target, int len, const char *fmt, x_hash_t *table);

xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *);

/*
 * PLAYLIST ************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_playlist_shuffle (xmmsc_connection_t *);
xmmsc_result_t *xmmsc_playlist_add (xmmsc_connection_t *, char *);
xmmsc_result_t *xmmsc_playlist_medialibadd (xmmsc_connection_t *c, char *);
xmmsc_result_t *xmmsc_playlist_remove (xmmsc_connection_t *, unsigned int);
xmmsc_result_t *xmmsc_playlist_clear (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_save (xmmsc_connection_t *c, char *filename);
xmmsc_result_t *xmmsc_playlist_list (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, unsigned int);
xmmsc_result_t *xmmsc_playlist_sort (xmmsc_connection_t *c, char *property);
xmmsc_result_t *xmmsc_playlist_set_next (xmmsc_connection_t *c, unsigned int type, int moment);
xmmsc_result_t *xmmsc_playlist_move (xmmsc_connection_t *c, unsigned int id, signed int moves);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_playlist_entry_changed (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c);

/* Syncronous commands */
unsigned int xmmscs_playlist_current_id (xmmsc_connection_t *c);
void xmmsc_playlist_entry_free (x_hash_t *entry);
x_hash_t *xmmscs_playlist_get_mediainfo (xmmsc_connection_t *c, unsigned int id);
x_list_t *xmmscs_playlist_list (xmmsc_connection_t *c);


/*
 * PLAYBACK ************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_playback_stop (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_next (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_start (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_pause (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_current_id (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds);
xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples);
xmmsc_result_t *xmmsc_playback_playtime (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_status (xmmsc_connection_t *c);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_playback_status (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_playback_current_id (xmmsc_connection_t *c);

/* signals */
xmmsc_result_t *xmmsc_signal_playback_playtime (xmmsc_connection_t *c);

/* Syncronous commands */
int xmmscs_playback_playtime (xmmsc_connection_t *c);
unsigned int xmmscs_playback_current_id (xmmsc_connection_t *c);


/*
 * OTHER **************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_configval_set (xmmsc_connection_t *c, char *key, char *val);
xmmsc_result_t *xmmsc_configval_list (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_configval_get (xmmsc_connection_t *c, char *key);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_configval_changed (xmmsc_connection_t *c);

/* Syncronous commands */
x_list_t *xmmscs_configval_list (xmmsc_connection_t *c);
char *xmmscs_configval_get (xmmsc_connection_t *c, char *key);

/*
 * MEDIALIB ***********************************************
 */

/* commands */
xmmsc_result_t *xmmsc_medialib_select (xmmsc_connection_t *conn, const char *query);
xmmsc_result_t *xmmsc_medialib_playlist_save_current (xmmsc_connection_t *conn, const char *name);
xmmsc_result_t *xmmsc_medialib_playlist_load (xmmsc_connection_t *conn, const char *name);


/*
 * MACROS 
 */

#define XMMS_CALLBACK_SET(conn,meth,callback,udata) {\
	xmmsc_result_t *res = meth (conn); \
	xmmsc_result_notifier_set (res, callback, udata);\
	xmmsc_result_unref (res);\
}
	
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

/*
 * RESULTS
 */

typedef void (*xmmsc_result_notifier_t) (xmmsc_result_t *res, void *user_data);

void xmmsc_result_restartable (xmmsc_result_t *res, xmmsc_connection_t *c, unsigned int signalid);
xmmsc_result_t *xmmsc_result_restart (xmmsc_result_t *res);
void xmmsc_result_run (xmmsc_result_t *res, xmms_ipc_msg_t *msg);

xmmsc_result_t *xmmsc_result_new (xmmsc_connection_t *c, guint32 commandid);
void xmmsc_result_ref (xmmsc_result_t *res);
void xmmsc_result_unref (xmmsc_result_t *res);

void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data);
void xmmsc_result_wait (xmmsc_result_t *res);

int xmmsc_result_iserror (xmmsc_result_t *res);
const char * xmmsc_result_get_error (xmmsc_result_t *res);
int xmmsc_result_cid (xmmsc_result_t *res);

int xmmsc_result_get_int (xmmsc_result_t *res, int *r);
int xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r);
int xmmsc_result_get_string (xmmsc_result_t *res, char **r);
int xmmsc_result_get_hashtable (xmmsc_result_t *res, x_hash_t **r);
int xmmsc_result_get_stringlist (xmmsc_result_t *res, x_list_t **r);
int xmmsc_result_get_intlist (xmmsc_result_t *res, x_list_t **r);
int xmmsc_result_get_uintlist (xmmsc_result_t *res, x_list_t **r);
int xmmsc_result_get_playlist_change (xmmsc_result_t *res, unsigned int *change, unsigned int *id, unsigned int *argument);
int xmmsc_result_get_hashlist (xmmsc_result_t *res, x_list_t **r);
void xmmsc_result_seterror (xmmsc_result_t *res, char *errstr);


#ifdef __cplusplus
}
#endif

#endif
