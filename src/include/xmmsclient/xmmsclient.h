/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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

#include <xmmsc/xmmsc_compiler.h>
#include <xmmsc/xmmsc_stdint.h>
#include <xmmsc/xmmsc_ipc_msg.h>
#include <xmmsc/xmmsc_idnumbers.h>
#include <xmmsc/xmmsv.h>
#include <xmmsc/xmmsv_coll.h>
#include <xmmsc/xmmsv_c2c.h>
#include <xmmsc/xmmsv_service.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmmsc_connection_St xmmsc_connection_t;
typedef struct xmmsc_result_St xmmsc_result_t;

typedef enum {
	XMMSC_RESULT_CLASS_DEFAULT,
	XMMSC_RESULT_CLASS_SIGNAL,
	XMMSC_RESULT_CLASS_BROADCAST
} xmmsc_result_type_t;

typedef void (*xmmsc_disconnect_func_t) (void *user_data);
typedef void (*xmmsc_user_data_free_func_t) (void *user_data);
typedef void (*xmmsc_io_need_out_callback_func_t) (int, void*);

xmmsc_connection_t *xmmsc_init (const char *clientname) XMMS_PUBLIC;
int xmmsc_connect (xmmsc_connection_t *, const char *) XMMS_PUBLIC;
xmmsc_connection_t *xmmsc_ref (xmmsc_connection_t *c) XMMS_PUBLIC;
void xmmsc_unref (xmmsc_connection_t *c) XMMS_PUBLIC;
void xmmsc_lock_set (xmmsc_connection_t *conn, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *)) XMMS_PUBLIC;
void xmmsc_disconnect_callback_set (xmmsc_connection_t *c, xmmsc_disconnect_func_t disconnect_func, void *userdata) XMMS_PUBLIC;
void xmmsc_disconnect_callback_set_full (xmmsc_connection_t *c, xmmsc_disconnect_func_t disconnect_func, void *userdata, xmmsc_user_data_free_func_t free_func) XMMS_PUBLIC;

void xmmsc_io_need_out_callback_set (xmmsc_connection_t *c, xmmsc_io_need_out_callback_func_t callback, void *userdata) XMMS_PUBLIC;
void xmmsc_io_need_out_callback_set_full (xmmsc_connection_t *c, xmmsc_io_need_out_callback_func_t callback, void *userdata, xmmsc_user_data_free_func_t free_func) XMMS_PUBLIC;
void xmmsc_io_disconnect (xmmsc_connection_t *c) XMMS_PUBLIC;
int xmmsc_io_want_out (xmmsc_connection_t *c) XMMS_PUBLIC;
int xmmsc_io_out_handle (xmmsc_connection_t *c) XMMS_PUBLIC;
int xmmsc_io_in_handle (xmmsc_connection_t *c) XMMS_PUBLIC;
int xmmsc_io_fd_get (xmmsc_connection_t *c) XMMS_PUBLIC;

char *xmmsc_get_last_error (xmmsc_connection_t *c) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *c) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_broadcast_quit (xmmsc_connection_t *c) XMMS_PUBLIC;

/* get user config dir */
const char *xmmsc_userconfdir_get (char *buf, int len) XMMS_PUBLIC;


/* Encoding of urls */
char *xmmsc_medialib_encode_url_full (const char *url, xmmsv_t *args) XMMS_PUBLIC XMMS_DEPRECATED;
char *xmmsc_medialib_encode_url (const char *url) XMMS_PUBLIC XMMS_DEPRECATED;


/*
 * PLAYLIST ************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_playlist_list (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_create (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_shuffle (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_add_args (xmmsc_connection_t *c, const char *playlist, const char *, int, const char **) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsc_result_t *xmmsc_playlist_add_full (xmmsc_connection_t *c, const char *playlist, const char *, xmmsv_t *) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_add_url (xmmsc_connection_t *c, const char *playlist, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_add_id (xmmsc_connection_t *c, const char *playlist, int id) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_add_encoded (xmmsc_connection_t *c, const char *playlist, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_add_idlist (xmmsc_connection_t *c, const char *playlist, xmmsv_t *coll) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_add_collection (xmmsc_connection_t *c, const char *playlist, xmmsv_t *coll, xmmsv_t *order) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_remove_entry (xmmsc_connection_t *c, const char *playlist, int) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_clear (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_replace (xmmsc_connection_t *c, const char *playlist, xmmsv_t *coll, xmms_playlist_position_action_t action) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_remove (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_list_entries (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_sort (xmmsc_connection_t *c, const char *playlist, xmmsv_t *properties) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_set_next (xmmsc_connection_t *c, int32_t) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_set_next_rel (xmmsc_connection_t *c, int32_t) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_move_entry (xmmsc_connection_t *c, const char *playlist, int, int) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_current_pos (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_current_active (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_insert_args (xmmsc_connection_t *c, const char *playlist, int pos, const char *url, int numargs, const char **args) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsc_result_t *xmmsc_playlist_insert_full (xmmsc_connection_t *c, const char *playlist, int pos, const char *url, xmmsv_t *args) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_insert_url (xmmsc_connection_t *c, const char *playlist, int pos, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_insert_id (xmmsc_connection_t *c, const char *playlist, int pos, int32_t id) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_insert_encoded (xmmsc_connection_t *c, const char *playlist, int pos, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_insert_collection (xmmsc_connection_t *c, const char *playlist, int pos, xmmsv_t *coll, xmmsv_t *order) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_load (xmmsc_connection_t *c, const char *playlist) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_radd (xmmsc_connection_t *c, const char *playlist, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_radd_encoded (xmmsc_connection_t *c, const char *playlist, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_rinsert (xmmsc_connection_t *c, const char *playlist, int pos, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playlist_rinsert_encoded (xmmsc_connection_t *c, const char *playlist, int pos, const char *url) XMMS_PUBLIC;

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_playlist_current_pos (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_playlist_loaded (xmmsc_connection_t *c) XMMS_PUBLIC;


/*
 * PLAYBACK ************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_playback_stop (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_tickle (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_start (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_pause (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_current_id (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_seek_ms (xmmsc_connection_t *c, int milliseconds, xmms_playback_seek_mode_t whence) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, int samples, xmms_playback_seek_mode_t whence) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_playtime (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_status (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_volume_set (xmmsc_connection_t *c, const char *channel, int volume) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_playback_volume_get (xmmsc_connection_t *c) XMMS_PUBLIC;

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_playback_volume_changed (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_playback_status (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_playback_current_id (xmmsc_connection_t *c) XMMS_PUBLIC;

/* signals */
xmmsc_result_t *xmmsc_signal_playback_playtime (xmmsc_connection_t *c) XMMS_PUBLIC;


/*
 * CONFIG **************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_config_set_value (xmmsc_connection_t *c, const char *key, const char *val) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_config_list_values (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_config_get_value (xmmsc_connection_t *c, const char *key) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_config_register_value (xmmsc_connection_t *c, const char *valuename, const char *defaultvalue) XMMS_PUBLIC;

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_config_value_changed (xmmsc_connection_t *c) XMMS_PUBLIC;


/*
 * STATS **************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_main_list_plugins (xmmsc_connection_t *c, xmms_plugin_type_t type) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_main_stats (xmmsc_connection_t *c) XMMS_PUBLIC;

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_mediainfo_reader_status (xmmsc_connection_t *c) XMMS_PUBLIC;

/* signals */
xmmsc_result_t *xmmsc_signal_mediainfo_reader_unindexed (xmmsc_connection_t *c) XMMS_PUBLIC;


/*
 * VISUALIZATION **************************************************
 */

/* commands */
xmmsc_result_t *xmmsc_visualization_version (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_visualization_init (xmmsc_connection_t *c) XMMS_PUBLIC;
/* Returns 1 on success, 0 on failure. In that case, see xmmsc_get_last_error() */
int xmmsc_visualization_init_handle (xmmsc_result_t *res) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_visualization_start (xmmsc_connection_t *c, int vv) XMMS_PUBLIC;
void xmmsc_visualization_start_handle (xmmsc_connection_t *c, xmmsc_result_t *res) XMMS_PUBLIC;
bool xmmsc_visualization_started (xmmsc_connection_t *c, int vv) XMMS_PUBLIC;
bool xmmsc_visualization_errored (xmmsc_connection_t *c, int vv) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_visualization_property_set (xmmsc_connection_t *c, int v, const char *key, const char *value) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_visualization_properties_set (xmmsc_connection_t *c, int v, xmmsv_t *props) XMMS_PUBLIC;
/*
 * drawtime: expected time needed to process the data in milliseconds after collecting it
    if >= 0, the data is returned as soon as currenttime >= (playtime - drawtime);
             data is thrown away if playtime < currenttime, but not if playtime < currenttime - drawtime
    if  < 0, the data is returned as soon as available, and no old data is thrown away
 * blocking: time limit given in ms to wait for data. The process will sleep until new data is available, or the
             limit is reached. But if data is found, it could still wait until it is current (see drawtime).
 * returns size read on success, -1 on failure (server killed!) and 0 if no data is available yet (retry later)
 * if a signal is received while waiting for data, 0 is returned, even before time ran out
 * Note: the size read can be less than expected (for example, on song end). Check it!
 */
int xmmsc_visualization_chunk_get (xmmsc_connection_t *c, int vv, short *buffer, int drawtime, unsigned int blocking) XMMS_PUBLIC;
void xmmsc_visualization_shutdown (xmmsc_connection_t *c, int v) XMMS_PUBLIC;


/*
 * MEDIALIB ***********************************************
 */

/* commands */
xmmsc_result_t *xmmsc_medialib_add_entry (xmmsc_connection_t *conn, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_add_entry_args (xmmsc_connection_t *conn, const char *url, int numargs, const char **args) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsc_result_t *xmmsc_medialib_add_entry_full (xmmsc_connection_t *conn, const char *url, xmmsv_t *args) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_add_entry_encoded (xmmsc_connection_t *conn, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_get_info (xmmsc_connection_t *, int) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_path_import (xmmsc_connection_t *conn, const char *path) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsc_result_t *xmmsc_medialib_path_import_encoded (xmmsc_connection_t *conn, const char *path) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsc_result_t *xmmsc_medialib_import_path (xmmsc_connection_t *conn, const char *path) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_import_path_encoded (xmmsc_connection_t *conn, const char *path) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_rehash (xmmsc_connection_t *conn, int id) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_get_id (xmmsc_connection_t *conn, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_get_id_encoded (xmmsc_connection_t *conn, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_remove_entry (xmmsc_connection_t *conn, int entry) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_move_entry (xmmsc_connection_t *conn, int entry, const char *url) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_medialib_entry_property_set_int (xmmsc_connection_t *c, int id, const char *key, int32_t value) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_entry_property_set_int_with_source (xmmsc_connection_t *c, int id, const char *source, const char *key, int32_t value) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_medialib_entry_property_set_str (xmmsc_connection_t *c, int id, const char *key, const char *value) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_entry_property_set_str_with_source (xmmsc_connection_t *c, int id, const char *source, const char *key, const char *value) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_medialib_entry_property_remove (xmmsc_connection_t *c, int id, const char *key) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_medialib_entry_property_remove_with_source (xmmsc_connection_t *c, int id, const char *source, const char *key) XMMS_PUBLIC;

/* XForm object */
xmmsc_result_t *xmmsc_xform_media_browse (xmmsc_connection_t *c, const char *url) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_xform_media_browse_encoded (xmmsc_connection_t *c, const char *url) XMMS_PUBLIC;

/* Bindata object */
xmmsc_result_t *xmmsc_bindata_add (xmmsc_connection_t *c, const unsigned char *data, unsigned int len) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_bindata_retrieve (xmmsc_connection_t *c, const char *hash) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_bindata_remove (xmmsc_connection_t *c, const char *hash) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_bindata_list (xmmsc_connection_t *c) XMMS_PUBLIC;

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_medialib_entry_changed (xmmsc_connection_t *c) XMMS_PUBLIC  XMMS_DEPRECATED;
xmmsc_result_t *xmmsc_broadcast_medialib_entry_updated (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_medialib_entry_added (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_medialib_entry_removed (xmmsc_connection_t *c) XMMS_PUBLIC;


/*
 * COLLECTION ***********************************************
 */

xmmsc_result_t* xmmsc_coll_get (xmmsc_connection_t *conn, const char *collname, xmmsv_coll_namespace_t ns) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_list (xmmsc_connection_t *conn, xmmsv_coll_namespace_t ns) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_save (xmmsc_connection_t *conn, xmmsv_t *coll, const char* name, xmmsv_coll_namespace_t ns) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_remove (xmmsc_connection_t *conn, const char* name, xmmsv_coll_namespace_t ns) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_find (xmmsc_connection_t *conn, int mediaid, xmmsv_coll_namespace_t ns) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_rename (xmmsc_connection_t *conn, const char* from_name, const char* to_name, xmmsv_coll_namespace_t ns) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_coll_idlist_from_playlist_file (xmmsc_connection_t *conn, const char *path) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_sync (xmmsc_connection_t *conn) XMMS_PUBLIC;

xmmsc_result_t* xmmsc_coll_query_ids (xmmsc_connection_t *conn, xmmsv_t *coll, xmmsv_t *order, int limit_start, int limit_len) XMMS_PUBLIC;
xmmsc_result_t* xmmsc_coll_query_infos (xmmsc_connection_t *conn, xmmsv_t *coll, xmmsv_t *order, int limit_start, int limit_len, xmmsv_t *fetch, xmmsv_t *group) XMMS_PUBLIC XMMS_DEPRECATED;
xmmsc_result_t* xmmsc_coll_query (xmmsc_connection_t *conn, xmmsv_t *coll, xmmsv_t *fetch) XMMS_PUBLIC;

/* string-to-collection parser */
typedef enum {
	XMMS_COLLECTION_TOKEN_INVALID,
	XMMS_COLLECTION_TOKEN_GROUP_OPEN,
	XMMS_COLLECTION_TOKEN_GROUP_CLOSE,
	XMMS_COLLECTION_TOKEN_REFERENCE,
	XMMS_COLLECTION_TOKEN_SYMBOL_ID,
	XMMS_COLLECTION_TOKEN_STRING,
	XMMS_COLLECTION_TOKEN_PATTERN,
	XMMS_COLLECTION_TOKEN_INTEGER,
	XMMS_COLLECTION_TOKEN_SEQUENCE,
	XMMS_COLLECTION_TOKEN_PROP_LONG,
	XMMS_COLLECTION_TOKEN_PROP_SHORT,
	XMMS_COLLECTION_TOKEN_OPSET_UNION,
	XMMS_COLLECTION_TOKEN_OPSET_INTERSECTION,
	XMMS_COLLECTION_TOKEN_OPSET_COMPLEMENT,
	XMMS_COLLECTION_TOKEN_OPFIL_HAS,
	XMMS_COLLECTION_TOKEN_OPFIL_EQUALS,
	XMMS_COLLECTION_TOKEN_OPFIL_MATCH,
	XMMS_COLLECTION_TOKEN_OPFIL_SMALLER,
	XMMS_COLLECTION_TOKEN_OPFIL_GREATER,
	XMMS_COLLECTION_TOKEN_OPFIL_SMALLEREQ,
	XMMS_COLLECTION_TOKEN_OPFIL_GREATEREQ
} xmmsv_coll_token_type_t;

#define XMMS_COLLECTION_TOKEN_CUSTOM 32

typedef struct xmmsv_coll_token_St xmmsv_coll_token_t;

struct xmmsv_coll_token_St {
	xmmsv_coll_token_type_t type;
	char *string;
	xmmsv_coll_token_t *next;
};

typedef xmmsv_coll_token_t* (*xmmsv_coll_parse_tokens_f) (const char *str, const char **newpos);
typedef xmmsv_t* (*xmmsv_coll_parse_build_f) (xmmsv_coll_token_t *tokens);

int xmmsv_coll_parse (const char *pattern, xmmsv_t** coll) XMMS_PUBLIC;
int xmmsv_coll_parse_custom (const char *pattern, xmmsv_coll_parse_tokens_f parse_f, xmmsv_coll_parse_build_f build_f, xmmsv_t** coll) XMMS_PUBLIC;

xmmsv_t *xmmsv_coll_default_parse_build (xmmsv_coll_token_t *tokens) XMMS_PUBLIC;
xmmsv_coll_token_t *xmmsv_coll_default_parse_tokens (const char *str, const char **newpos) XMMS_PUBLIC;


/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_collection_changed (xmmsc_connection_t *c) XMMS_PUBLIC;

/*
 * C2C ***********************************************
 */

/* methods */
xmmsc_result_t *xmmsc_c2c_send (xmmsc_connection_t *c, int dest, xmms_c2c_reply_policy_t reply_policy, xmmsv_t *payload) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_c2c_reply (xmmsc_connection_t *c, int msgid, xmms_c2c_reply_policy_t reply_policy, xmmsv_t *payload) XMMS_PUBLIC;
int32_t xmmsc_c2c_get_own_id (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_c2c_get_connected_clients (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_c2c_ready (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_c2c_get_ready_clients (xmmsc_connection_t *c) XMMS_PUBLIC;

/* broadcasts */
xmmsc_result_t *xmmsc_broadcast_c2c_message (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_c2c_ready (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_c2c_client_connected (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_broadcast_c2c_client_disconnected (xmmsc_connection_t *c) XMMS_PUBLIC;

/*
 * SERVICE *******************************************
 */

typedef enum {
	XMMSC_SC_CALL,
	XMMSC_SC_BROADCAST_SUBSCRIBE,
	XMMSC_SC_INTROSPECT
} xmmsc_sc_commands_t;

/* Special keys for message fields. */
#define XMMSC_SC_CMD_KEY "libxmmsclient-sc-command"
#define XMMSC_SC_ARGS_KEY "libxmmsclient-sc-args"

/* Special keys for individual commands */
#define XMMSC_SC_CALL_METHOD_KEY "libxmmsclient-sc-call-method"
#define XMMSC_SC_CALL_PARGS_KEY "libxmmsclient-sc-call-pargs"
#define XMMSC_SC_CALL_NARGS_KEY "libxmmsclient-sc-call-nargs"

#define XMMSC_SC_INTROSPECT_PATH_KEY "libxmmsclient-sc-introspect-path"
#define XMMSC_SC_INTROSPECT_TYPE_KEY "libxmmsclient-sc-introspect-type"
#define XMMSC_SC_INTROSPECT_KEYFILTER_KEY "libxmmsclient-sc-introspect-keyfilter"

typedef struct xmmsc_sc_namespace_St xmmsc_sc_namespace_t;
typedef xmmsv_t *(*xmmsc_sc_method_t) (xmmsv_t *positional_args, xmmsv_t *named_args, void *user_data);

xmmsc_sc_namespace_t *xmmsc_sc_init (xmmsc_connection_t *c) XMMS_PUBLIC;

/* api creation */
xmmsc_sc_namespace_t *xmmsc_sc_namespace_root (xmmsc_connection_t *c) XMMS_PUBLIC;
xmmsc_sc_namespace_t *xmmsc_sc_namespace_lookup (xmmsc_connection_t *c, xmmsv_t *nms) XMMS_PUBLIC;
xmmsc_sc_namespace_t *xmmsc_sc_namespace_new (xmmsc_sc_namespace_t *parent, const char *name, const char *docstring) XMMS_PUBLIC;
xmmsc_sc_namespace_t *xmmsc_sc_namespace_get (xmmsc_sc_namespace_t *parent, const char *name) XMMS_PUBLIC;

bool xmmsc_sc_namespace_add_constant (xmmsc_sc_namespace_t *nms, const char *key, xmmsv_t *value) XMMS_PUBLIC;
void xmmsc_sc_namespace_remove_constant (xmmsc_sc_namespace_t *nms, const char *key) XMMS_PUBLIC;

/* broadcasts */
bool xmmsc_sc_namespace_add_method (xmmsc_sc_namespace_t *nms, xmmsc_sc_method_t method, const char *name, const char *docstring, xmmsv_t *positional_args, xmmsv_t *named_args, bool va_positional, bool va_named, void *userdata) XMMS_PUBLIC;
bool xmmsc_sc_namespace_add_method_noarg (xmmsc_sc_namespace_t *nms, xmmsc_sc_method_t method, const char *name, const char *docstring, void *userdata) XMMS_PUBLIC;

bool xmmsc_sc_namespace_add_broadcast (xmmsc_sc_namespace_t *nms, const char *name, const char *docstring) XMMS_PUBLIC;

void xmmsc_sc_namespace_remove (xmmsc_sc_namespace_t *nms, xmmsv_t *path) XMMS_PUBLIC;

/* void xmmsc_sc_api_ready (xmmsc_connection_t *c); */

/* broadcasts */
bool xmmsc_sc_broadcast_emit (xmmsc_connection_t *c, xmmsv_t *broadcast, xmmsv_t *value) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_sc_broadcast_subscribe (xmmsc_connection_t *c, int dest, xmmsv_t *broadcast) XMMS_PUBLIC;

/* method call */
xmmsc_result_t *xmmsc_sc_call (xmmsc_connection_t *c, int dest, xmmsv_t *method, xmmsv_t *pargs, xmmsv_t *nargs) XMMS_PUBLIC;

/* introspection */
xmmsc_result_t *xmmsc_sc_introspect_namespace (xmmsc_connection_t *c, int dest, xmmsv_t *nms) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_sc_introspect_method (xmmsc_connection_t *c, int dest, xmmsv_t *method) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_sc_introspect_broadcast (xmmsc_connection_t *c, int dest, xmmsv_t *broadcast) XMMS_PUBLIC;
xmmsc_result_t *xmmsc_sc_introspect_constant (xmmsc_connection_t *c, int dest, xmmsv_t *nms, const char *key) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_sc_introspect_docstring (xmmsc_connection_t *c, int dest, xmmsv_t *path) XMMS_PUBLIC;

/*
 * MACROS
 */

#define XMMS_CALLBACK_SET(conn,meth,callback,udata) \
	XMMS_CALLBACK_SET_FULL(conn,meth,callback,udata,NULL);

#define XMMS_CALLBACK_SET_FULL(conn,meth,callback,udata,free_func) {\
	xmmsc_result_t *res = meth (conn); \
	xmmsc_result_notifier_set_full (res, callback, udata, free_func);\
	xmmsc_result_unref (res);\
}

/*
 * RESULTS
 */

typedef int (*xmmsc_result_notifier_t) (xmmsv_t *val, void *user_data);

xmmsc_result_type_t xmmsc_result_get_class (xmmsc_result_t *res) XMMS_PUBLIC;
void xmmsc_result_disconnect (xmmsc_result_t *res) XMMS_PUBLIC;

xmmsc_result_t *xmmsc_result_ref (xmmsc_result_t *res) XMMS_PUBLIC;
void xmmsc_result_unref (xmmsc_result_t *res) XMMS_PUBLIC;

void xmmsc_result_notifier_set_default (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data) XMMS_PUBLIC;
void xmmsc_result_notifier_set_default_full (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data, xmmsc_user_data_free_func_t free_func) XMMS_PUBLIC;
void xmmsc_result_notifier_set_raw (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data) XMMS_PUBLIC;
void xmmsc_result_notifier_set_raw_full (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data, xmmsc_user_data_free_func_t free_func) XMMS_PUBLIC;
void xmmsc_result_notifier_set_c2c (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data) XMMS_PUBLIC;
void xmmsc_result_notifier_set_c2c_full (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data, xmmsc_user_data_free_func_t free_func) XMMS_PUBLIC;
void xmmsc_result_wait (xmmsc_result_t *res) XMMS_PUBLIC;

xmmsv_t *xmmsc_result_get_value (xmmsc_result_t *res) XMMS_PUBLIC;

/* Legacy aliases for convenience. */
#define xmmsc_result_iserror(res) xmmsv_is_error(xmmsc_result_get_value(res))

/* compability */
#define xmmsc_result_notifier_set xmmsc_result_notifier_set_default
#define xmmsc_result_notifier_set_full xmmsc_result_notifier_set_default_full

typedef xmmsv_coll_token_type_t xmmsc_coll_token_type_t;
typedef xmmsv_coll_token_t xmmsc_coll_token_t;
typedef xmmsv_coll_parse_tokens_f xmmsc_coll_parse_tokens_f;
typedef xmmsv_coll_parse_build_f xmmsc_coll_parse_build_f;

#define xmmsc_coll_parse xmmsv_coll_parse
#define xmmsc_coll_parse_custom xmmsv_coll_parse_custom
#define xmmsc_coll_default_parse_build xmmsv_coll_default_parse_build
#define xmmsc_coll_default_parse_tokens xmmsv_coll_default_parse_tokens

#ifdef __cplusplus
}
#endif

#endif
