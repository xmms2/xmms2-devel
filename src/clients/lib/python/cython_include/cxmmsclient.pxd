from cxmmsvalue cimport *

cdef extern from "xmmsc/xmmsc_idnumbers.h":
	ctypedef enum xmms_playback_status_t:
		XMMS_PLAYBACK_STATUS_STOP
		XMMS_PLAYBACK_STATUS_PLAY
		XMMS_PLAYBACK_STATUS_PAUSE

	ctypedef enum xmms_playlist_changed_action_t:
		XMMS_PLAYLIST_CHANGED_ADD
		XMMS_PLAYLIST_CHANGED_INSERT
		XMMS_PLAYLIST_CHANGED_SHUFFLE
		XMMS_PLAYLIST_CHANGED_REMOVE
		XMMS_PLAYLIST_CHANGED_CLEAR
		XMMS_PLAYLIST_CHANGED_MOVE
		XMMS_PLAYLIST_CHANGED_SORT
		XMMS_PLAYLIST_CHANGED_UPDATE

	ctypedef enum xmms_plugin_type_t:
		XMMS_PLUGIN_TYPE_ALL
		XMMS_PLUGIN_TYPE_XFORM
		XMMS_PLUGIN_TYPE_OUTPUT

	ctypedef enum xmmsc_medialib_entry_status_t:
		XMMS_MEDIALIB_ENTRY_STATUS_NEW
		XMMS_MEDIALIB_ENTRY_STATUS_OK
		XMMS_MEDIALIB_ENTRY_STATUS_RESOLVING
		XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE
		XMMS_MEDIALIB_ENTRY_STATUS_REHASH

	ctypedef enum xmms_collection_changed_action_t:
		XMMS_COLLECTION_CHANGED_ADD
		XMMS_COLLECTION_CHANGED_UPDATE
		XMMS_COLLECTION_CHANGED_RENAME
		XMMS_COLLECTION_CHANGED_REMOVE

	ctypedef enum xmms_playback_seek_mode_t:
		XMMS_PLAYBACK_SEEK_CUR
		XMMS_PLAYBACK_SEEK_SET

	ctypedef enum xmmsc_c2c_reply_policy_t:
		XMMS_C2C_REPLY_POLICY_NO_REPLY
		XMMS_C2C_REPLY_POLICY_SINGLE_REPLY
		XMMS_C2C_REPLY_POLICY_MULTI_REPLY

cdef extern from "xmmsc/xmmsc_util.h":
	cdef enum:
		XMMS_PATH_MAX

cdef extern from "xmmsc/xmmsv.h":
	ctypedef struct xmmsv_t

cdef extern from "xmmsc/xmmsc_visualization.h":
	cdef enum:
		XMMSC_VISUALIZATION_WINDOW_SIZE

cdef extern from "xmmsclient/xmmsclient.h":
	ctypedef struct xmmsc_connection_t
	ctypedef struct xmmsc_result_t

	ctypedef enum xmmsc_result_type_t:
		XMMSC_RESULT_CLASS_DEFAULT
		XMMSC_RESULT_CLASS_SIGNAL
		XMMSC_RESULT_CLASS_BROADCAST

	ctypedef void (*xmmsc_disconnect_func_t)           (void *user_data)
	ctypedef void (*xmmsc_user_data_free_func_t)       (void *user_data)
	ctypedef void (*xmmsc_io_need_out_callback_func_t) (int, void*)

	xmmsc_connection_t *xmmsc_init    (char *clientname)
	bint                xmmsc_connect (xmmsc_connection_t *c, char *clientname)
	xmmsc_connection_t *xmmsc_ref     (xmmsc_connection_t *c)
	void                xmmsc_unref   (xmmsc_connection_t *c)

	void xmmsc_lock_set (xmmsc_connection_t *c, void *lock, void (*lockfunc)(void *), void (*unlockfunc)(void *))
	void xmmsc_disconnect_callback_set (xmmsc_connection_t *c, xmmsc_disconnect_func_t callback, void *userdata)
	void xmmsc_disconnect_callback_set_full (xmmsc_connection_t *c, xmmsc_disconnect_func_t disconnect_func, void *userdata, xmmsc_user_data_free_func_t free_func)

	void xmmsc_io_need_out_callback_set      (xmmsc_connection_t *c, xmmsc_io_need_out_callback_func_t callback, void *userdata)
	void xmmsc_io_need_out_callback_set_full (xmmsc_connection_t *c, xmmsc_io_need_out_callback_func_t callback, void *userdata, xmmsc_user_data_free_func_t free_func)

	void xmmsc_io_disconnect (xmmsc_connection_t *c)
	bint xmmsc_io_want_out   (xmmsc_connection_t *c)
	bint xmmsc_io_out_handle (xmmsc_connection_t *c)
	bint xmmsc_io_in_handle  (xmmsc_connection_t *c)
	int  xmmsc_io_fd_get     (xmmsc_connection_t *c)

	char *xmmsc_get_last_error (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_quit (xmmsc_connection_t *c)

	char *xmmsc_userconfdir_get (char *buf, int len)

	# Playlists
	xmmsc_result_t *xmmsc_playlist_list    (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_create  (xmmsc_connection_t *c, char *playlist)
	xmmsc_result_t *xmmsc_playlist_shuffle (xmmsc_connection_t *c, char *playlist)

	xmmsc_result_t *xmmsc_playlist_add_full    (xmmsc_connection_t *c, char *playlist, char *, xmmsv_t *)
	xmmsc_result_t *xmmsc_playlist_add_url     (xmmsc_connection_t *c, char *playlist, char *)
	xmmsc_result_t *xmmsc_playlist_add_id      (xmmsc_connection_t *c, char *playlist, int)
	xmmsc_result_t *xmmsc_playlist_add_encoded (xmmsc_connection_t *c, char *, char *)
	xmmsc_result_t *xmmsc_playlist_add_idlist  (xmmsc_connection_t *c, char *playlist, xmmsv_t *coll)
	xmmsc_result_t *xmmsc_playlist_add_collection (xmmsc_connection_t *c, char *playlist, xmmsv_t *coll, xmmsv_t *order)

	xmmsc_result_t *xmmsc_playlist_remove_entry (xmmsc_connection_t *c, char *playlist, int)
	xmmsc_result_t *xmmsc_playlist_clear        (xmmsc_connection_t *c, char *playlist)
	xmmsc_result_t *xmmsc_playlist_remove       (xmmsc_connection_t *c, char *playlist)
	xmmsc_result_t *xmmsc_playlist_list_entries (xmmsc_connection_t *c, char *playlist)
	xmmsc_result_t *xmmsc_playlist_sort         (xmmsc_connection_t *c, char *playlist, xmmsv_t *properties)
	xmmsc_result_t *xmmsc_playlist_set_next     (xmmsc_connection_t *c, int pos)
	xmmsc_result_t *xmmsc_playlist_set_next_rel (xmmsc_connection_t *c, int rpos)
	xmmsc_result_t *xmmsc_playlist_move_entry   (xmmsc_connection_t *c, char *playlist, int id, int movement)
	xmmsc_result_t *xmmsc_playlist_current_pos  (xmmsc_connection_t *c, char *playlist)
	xmmsc_result_t *xmmsc_playlist_current_active (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_playlist_insert_full (xmmsc_connection_t *c, char *playlist, int pos, char *url, xmmsv_t *args)
	xmmsc_result_t *xmmsc_playlist_insert_url  (xmmsc_connection_t *c, char *playlist, int pos, char *url)
	xmmsc_result_t *xmmsc_playlist_insert_id   (xmmsc_connection_t *c, char *playlist, int pos, int)
	xmmsc_result_t *xmmsc_playlist_insert_encoded (xmmsc_connection_t *c, char *playlist, int pos, char *url)
	xmmsc_result_t *xmmsc_playlist_insert_collection (xmmsc_connection_t *c, char *playlist, int pos, xmmsv_t *coll, xmmsv_t *order)

	xmmsc_result_t *xmmsc_playlist_load (xmmsc_connection_t *c, char *playlist)
	xmmsc_result_t *xmmsc_playlist_radd (xmmsc_connection_t *c, char *playlist, char *path)
	xmmsc_result_t *xmmsc_playlist_radd_encoded (xmmsc_connection_t *c, char *playlist, char *path)
	xmmsc_result_t *xmmsc_playlist_rinsert (xmmsc_connection_t *c, char *playlist, int pos, char *url)
	xmmsc_result_t *xmmsc_playlist_rinsert_encoded (xmmsc_connection_t *c, char *playlist, int pos, char *url)

	xmmsc_result_t *xmmsc_broadcast_playlist_changed     (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playlist_current_pos (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playlist_loaded      (xmmsc_connection_t *c)

	# Playback
	xmmsc_result_t *xmmsc_playback_stop         (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_tickle       (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_start        (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_pause        (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_current_id   (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_seek_ms      (xmmsc_connection_t *c, int milliseconds, xmms_playback_seek_mode_t whence)
	xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, int samples, xmms_playback_seek_mode_t whence)
	xmmsc_result_t *xmmsc_playback_playtime     (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_status       (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_volume_set   (xmmsc_connection_t *c, char *channel, int volume)
	xmmsc_result_t *xmmsc_playback_volume_get   (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playback_volume_changed (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_status         (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_current_id     (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_signal_playback_playtime(xmmsc_connection_t *c)

	# Config
	xmmsc_result_t *xmmsc_config_set_value      (xmmsc_connection_t *c, char *key, char *val)
	xmmsc_result_t *xmmsc_config_list_values    (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_config_get_value      (xmmsc_connection_t *c, char *key)
	xmmsc_result_t *xmmsc_config_register_value (xmmsc_connection_t *c, char *valuename, char *defaultvalue)

	xmmsc_result_t *xmmsc_broadcast_config_value_changed (xmmsc_connection_t *c)

	# Stats
	xmmsc_result_t *xmmsc_main_list_plugins (xmmsc_connection_t *c, xmms_plugin_type_t type)
	xmmsc_result_t *xmmsc_main_stats        (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_mediainfo_reader_status (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_signal_mediainfo_reader_unindexed (xmmsc_connection_t *c)

	# Visualization
	xmmsc_result_t *xmmsc_visualization_version      (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_visualization_init         (xmmsc_connection_t *c)
	int             xmmsc_visualization_init_handle  (xmmsc_result_t *res)
	xmmsc_result_t *xmmsc_visualization_start        (xmmsc_connection_t *c, int vv)
	void            xmmsc_visualization_start_handle (xmmsc_connection_t *c, xmmsc_result_t *res)
	bint            xmmsc_visualization_started      (xmmsc_connection_t *c, int vv)
	bint            xmmsc_visualization_errored      (xmmsc_connection_t *c, int vv)

	xmmsc_result_t *xmmsc_visualization_property_set   (xmmsc_connection_t *c, int v, char *key, char *value)
	xmmsc_result_t *xmmsc_visualization_properties_set (xmmsc_connection_t *c, int v, xmmsv_t *props)

	int  xmmsc_visualization_chunk_get (xmmsc_connection_t *c, int vv, short *buffer, int drawtime, unsigned int blocking)
	void xmmsc_visualization_shutdown (xmmsc_connection_t *c, int v)

	# Medialib
	xmmsc_result_t *xmmsc_medialib_add_entry           (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_medialib_add_entry_full      (xmmsc_connection_t *c, char *url, xmmsv_t *args)
	xmmsc_result_t *xmmsc_medialib_add_entry_encoded   (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_medialib_get_info            (xmmsc_connection_t *c, int id)
	xmmsc_result_t *xmmsc_medialib_import_path         (xmmsc_connection_t *c, char *path)
	xmmsc_result_t *xmmsc_medialib_import_path_encoded (xmmsc_connection_t *c, char *path)
	xmmsc_result_t *xmmsc_medialib_rehash              (xmmsc_connection_t *c, unsigned int)
	xmmsc_result_t *xmmsc_medialib_get_id              (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_medialib_get_id_encoded      (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_medialib_remove_entry        (xmmsc_connection_t *conn, int id)
	xmmsc_result_t *xmmsc_medialib_move_entry          (xmmsc_connection_t *conn, int id, char *url)

	xmmsc_result_t *xmmsc_medialib_entry_property_set_int (xmmsc_connection_t *c, int id, char *key, int value)
	xmmsc_result_t *xmmsc_medialib_entry_property_set_int_with_source (xmmsc_connection_t *c, int id, char *source, char *key, int value)

	xmmsc_result_t *xmmsc_medialib_entry_property_set_str (xmmsc_connection_t *c, int id, char *key, char *value)
	xmmsc_result_t *xmmsc_medialib_entry_property_set_str_with_source (xmmsc_connection_t *c, int id, char *source, char *key, char *value)

	xmmsc_result_t *xmmsc_medialib_entry_property_remove (xmmsc_connection_t *c, int id, char *key)
	xmmsc_result_t *xmmsc_medialib_entry_property_remove_with_source (xmmsc_connection_t *c, int id, char *source, char *key)

	xmmsc_result_t *xmmsc_xform_media_browse         (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_xform_media_browse_encoded (xmmsc_connection_t *c, char *url)

	xmmsc_result_t *xmmsc_bindata_add      (xmmsc_connection_t *c, unsigned char *data, int len)
	xmmsc_result_t *xmmsc_bindata_retrieve (xmmsc_connection_t *c, char *hash)
	xmmsc_result_t *xmmsc_bindata_remove   (xmmsc_connection_t *c, char *hash)
	xmmsc_result_t *xmmsc_bindata_list     (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_medialib_entry_added   (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_medialib_entry_updated (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_medialib_entry_removed (xmmsc_connection_t *c)

	# Collections
	ctypedef char *xmmsv_coll_namespace_t # Need to redeclare it
	xmmsc_result_t *xmmsc_coll_get    (xmmsc_connection_t *c, char *collname, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_list   (xmmsc_connection_t *c, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_save   (xmmsc_connection_t *c, xmmsv_t *coll, char* name, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_remove (xmmsc_connection_t *c, char* name, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_find   (xmmsc_connection_t *c, int mediaid, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_rename (xmmsc_connection_t *c, char* from_name, char* to_name, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_idlist_from_playlist_file (xmmsc_connection_t *c, char *path)
	xmmsc_result_t *xmmsc_coll_sync   (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_coll_query       (xmmsc_connection_t *c, xmmsv_t *coll, xmmsv_t *fetch)
	xmmsc_result_t *xmmsc_coll_query_ids   (xmmsc_connection_t *c, xmmsv_t *coll, xmmsv_t *order, unsigned int limit_start, unsigned int limit_len)
	xmmsc_result_t *xmmsc_coll_query_infos (xmmsc_connection_t *c, xmmsv_t *coll, xmmsv_t *order, unsigned int limit_start, unsigned int limit_len,  xmmsv_t *fetch, xmmsv_t *group)

	xmmsc_result_t *xmmsc_broadcast_collection_changed (xmmsc_connection_t *c)


	# C2C
	xmmsc_result_t *xmmsc_c2c_send  (xmmsc_connection_t *c, int dest, xmmsc_c2c_reply_policy_t reply_policy, xmmsv_t *payload)
	xmmsc_result_t *xmmsc_c2c_reply (xmmsc_connection_t *c, int msgid, xmmsc_c2c_reply_policy_t reply_policy, xmmsv_t *payload)
	int xmmsc_c2c_get_own_id (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_c2c_get_connected_clients (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_c2c_ready (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_c2c_get_ready_clients (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_c2c_message (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_c2c_ready (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_c2c_client_connected (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_c2c_client_disconnected (xmmsc_connection_t *c)


	# Service
	ctypedef enum xmmsc_sc_command_t:
		XMMSC_SC_CALL
		XMMSC_SC_BROADCAST_SUBSCRIBE
		XMMSC_SC_INTROSPECT

	# XXX Requires an explicit cast to <char *> when used
	enum:
		XMMSC_SC_CMD_KEY
		XMMSC_SC_ARGS_KEY
		XMMSC_SC_CALL_METHOD_KEY
		XMMSC_SC_CALL_PARGS_KEY
		XMMSC_SC_CALL_NARGS_KEY
		XMMSC_SC_INSTROSPECT_PATH_KEY
		XMMSC_SC_INSTROSPECT_TYPE_KEY
		XMMSC_SC_INSTROSPECT_KEYFILTER_KEY

	ctypedef struct xmmsc_sc_namespace_t
	ctypedef xmmsv_t *(*xmmsc_sc_method_t) (xmmsv_t *pargs, xmmsv_t *nargs, void *udata)

	xmmsc_sc_namespace_t *xmmsc_sc_init (xmmsc_connection_t *c)

	xmmsc_sc_namespace_t *xmmsc_sc_namespace_root (xmmsc_connection_t *c)
	xmmsc_sc_namespace_t *xmmsc_sc_namespace_lookup (xmmsc_connection_t *c, xmmsv_t *nms)
	xmmsc_sc_namespace_t *xmmsc_sc_namespace_new (xmmsc_sc_namespace_t *parent, const_char *name, const_char *docstring)
	xmmsc_sc_namespace_t *xmmsc_sc_namespace_get (xmmsc_sc_namespace_t *parent, const_char *name)
	bint xmmsc_sc_namespace_add_constant (xmmsc_sc_namespace_t *nms, const_char *key, xmmsv_t *value)
	void xmmsc_sc_namespace_remove_constant (xmmsc_sc_namespace_t *nms, const char *key)

	bint xmmsc_sc_namespace_add_method (xmmsc_sc_namespace_t *nmd, xmmsc_sc_method_t method, const_char *name, const_char *docstring, xmmsv_t *pargs, xmmsv_t *nargs, bint va_pos, bint va_named, void *udata)
	bint xmmsc_sc_namespace_add_method_noarg (xmmsc_sc_namespace_t *nms, xmmsc_sc_method_t method, const_char *name, const_char *docstring, void * udata)

	bint xmmsc_sc_namespace_add_broadcast (xmmsc_sc_namespace_t *nms, const_char *name, const_char *docstring)

	void xmmsc_sc_namespace_remove (xmmsc_sc_namespace_t *nms, xmmsv_t *path)
	bint xmmsc_sc_broadcast_emit (xmmsc_connection_t *c, xmmsv_t *broadcast, xmmsv_t *value)
	xmmsc_result_t *xmmsc_sc_broadcast_subscribe (xmmsc_connection_t *c, int dest, xmmsv_t *broadcast)

	xmmsc_result_t *xmmsc_sc_call (xmmsc_connection_t *c, int dest, xmmsv_t *method, xmmsv_t *pargs, xmmsv_t *nargs)

	xmmsc_result_t *xmmsc_sc_introspect_namespace (xmmsc_connection_t *c, int dest, xmmsv_t *nms)
	xmmsc_result_t *xmmsc_sc_introspect_method (xmmsc_connection_t *c, int dest, xmmsv_t *method)
	xmmsc_result_t *xmmsc_sc_introspect_broadcast (xmmsc_connection_t *c, int dest, xmmsv_t *broadcast)
	xmmsc_result_t *xmmsc_sc_introspect_constant (xmmsc_connection_t *c, int dest, xmmsv_t *nms, const_char *key)
	xmmsc_result_t *xmmsc_sc_introspect_docstring (xmmsc_connection_t *c, int dest, xmmsv_t *path)


	# Results
	ctypedef bint (*xmmsc_result_notifier_t) (xmmsv_t *val, void *user_data)

	xmmsc_result_type_t xmmsc_result_get_class  (xmmsc_result_t *res)
	void                xmmsc_result_disconnect (xmmsc_result_t *res)

	xmmsc_result_t *xmmsc_result_ref   (xmmsc_result_t *res)
	void            xmmsc_result_unref (xmmsc_result_t *res)

	void xmmsc_result_notifier_set      (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data)
	void xmmsc_result_notifier_set_full (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data, xmmsc_user_data_free_func_t free_func)
	void xmmsc_result_wait              (xmmsc_result_t *res)

	xmmsv_t *xmmsc_result_get_value (xmmsc_result_t *res)
