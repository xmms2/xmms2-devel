/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#include "xmmsc/xmmsc_stdint.h"
#include "xmmsc/xmmsc_ipc_msg.h"
#include "xmmsc/xmmsc_idnumbers.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_result_St xmmsc_result_t;
typedef struct xmmsc_coll_St xmmsc_coll_t;

typedef enum {
	XMMSC_RESULT_CLASS_DEFAULT,
	XMMSC_RESULT_CLASS_SIGNAL,
	XMMSC_RESULT_CLASS_BROADCAST
} xmmsc_result_type_t;

typedef struct xmmsc_query_attribute_St {
	char *key;
	char *value;
} xmmsc_query_attribute_t;

xmmsc_connection_t *xmmsc_init (const char *clientname);
int xmmsc_connect (xmmsc_connection_t *, const char *);
void xmmsc_unref (xmmsc_connection_t *c);
void xmmsc_lock_set (xmmsc_connection_t *conn, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *));
void xmmsc_disconnect_callback_set (xmmsc_connection_t *c, void (*callback) (void*), void *userdata);

void xmmsc_io_need_out_callback_set (xmmsc_connection_t *c, void (*callback) (int, void*), void *userdata);
void xmmsc_io_disconnect (xmmsc_connection_t *c);
int xmmsc_io_want_out (xmmsc_connection_t *c);
int xmmsc_io_out_handle (xmmsc_connection_t *c);
int xmmsc_io_in_handle (xmmsc_connection_t *c);
int xmmsc_io_fd_get (xmmsc_connection_t *c);

char *xmmsc_get_last_error (xmmsc_connection_t *c);

xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *);

xmmsc_result_t *xmmsc_broadcast_quit (xmmsc_connection_t *c);


char *xmmsc_querygen_and (xmmsc_query_attribute_t *attributes, unsigned n);
char *xmmsc_sqlite_prepare_string (const char *input);


/*
 * PLAYLIST ************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_playlist_shuffle (xmmsc_connection_t *);
xmmsc_result_t *xmmsc_playlist_add_args (xmmsc_connection_t *, const char *, int, const char **);
xmmsc_result_t *xmmsc_playlist_add_url (xmmsc_connection_t *c, const char *url);
xmmsc_result_t *xmmsc_playlist_add_id (xmmsc_connection_t *c, uint32_t id);
xmmsc_result_t *xmmsc_playlist_remove (xmmsc_connection_t *, uint32_t);
xmmsc_result_t *xmmsc_playlist_clear (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_list (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_sort (xmmsc_connection_t *c, const char*);
xmmsc_result_t *xmmsc_playlist_set_next (xmmsc_connection_t *c, uint32_t);
xmmsc_result_t *xmmsc_playlist_set_next_rel (xmmsc_connection_t *c, int32_t);
xmmsc_result_t *xmmsc_playlist_move (xmmsc_connection_t *c, uint32_t, uint32_t);
xmmsc_result_t *xmmsc_playlist_current_pos (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playlist_insert_args (xmmsc_connection_t *c, int pos, const char *url, int numargs, const char **args);
xmmsc_result_t *xmmsc_playlist_insert_url (xmmsc_connection_t *c, int pos, const char *url);
xmmsc_result_t *xmmsc_playlist_insert_id (xmmsc_connection_t *c, int pos, uint32_t id);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_playlist_current_pos (xmmsc_connection_t *c);


/*
 * PLAYBACK ************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_playback_stop (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_tickle (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_start (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_pause (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_current_id (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_seek_ms (xmmsc_connection_t *c, uint32_t milliseconds);
xmmsc_result_t *xmmsc_playback_seek_ms_rel (xmmsc_connection_t *c, int milliseconds);
xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, uint32_t samples);
xmmsc_result_t *xmmsc_playback_seek_samples_rel (xmmsc_connection_t *c, int samples);
xmmsc_result_t *xmmsc_playback_playtime (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_status (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_playback_volume_set (xmmsc_connection_t *c, const char *channel, uint32_t volume);
xmmsc_result_t *xmmsc_playback_volume_get (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_playback_volume_changed (xmmsc_connection_t *c);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_playback_status (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_playback_current_id (xmmsc_connection_t *c);

/* signals */
xmmsc_result_t *xmmsc_signal_playback_playtime (xmmsc_connection_t *c);


/*
 * CONFIG **************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_configval_set (xmmsc_connection_t *c, const char *key, const char *val);
xmmsc_result_t *xmmsc_configval_list (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_configval_get (xmmsc_connection_t *c, const char *key);
xmmsc_result_t *xmmsc_configval_register (xmmsc_connection_t *c, const char *valuename, const char *defaultvalue);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_configval_changed (xmmsc_connection_t *c);


/*
 * STATS **************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_plugin_list (xmmsc_connection_t *c, uint32_t type);
xmmsc_result_t *xmmsc_main_stats (xmmsc_connection_t *c);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_mediainfo_reader_status (xmmsc_connection_t *c);

/* signals */
xmmsc_result_t *xmmsc_signal_visualisation_data (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_signal_mediainfo_reader_unindexed (xmmsc_connection_t *c);


/*
 * MEDIALIB ***********************************************
 */

/* commands */
int xmmsc_entry_format (char *target, int len, const char *fmt, xmmsc_result_t *res);
xmmsc_result_t *xmmsc_medialib_select (xmmsc_connection_t *conn, const char *query);
xmmsc_result_t *xmmsc_medialib_playlist_save_current (xmmsc_connection_t *conn, const char *name);
xmmsc_result_t *xmmsc_medialib_playlist_load (xmmsc_connection_t *conn, const char *name);
xmmsc_result_t *xmmsc_medialib_add_entry (xmmsc_connection_t *conn, const char *url);
xmmsc_result_t *xmmsc_medialib_add_entry_args (xmmsc_connection_t *conn, const char *url, int numargs, const char **args);
xmmsc_result_t *xmmsc_medialib_get_info (xmmsc_connection_t *, uint32_t);
xmmsc_result_t *xmmsc_medialib_add_to_playlist (xmmsc_connection_t *c, const char *query);
xmmsc_result_t *xmmsc_medialib_playlists_list (xmmsc_connection_t *);
xmmsc_result_t *xmmsc_medialib_playlist_list (xmmsc_connection_t *, const char *playlist);
xmmsc_result_t *xmmsc_medialib_playlist_import (xmmsc_connection_t *conn, const char *playlist, const char *url);
xmmsc_result_t *xmmsc_medialib_playlist_export (xmmsc_connection_t *conn, const char *playlist, const char *mime);
xmmsc_result_t *xmmsc_medialib_playlist_remove (xmmsc_connection_t *conn, const char *playlist);
xmmsc_result_t *xmmsc_medialib_path_import (xmmsc_connection_t *conn, const char *path);
xmmsc_result_t *xmmsc_medialib_rehash (xmmsc_connection_t *conn, uint32_t id);
xmmsc_result_t *xmmsc_medialib_get_id (xmmsc_connection_t *conn, const char *url);
xmmsc_result_t *xmmsc_medialib_remove_entry (xmmsc_connection_t *conn, uint32_t entry);

xmmsc_result_t *xmmsc_medialib_entry_property_set_int (xmmsc_connection_t *c, uint32_t id, const char *key, int32_t value);
xmmsc_result_t *xmmsc_medialib_entry_property_set_int_with_source (xmmsc_connection_t *c, uint32_t id, const char *source, const char *key, int32_t value);

xmmsc_result_t *xmmsc_medialib_entry_property_set_str (xmmsc_connection_t *c, uint32_t id, const char *key, const char *value);
xmmsc_result_t *xmmsc_medialib_entry_property_set_str_with_source (xmmsc_connection_t *c, uint32_t id, const char *source, const char *key, const char *value);

xmmsc_result_t *xmmsc_medialib_entry_property_remove (xmmsc_connection_t *c, uint32_t id, const char *key);
xmmsc_result_t *xmmsc_medialib_entry_property_remove_with_source (xmmsc_connection_t *c, uint32_t id, const char *source, const char *key);

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_medialib_entry_changed (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_medialib_entry_added (xmmsc_connection_t *c);
xmmsc_result_t *xmmsc_broadcast_medialib_playlist_loaded (xmmsc_connection_t *c);


/*
 * COLLECTION ***********************************************
 */

typedef void (*xmmsc_coll_attribute_foreach_func) (const char *key, const char *value, void *udata);

xmmsc_coll_t* xmmsc_coll_new (xmmsc_coll_type_t type);
void xmmsc_coll_ref (xmmsc_coll_t *coll);
void xmmsc_coll_unref (xmmsc_coll_t *coll);
void xmmsc_coll_free (xmmsc_coll_t *coll);

void xmmsc_coll_set_type (xmmsc_coll_t *coll, xmmsc_coll_type_t type);
void xmmsc_coll_set_idlist (xmmsc_coll_t *coll, unsigned int ids[]);
void xmmsc_coll_add_operand (xmmsc_coll_t *coll, xmmsc_coll_t *op);
void xmmsc_coll_remove_operand (xmmsc_coll_t *coll, xmmsc_coll_t *op);

xmmsc_coll_type_t xmmsc_coll_get_type (xmmsc_coll_t *coll);
uint32_t* xmmsc_coll_get_idlist (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_first (xmmsc_coll_t *coll);
int xmmsc_coll_operand_list_entry (xmmsc_coll_t *coll, xmmsc_coll_t **operand);
int xmmsc_coll_operand_list_next (xmmsc_coll_t *coll);

void xmmsc_coll_attribute_set (xmmsc_coll_t *coll, const char *key, const char *value);
int xmmsc_coll_attribute_remove (xmmsc_coll_t *coll, const char *key);
int xmmsc_coll_attribute_get (xmmsc_coll_t *coll, const char *key, char **value);
void xmmsc_coll_attribute_foreach (xmmsc_coll_t *coll, xmmsc_coll_attribute_foreach_func func, void *user_data);

xmmsc_coll_t* xmmsc_coll_universe ();

xmmsc_result_t* xmmsc_coll_get (xmmsc_connection_t *conn, char *collname, xmmsc_coll_namespace_t ns);
xmmsc_result_t* xmmsc_coll_list (xmmsc_connection_t *conn, xmmsc_coll_namespace_t ns);
xmmsc_result_t* xmmsc_coll_save (xmmsc_connection_t *conn, xmmsc_coll_t *coll, char* name, xmmsc_coll_namespace_t ns);
xmmsc_result_t* xmmsc_coll_remove (xmmsc_connection_t *conn, char* name, xmmsc_coll_namespace_t ns);
xmmsc_result_t* xmmsc_coll_find (xmmsc_connection_t *conn, unsigned int mediaid, xmmsc_coll_namespace_t ns);

xmmsc_result_t* xmmsc_coll_query_ids  (xmmsc_connection_t *conn, xmmsc_coll_t *coll, const char* order[], unsigned int limit_start, unsigned int limit_len);
xmmsc_result_t* xmmsc_coll_query_infos (xmmsc_connection_t *conn, xmmsc_coll_t *coll, const char* order[], unsigned int limit_start, unsigned int limit_len, const char* fetch[], const char* group[]);


/*
 * MACROS 
 */

#define XMMS_CALLBACK_SET(conn,meth,callback,udata) {\
	xmmsc_result_t *res = meth (conn); \
	xmmsc_result_notifier_set (res, callback, udata);\
	xmmsc_result_unref (res);\
}
	
/*
 * RESULTS
 */

typedef void (*xmmsc_result_notifier_t) (xmmsc_result_t *res, void *user_data);

xmmsc_result_t *xmmsc_result_restart (xmmsc_result_t *res);
void xmmsc_result_run (xmmsc_result_t *res, xmms_ipc_msg_t *msg);
xmmsc_result_type_t xmmsc_result_get_class (xmmsc_result_t *res);
void xmmsc_result_disconnect (xmmsc_result_t *res);

void xmmsc_result_ref (xmmsc_result_t *res);
void xmmsc_result_unref (xmmsc_result_t *res);

void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data);
void xmmsc_result_wait (xmmsc_result_t *res);

int xmmsc_result_iserror (xmmsc_result_t *res);
const char * xmmsc_result_get_error (xmmsc_result_t *res);

int xmmsc_result_get_int (xmmsc_result_t *res, int32_t *r);
int xmmsc_result_get_uint (xmmsc_result_t *res, uint32_t *r);
int xmmsc_result_get_string (xmmsc_result_t *res, char **r);
int xmmsc_result_get_collection (xmmsc_result_t *conn, xmmsc_coll_t **coll);

typedef enum {
	XMMSC_RESULT_VALUE_TYPE_NONE = XMMS_OBJECT_CMD_ARG_NONE,
	XMMSC_RESULT_VALUE_TYPE_UINT32 = XMMS_OBJECT_CMD_ARG_UINT32,
	XMMSC_RESULT_VALUE_TYPE_INT32 = XMMS_OBJECT_CMD_ARG_INT32,
	XMMSC_RESULT_VALUE_TYPE_STRING = XMMS_OBJECT_CMD_ARG_STRING,
	XMMSC_RESULT_VALUE_TYPE_COLL = XMMS_OBJECT_CMD_ARG_COLL
} xmmsc_result_value_type_t;

typedef void (*xmmsc_propdict_foreach_func) (const void *key, xmmsc_result_value_type_t type, const void *value, const char *source, void *user_data);
typedef void (*xmmsc_dict_foreach_func) (const void *key, xmmsc_result_value_type_t type, const void *value, void *user_data);

xmmsc_result_value_type_t xmmsc_result_get_dict_entry_type (xmmsc_result_t *res, const char *key);
int xmmsc_result_get_dict_entry_string (xmmsc_result_t *res, const char *key, char **r);
int xmmsc_result_get_dict_entry_int (xmmsc_result_t *res, const char *key, int32_t *r);
int xmmsc_result_get_dict_entry_uint (xmmsc_result_t *res, const char *key, uint32_t *r);
int xmmsc_result_get_dict_entry_collection (xmmsc_result_t *conn, const char *key, xmmsc_coll_t **coll);
int xmmsc_result_dict_foreach (xmmsc_result_t *res, xmmsc_dict_foreach_func func, void *user_data);
int xmmsc_result_propdict_foreach (xmmsc_result_t *res, xmmsc_propdict_foreach_func func, void *user_data);
void xmmsc_result_source_preference_set (xmmsc_result_t *res, const char **preference);

int xmmsc_result_is_list (xmmsc_result_t *res);
int xmmsc_result_list_next (xmmsc_result_t *res);
int xmmsc_result_list_first (xmmsc_result_t *res);
int xmmsc_result_list_valid (xmmsc_result_t *res);

int xmmsc_result_get_type (xmmsc_result_t *res);

const char *xmmsc_result_decode_url (xmmsc_result_t *res, const char *string);

#ifdef __cplusplus
}
#endif

#endif
