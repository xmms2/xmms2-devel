"""
Python bindings for XMMS2.
"""

cdef extern from "Python.h":
	object PyUnicode_DecodeUTF8(char *unicode, int size, char *errors)
	object PyUnicode_AsUTF8String(object o)
	object PyString_FromStringAndSize(char *, int)
	void Py_INCREF(object)
	void Py_DECREF(object)

cdef extern from "xmms_pyrex_hacks.h":
	# pyrex doesn't know about const, so we do some tricks to get
	# rid of compiler warnings
	ctypedef struct xmms_pyrex_constcharp_t:
		pass
	ctypedef struct xmms_pyrex_constcharpp_t:
		pass
	ctypedef struct xmms_pyrex_constucharpp_t:
		pass

cdef extern from "xmmsc/xmmsc_idnumbers.h":
	ctypedef enum xmmsv_coll_type_t:
		XMMS_COLLECTION_TYPE_REFERENCE
		XMMS_COLLECTION_TYPE_UNION
		XMMS_COLLECTION_TYPE_INTERSECTION
		XMMS_COLLECTION_TYPE_COMPLEMENT
		XMMS_COLLECTION_TYPE_HAS
		XMMS_COLLECTION_TYPE_EQUALS
		XMMS_COLLECTION_TYPE_MATCH
		XMMS_COLLECTION_TYPE_SMALLER
		XMMS_COLLECTION_TYPE_GREATER
		XMMS_COLLECTION_TYPE_IDLIST
		XMMS_COLLECTION_TYPE_QUEUE
		XMMS_COLLECTION_TYPE_PARTYSHUFFLE

	ctypedef enum xmms_playback_status_t:
		XMMS_PLAYBACK_STATUS_STOP,
		XMMS_PLAYBACK_STATUS_PLAY,
		XMMS_PLAYBACK_STATUS_PAUSE

	ctypedef enum xmms_playlist_changed_actions_t:
		XMMS_PLAYLIST_CHANGED_ADD,
		XMMS_PLAYLIST_CHANGED_INSERT,
		XMMS_PLAYLIST_CHANGED_SHUFFLE,
		XMMS_PLAYLIST_CHANGED_REMOVE,
		XMMS_PLAYLIST_CHANGED_CLEAR,
		XMMS_PLAYLIST_CHANGED_MOVE,
		XMMS_PLAYLIST_CHANGED_SORT,
		XMMS_PLAYLIST_CHANGED_UPDATE

	ctypedef enum xmms_plugin_type_t:
		XMMS_PLUGIN_TYPE_ALL,
		XMMS_PLUGIN_TYPE_XFORM,
		XMMS_PLUGIN_TYPE_OUTPUT,

	ctypedef enum xmmsc_medialib_entry_status_t:
		XMMS_MEDIALIB_ENTRY_STATUS_NEW,
		XMMS_MEDIALIB_ENTRY_STATUS_OK,
		XMMS_MEDIALIB_ENTRY_STATUS_RESOLVING,
		XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE,
		XMMS_MEDIALIB_ENTRY_STATUS_REHASH

	ctypedef enum xmmsc_collection_changed_actions_t:
		XMMS_COLLECTION_CHANGED_ADD,
		XMMS_COLLECTION_CHANGED_UPDATE,
		XMMS_COLLECTION_CHANGED_RENAME,
		XMMS_COLLECTION_CHANGED_REMOVE

cdef extern from "xmmsc/xmmsv.h":
	ctypedef enum xmmsv_type_t:
		XMMSV_TYPE_NONE,
		XMMSV_TYPE_ERROR,
		XMMSV_TYPE_INT32,
		XMMSV_TYPE_STRING,
		XMMSV_TYPE_COLL,
		XMMSV_TYPE_BIN,
		XMMSV_TYPE_LIST,
		XMMSV_TYPE_DICT,

	ctypedef struct xmmsv_t
	ctypedef struct xmmsv_coll_t

	xmmsv_t *xmmsv_new_none   ()
	xmmsv_t *xmmsv_new_error  (char *errstr)
	xmmsv_t *xmmsv_new_int    (int i)
	xmmsv_t *xmmsv_new_uint   (unsigned int u)
	xmmsv_t *xmmsv_new_string (char *s)
	xmmsv_t *xmmsv_new_coll   (xmmsv_coll_t *coll)
	xmmsv_t *xmmsv_new_bin    (unsigned char *data, unsigned int len)

	xmmsv_t *xmmsv_new_list ()
	xmmsv_t *xmmsv_new_dict ()

	xmmsv_t *xmmsv_ref (xmmsv_t *val)
	void xmmsv_unref   (xmmsv_t *value)

	xmmsv_type_t xmmsv_get_type(xmmsv_t *res)

	int  xmmsv_is_error (xmmsv_t *res)
	int  xmmsv_is_list  (xmmsv_t *res)
	int  xmmsv_is_dict  (xmmsv_t *val)

	int  xmmsv_get_error      (xmmsv_t *value, xmms_pyrex_constcharpp_t r)
	int  xmmsv_get_int        (xmmsv_t *res, int *r)
	int  xmmsv_get_string     (xmmsv_t *res, xmms_pyrex_constcharpp_t r)
	int  xmmsv_get_coll (xmmsv_t *value, xmmsv_coll_t **coll)
	int  xmmsv_get_bin        (xmmsv_t *res, xmms_pyrex_constucharpp_t r, unsigned int *rlen)

	ctypedef void (*xmmsv_list_foreach_func) (xmmsv_t *value, void *user_data)

	int  xmmsv_list_get (xmmsv_t *listv, int pos, xmmsv_t **val)
	int  xmmsv_list_append (xmmsv_t *listv, xmmsv_t *val)
	int  xmmsv_list_insert (xmmsv_t *listv, int pos, xmmsv_t *val)
	int  xmmsv_list_remove (xmmsv_t *listv, int pos)
	int  xmmsv_list_clear (xmmsv_t *listv)
	int  xmmsv_list_foreach (xmmsv_t *listv, xmmsv_list_foreach_func func, void* user_data)
	int  xmmsv_list_get_size (xmmsv_t *listv)

	ctypedef struct xmmsv_list_iter_t

	int  xmmsv_get_list_iter    (xmmsv_t *val, xmmsv_list_iter_t **it)

	int  xmmsv_list_iter_entry  (xmmsv_list_iter_t *it, xmmsv_t **val)
	int  xmmsv_list_iter_valid  (xmmsv_list_iter_t *it)
	void xmmsv_list_iter_first  (xmmsv_list_iter_t *it)
	void xmmsv_list_iter_next   (xmmsv_list_iter_t *it)
	int  xmmsv_list_iter_seek   (xmmsv_list_iter_t *it, int pos)

	int  xmmsv_list_iter_insert (xmmsv_list_iter_t *it, xmmsv_t *val)
	int  xmmsv_list_iter_remove (xmmsv_list_iter_t *it)

	void xmmsv_list_iter_explicit_destroy (xmmsv_list_iter_t *it)

	ctypedef void (*xmmsv_dict_foreach_func) (char *key, xmmsv_t *value, void *user_data)

	int  xmmsv_dict_get     (xmmsv_t *dictv, char *key, xmmsv_t **val)
	int  xmmsv_dict_set     (xmmsv_t *dictv, char *key, xmmsv_t *val)
	int  xmmsv_dict_remove  (xmmsv_t *dictv, char *key)
	int  xmmsv_dict_clear   (xmmsv_t *dictv)
	int  xmmsv_dict_foreach (xmmsv_t *dictv, xmmsv_dict_foreach_func func, void *user_data)

	ctypedef struct xmmsv_dict_iter_t

	int  xmmsv_get_dict_iter   (xmmsv_t *val, xmmsv_dict_iter_t **it)

	int  xmmsv_dict_iter_pair  (xmmsv_dict_iter_t *it, xmms_pyrex_constcharp_t *key, xmmsv_t **val)
	int  xmmsv_dict_iter_pair_string  (xmmsv_dict_iter_t *it, xmms_pyrex_constcharpp_t key, xmms_pyrex_constcharpp_t val)
	int  xmmsv_dict_iter_valid (xmmsv_dict_iter_t *it)
	void xmmsv_dict_iter_first (xmmsv_dict_iter_t *it)
	void xmmsv_dict_iter_next  (xmmsv_dict_iter_t *it)
	int  xmmsv_dict_iter_find  (xmmsv_dict_iter_t *it, char *key)

	int  xmmsv_dict_iter_set    (xmmsv_dict_iter_t *it, xmmsv_t *val)
	int  xmmsv_dict_iter_remove (xmmsv_dict_iter_t *it)

	void xmmsv_dict_iter_explicit_destroy (xmmsv_dict_iter_t *it)


cdef extern from "xmms_configuration.h":
	cdef enum:
		XMMS_PATH_MAX

# The following constants are meant for interpreting the return value of
# XMMS.playback_status ()
PLAYBACK_STATUS_STOP = XMMS_PLAYBACK_STATUS_STOP
PLAYBACK_STATUS_PLAY = XMMS_PLAYBACK_STATUS_PLAY
PLAYBACK_STATUS_PAUSE = XMMS_PLAYBACK_STATUS_PAUSE

PLAYLIST_CHANGED_ADD = XMMS_PLAYLIST_CHANGED_ADD
PLAYLIST_CHANGED_INSERT = XMMS_PLAYLIST_CHANGED_INSERT
PLAYLIST_CHANGED_SHUFFLE = XMMS_PLAYLIST_CHANGED_SHUFFLE
PLAYLIST_CHANGED_REMOVE = XMMS_PLAYLIST_CHANGED_REMOVE
PLAYLIST_CHANGED_CLEAR = XMMS_PLAYLIST_CHANGED_CLEAR
PLAYLIST_CHANGED_MOVE = XMMS_PLAYLIST_CHANGED_MOVE
PLAYLIST_CHANGED_SORT = XMMS_PLAYLIST_CHANGED_SORT
PLAYLIST_CHANGED_UPDATE = XMMS_PLAYLIST_CHANGED_UPDATE

PLUGIN_TYPE_ALL = XMMS_PLUGIN_TYPE_ALL
PLUGIN_TYPE_XFORM = XMMS_PLUGIN_TYPE_XFORM
PLUGIN_TYPE_OUTPUT = XMMS_PLUGIN_TYPE_OUTPUT

MEDIALIB_ENTRY_STATUS_NEW = XMMS_MEDIALIB_ENTRY_STATUS_NEW
MEDIALIB_ENTRY_STATUS_OK = XMMS_MEDIALIB_ENTRY_STATUS_OK
MEDIALIB_ENTRY_STATUS_RESOLVING = XMMS_MEDIALIB_ENTRY_STATUS_RESOLVING
MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE = XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE
MEDIALIB_ENTRY_STATUS_REHASH = XMMS_MEDIALIB_ENTRY_STATUS_REHASH

COLLECTION_CHANGED_ADD = XMMS_COLLECTION_CHANGED_ADD
COLLECTION_CHANGED_UPDATE = XMMS_COLLECTION_CHANGED_UPDATE
COLLECTION_CHANGED_RENAME = XMMS_COLLECTION_CHANGED_RENAME
COLLECTION_CHANGED_REMOVE = XMMS_COLLECTION_CHANGED_REMOVE

VALUE_TYPE_NONE = XMMSV_TYPE_NONE
VALUE_TYPE_ERROR = XMMSV_TYPE_ERROR
VALUE_TYPE_INT32 = XMMSV_TYPE_INT32
VALUE_TYPE_STRING = XMMSV_TYPE_STRING
VALUE_TYPE_COLL = XMMSV_TYPE_COLL
VALUE_TYPE_BIN = XMMSV_TYPE_BIN
VALUE_TYPE_LIST = XMMSV_TYPE_LIST
VALUE_TYPE_DICT = XMMSV_TYPE_DICT

cdef extern from "xmmsclient/xmmsclient.h":
	ctypedef enum xmmsc_result_type_t:
		XMMSC_RESULT_CLASS_DEFAULT,
		XMMSC_RESULT_CLASS_SIGNAL,
		XMMSC_RESULT_CLASS_BROADCAST

	ctypedef struct xmmsc_connection_t:
		pass
	ctypedef struct xmmsc_result_t

	ctypedef int  (*xmmsc_result_notifier_t) (xmmsv_t *val, void *user_data)
	ctypedef void (*xmmsc_user_data_free_func_t) (void *user_data)
	ctypedef void (*xmmsc_io_need_out_callback_func_t) (int, void*)
	ctypedef void (*xmmsc_disconnect_func_t) (void *user_data)

	xmmsc_result_t *xmmsc_result_ref(xmmsc_result_t *res)
	void xmmsc_result_unref(xmmsc_result_t *res)
	void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data)
	void xmmsc_result_notifier_set_full(xmmsc_result_t *res, xmmsc_result_notifier_t func, void *user_data, xmmsc_user_data_free_func_t free_func)
	void xmmsc_result_wait(xmmsc_result_t *res)
	xmmsv_t *xmmsc_result_get_value(xmmsc_result_t *res)

	xmmsc_connection_t *xmmsc_init(char *clientname)

	void xmmsc_disconnect_callback_set(xmmsc_connection_t *c, xmmsc_disconnect_func_t callback, void *userdata)
	void xmmsc_disconnect_callback_set_full(xmmsc_connection_t *c, xmmsc_disconnect_func_t disconnect_func, void *userdata, xmmsc_user_data_free_func_t free_func)
	int xmmsc_connect(xmmsc_connection_t *c, char *p)
	void xmmsc_unref(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_plugin_list (xmmsc_connection_t *c, unsigned int type)

	int xmmsv_coll_parse (char *pattern, xmmsv_coll_t **coll)

	xmmsc_result_t *xmmsc_playlist_list(xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_playlist_shuffle(xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_playlist_add_args(xmmsc_connection_t *, char *playlist, char *, int, char **)
	xmmsc_result_t *xmmsc_playlist_add_url(xmmsc_connection_t *, char *playlist, char *)
	xmmsc_result_t *xmmsc_playlist_add_id(xmmsc_connection_t *, char *playlist, unsigned int)
	xmmsc_result_t *xmmsc_playlist_add_encoded(xmmsc_connection_t *, char *, char *)
	xmmsc_result_t *xmmsc_playlist_add_collection(xmmsc_connection_t *, char *playlist, xmmsv_coll_t *coll, xmmsv_t *order)
	xmmsc_result_t *xmmsc_playlist_remove_entry(xmmsc_connection_t *, char *playlist, unsigned int)
	xmmsc_result_t *xmmsc_playlist_clear(xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_playlist_remove(xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_playlist_list_entries(xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_playlist_sort(xmmsc_connection_t *, char *playlist, xmmsv_t *properties)
	xmmsc_result_t *xmmsc_playlist_set_next(xmmsc_connection_t *, int pos)
	xmmsc_result_t *xmmsc_playlist_set_next_rel(xmmsc_connection_t *, int)
	xmmsc_result_t *xmmsc_playlist_move_entry(xmmsc_connection_t *, char *playlist, unsigned int id, int movement)
	xmmsc_result_t *xmmsc_playlist_current_pos(xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_playlist_current_active(xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_playlist_insert_args(xmmsc_connection_t *, char *playlist, int pos, char *url, int numargs, char **args)
	xmmsc_result_t *xmmsc_playlist_insert_url(xmmsc_connection_t *, char *playlist, int pos, char *)
	xmmsc_result_t *xmmsc_playlist_insert_id(xmmsc_connection_t *, char *playlist, int pos, unsigned int)
	xmmsc_result_t *xmmsc_playlist_insert_encoded(xmmsc_connection_t *, char *, int pos, char *)
	xmmsc_result_t *xmmsc_playlist_insert_collection(xmmsc_connection_t *, char *playlist, int pos, xmmsv_coll_t *coll, xmmsv_t *order)
	xmmsc_result_t *xmmsc_playlist_rinsert (xmmsc_connection_t *c, char *playlist, int pos, char *url)
	xmmsc_result_t *xmmsc_playlist_rinsert_encoded (xmmsc_connection_t *c, char *playlist, int pos, char *url)
	xmmsc_result_t *xmmsc_playlist_radd(xmmsc_connection_t *c, char *, char *path)
	xmmsc_result_t *xmmsc_playlist_radd_encoded(xmmsc_connection_t *c, char *, char *path)

	xmmsc_result_t *xmmsc_playlist_load(xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_playlist_move(xmmsc_connection_t *c, unsigned int id, int movement)
	xmmsc_result_t *xmmsc_playlist_create(xmmsc_connection_t *c, char *playlist)

	xmmsc_result_t *xmmsc_broadcast_playlist_changed(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playlist_current_pos(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playlist_loaded(xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_playback_stop(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_tickle(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_start(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_pause(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_current_id(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_seek_ms_abs(xmmsc_connection_t *c, unsigned int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_ms_rel(xmmsc_connection_t *c, int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_samples_abs(xmmsc_connection_t *c, unsigned int samples)
	xmmsc_result_t *xmmsc_playback_seek_samples_rel(xmmsc_connection_t *c, int samples)
	xmmsc_result_t *xmmsc_playback_playtime(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_status(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playback_status(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_current_id(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_signal_playback_playtime(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_playback_volume_set (xmmsc_connection_t *c, char *channel, unsigned int volume)
	xmmsc_result_t *xmmsc_playback_volume_get (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_volume_changed (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_config_set_value(xmmsc_connection_t *c, char *key, char *val)
	xmmsc_result_t *xmmsc_config_list_values(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_config_get_value(xmmsc_connection_t *c, char *key)
	xmmsc_result_t *xmmsc_config_register_value(xmmsc_connection_t *c, char *valuename, char *defaultvalue)

	xmmsc_result_t *xmmsc_broadcast_config_value_changed(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_medialib_playlist_load(xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_add_entry(xmmsc_connection_t *conn, char *url)
	xmmsc_result_t *xmmsc_medialib_remove_entry(xmmsc_connection_t *conn, unsigned int id)
	xmmsc_result_t *xmmsc_medialib_add_entry_encoded(xmmsc_connection_t *conn, char *url)
	xmmsc_result_t *xmmsc_medialib_get_info(xmmsc_connection_t *, unsigned int id)
	xmmsc_result_t *xmmsc_medialib_path_import (xmmsc_connection_t *c, char *path)
	xmmsc_result_t *xmmsc_medialib_path_import_encoded (xmmsc_connection_t *c, char *path)
	xmmsc_result_t *xmmsc_medialib_rehash(xmmsc_connection_t *c, unsigned int)
	xmmsc_result_t *xmmsc_medialib_get_id (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_medialib_entry_property_set_int (xmmsc_connection_t *c, unsigned int id, char *key, int value)
	xmmsc_result_t *xmmsc_medialib_entry_property_set_str (xmmsc_connection_t *c, unsigned int id, char *key, char *value)
	xmmsc_result_t *xmmsc_medialib_entry_property_set_int_with_source (xmmsc_connection_t *c, unsigned int id, char *source, char *key, int value)
	xmmsc_result_t *xmmsc_medialib_entry_property_set_str_with_source (xmmsc_connection_t *c, unsigned int id, char *source, char *key, char *value)
	xmmsc_result_t *xmmsc_medialib_entry_property_remove (xmmsc_connection_t *c, unsigned int id, char *key)
	xmmsc_result_t *xmmsc_medialib_entry_property_remove_with_source (xmmsc_connection_t *c, unsigned int id, char *source, char *key)

	xmmsc_result_t *xmmsc_xform_media_browse (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_xform_media_browse_encoded (xmmsc_connection_t *c, char *url)
	xmmsc_result_t *xmmsc_bindata_add (xmmsc_connection_t *c, unsigned char *, int len)
	xmmsc_result_t *xmmsc_bindata_retrieve (xmmsc_connection_t *c, char *hash)
	xmmsc_result_t *xmmsc_bindata_remove (xmmsc_connection_t *c, char *hash)
	xmmsc_result_t *xmmsc_bindata_list (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_medialib_entry_added(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_medialib_entry_changed(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_collection_changed(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_mediainfo_reader_status (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_signal_mediainfo_reader_unindexed (xmmsc_connection_t *c)

	char *xmmsc_userconfdir_get (char *buf, int len)

	void xmmsc_io_need_out_callback_set_full(xmmsc_connection_t *c, xmmsc_io_need_out_callback_func_t callback, void *userdata, xmmsc_user_data_free_func_t free_func)
	void xmmsc_io_disconnect(xmmsc_connection_t *c)
	int xmmsc_io_want_out(xmmsc_connection_t *c)
	int xmmsc_io_out_handle(xmmsc_connection_t *c)
	int xmmsc_io_in_handle(xmmsc_connection_t *c)
	int xmmsc_io_fd_get(xmmsc_connection_t *c)


	ctypedef char *xmmsv_coll_namespace_t
	char *XMMS_COLLECTION_NS_COLLECTIONS
	char *XMMS_COLLECTION_NS_PLAYLISTS
	char *XMMS_COLLECTION_NS_ALL

	xmmsc_result_t *xmmsc_coll_get (xmmsc_connection_t *conn, char *collname, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_list (xmmsc_connection_t *conn, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_save (xmmsc_connection_t *conn, xmmsv_coll_t *coll, char* name, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_remove (xmmsc_connection_t *conn, char* name, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_find (xmmsc_connection_t *conn, unsigned int mediaid, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_rename (xmmsc_connection_t *conn, char* from_name, char* to_name, xmmsv_coll_namespace_t ns)
	xmmsc_result_t *xmmsc_coll_idlist_from_playlist_file (xmmsc_connection_t *conn, char *path)

	xmmsc_result_t *xmmsc_coll_query_ids (xmmsc_connection_t *conn, xmmsv_coll_t *coll, xmmsv_t *order, unsigned int limit_start, unsigned int limit_len)
	xmmsc_result_t *xmmsc_coll_query_infos (xmmsc_connection_t *conn, xmmsv_coll_t *coll, xmmsv_t *order, unsigned int limit_start, unsigned int limit_len,  xmmsv_t *fetch, xmmsv_t *order)


	xmmsv_coll_t *xmmsv_coll_new (xmmsv_coll_type_t)
	void xmmsv_coll_ref (xmmsv_coll_t *)
	void xmmsv_coll_unref (xmmsv_coll_t *)
	xmmsv_coll_t *xmmsv_coll_universe ()
	xmmsv_coll_type_t xmmsv_coll_get_type (xmmsv_coll_t *coll)

	unsigned int *xmmsv_coll_get_idlist (xmmsv_coll_t *coll)
	int xmmsv_coll_idlist_append (xmmsv_coll_t *coll, unsigned int id)
	int xmmsv_coll_idlist_insert (xmmsv_coll_t *coll, unsigned int index, unsigned int id)
	int xmmsv_coll_idlist_move (xmmsv_coll_t *coll, unsigned int index, unsigned int newindex)
	int xmmsv_coll_idlist_remove (xmmsv_coll_t *coll, unsigned int index)
	int xmmsv_coll_idlist_clear (xmmsv_coll_t *coll)
	int xmmsv_coll_idlist_get_index (xmmsv_coll_t *coll, unsigned int index, unsigned int *val)
	int xmmsv_coll_idlist_set_index (xmmsv_coll_t *coll, unsigned int index, unsigned int val)
	int xmmsv_coll_idlist_get_size (xmmsv_coll_t *coll)


	void xmmsv_coll_add_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)
	void xmmsv_coll_remove_operand (xmmsv_coll_t *coll, xmmsv_coll_t *op)

	xmmsv_t *xmmsv_coll_operands_get (xmmsv_coll_t *coll)

	void xmmsv_coll_attribute_set (xmmsv_coll_t *coll, char *key, char *value)
	int xmmsv_coll_attribute_remove (xmmsv_coll_t *coll, char *key)
	int xmmsv_coll_attribute_get (xmmsv_coll_t *coll, char *key, char **value)

	xmmsv_t *xmmsv_coll_attributes_get (xmmsv_coll_t *coll)

#####################################################################

from select import select
from os import write
import os
import traceback
import sys

cdef to_unicode(char *s):
	try:
		ns = PyUnicode_DecodeUTF8(s, len(s), NULL)
	except:
		ns = s
	return ns

cdef from_unicode(object o):
	if isinstance(o, unicode):
		return PyUnicode_AsUTF8String(o)
	else:
		return o

cdef int ResultNotifier(xmmsv_t *res, void *o):
	cdef object obj
	obj = <object> o
	if obj._callback():
		return 1
	return 0

cdef void ObjectFreeer(void *o):
	cdef object obj
	obj = <object> o
	Py_DECREF(obj)

from propdict import PropDict

cdef class Collection:
	cdef xmmsv_coll_t *coll
	cdef object attributes
	cdef object operands
	cdef object idl

	def __init__(self):
		self.idl=CollectionIDList()
		self.attributes=CollectionAttributes()
		self.operands=CollectionOperands()

	def __dealloc__(self):
		if self.coll != NULL:
			xmmsv_coll_unref (self.coll)
		self.coll = NULL

	def __getattr__(self, name):
		if name == 'attributes':
			return self.attributes
		elif name == 'operands':
			return self.operands
		elif name == 'ids':
			return self.idl
		raise AttributeError("No such attribute")

	def x__setattr__(self, name, value):
		if name == 'ids' and value != self.ids:
			raise RuntimeError("Can't change ids")
		if name == 'operands' and value != self.operands:
			raise RuntimeError("Can't change operands")
		if name == 'attributes' and value != self.attributes:
			raise RuntimeError("Can't change attributes")

		#if name not in ('ids', 'operands', 'attributes'):
		#	raise AttributeError("No such attribute")

	def __repr__(self):
		atr = []
		for k,v in self.attributes.iteritems():
			atr.append("%s=%s" % (k, repr(v)))
		return "%s(%s)" % (self.__class__.__name__,",".join(atr))

	def __or__(self, other): # |
		return Union(self, other)
	def __and__(self, other): # &
		return Intersection(self, other)
	def __invert__(self): #~
		return Complement(self)

cdef class CollectionIDList:
	cdef xmmsv_coll_t *coll

	def __cinit__(self):
		self.coll = NULL

	def __dealloc__(self):
		if self.coll != NULL:
			xmmsv_coll_unref(self.coll)
		self.coll = NULL

	def __len__(self):
		return xmmsv_coll_idlist_get_size(self.coll)

	def list(self):
		"""Returns a _COPY_ of the idlist as an ordinary list"""
		cdef unsigned int x
		cdef int l
		cdef int i
		l = xmmsv_coll_idlist_get_size(self.coll)
		i = 0
		res = []
		while i < l:
			x = -1
			xmmsv_coll_idlist_get_index(self.coll, i, &x)
			res.append(x)
			i = i + 1
		return res

	def __repr__(self):
		return repr(self.list())

	def __iter__(self):
		return iter(self.list())

	def append(self, int v):
		"""Appends an id to the idlist"""
		xmmsv_coll_idlist_append(self.coll, v)

	def __iadd__(self, v):
		for a in v:
			self.append(a)
		return self

	def insert(self, int v, int i):
		"""Inserts an id at specified position"""
		if not xmmsv_coll_idlist_insert(self.coll, v, i):
			raise IndexError("Index out of range")

	def remove(self, int i):
		"""Removes entry as specified position"""
		if not xmmsv_coll_idlist_remove(self.coll, i):
			raise IndexError("Index out of range")

	def __delitem__(self, int i):
		self.remove(i)

	def __getitem__(self, int i):
		cdef unsigned int x
		if i < 0:
			i = len(self) + i
		if not xmmsv_coll_idlist_get_index(self.coll, i, &x):
			raise IndexError("Index out of range")
		return x
		
	def __setitem__(self, int i, int v):
		if not xmmsv_coll_idlist_set_index(self.coll, i, v):
			raise IndexError("Index out of range")

cdef class CollectionOperands:
	cdef xmmsv_coll_t *coll
	cdef object pylist
	
	def __cinit__(self):
		self.pylist = []
		self.coll = NULL
	def __dealloc__(self):
		if self.coll != NULL:
			xmmsv_coll_unref(self.coll)
		self.coll = NULL
	def __repr__(self):
		return repr(self.pylist)
	def __str__(self):
		return str(self.pylist)
	def __len__(self):
		return len(self.pylist)
	def __getitem__(self, i):
		return self.pylist[i]
	def __iter__(self):
		return iter(self.pylist)

	def append(self, Collection op not None):
		"""Append an operand"""
		xmmsv_coll_add_operand(self.coll, op.coll)
		self.pylist.append(op)
		
	def remove(self, Collection op not None):
		"""Remove an operand"""
		self.pylist.remove(op)
		xmmsv_coll_remove_operand(self.coll, op.coll)
	
cdef class CollectionAttributes:
	cdef xmmsv_coll_t *coll
	cdef object pydict

	def __cinit__(self):
		self.coll = NULL

	def __dealloc__(self):
		if self.coll != NULL:
			xmmsv_coll_unref(self.coll)
		self.coll = NULL

	def _py_dict(self):
		cdef xmmsv_dict_iter_t *it
		cdef char *x
		cdef char *y
		dct = {}

		if not xmmsv_get_dict_iter(xmmsv_coll_attributes_get(self.coll), &it):
			raise RuntimeError("Failed to get dict iterator")

		xmmsv_dict_iter_first(it)
		while xmmsv_dict_iter_valid(it):
			xmmsv_dict_iter_pair_string(it, <xmms_pyrex_constcharpp_t>&x, <xmms_pyrex_constcharpp_t>&y)
			dct[x] = y

			xmmsv_dict_iter_next(it)
		xmmsv_dict_iter_explicit_destroy (it);
		return dct

	def __repr__(self):
		return repr(self._py_dict())

	def __str__(self):
		return str(self._py_dict())

	def __getitem__(self, name):
		ret = self._py_dict()[name]
		return ret

	def get(self, name, default=None):
		try:
			return self[name]
		except KeyError:
			return default

	def __setitem__(self, name, val):
		xmmsv_coll_attribute_set(self.coll, name, val)

	def items(self):
		return self._py_dict().items()
	
	def iteritems(self):
		return self._py_dict().iteritems()

	def keys(self):
		return self._py_dict().keys()

	def iterkeys(self):
		return self._py_dict().iterkeys()

	def values(self):
		return self._py_dict().values()

	def itervalues(self):
		return self._py_dict().itervalues()

	def __iter__(self):
		return iter(self._py_dict())


# Create a dummy object that can't be accessed
# from outside this module. Used as a argument
# to BaseCollection to tell it not to setup
# the attributes. Pretty ugly hack.
cdef object DontSetup
DontSetup = object()

class BaseCollection(Collection):
	def __init__(Collection self, int typ, setup=None):
		cdef CollectionAttributes atr
		cdef CollectionOperands opr
		cdef CollectionIDList idl

		Collection.__init__(self)

		if setup == DontSetup:
			return

		self.coll = xmmsv_coll_new(<xmmsv_coll_type_t> typ)
		if self.coll == NULL:
			raise RuntimeError("Bad coll")

		atr = self.attributes
		xmmsv_coll_ref(self.coll)
		atr.coll = self.coll

		opr = self.operands
		xmmsv_coll_ref(self.coll)
		opr.coll = self.coll

		idl = self.idl
		xmmsv_coll_ref(self.coll)
		idl.coll = self.coll

class Reference(BaseCollection):
	def __init__(Collection self, ref, ns="Collections"):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_REFERENCE)
		xmmsv_coll_attribute_set (self.coll, "namespace", ns);
		xmmsv_coll_attribute_set (self.coll, "reference", ref);

class Universe(Reference):
	def __init__(self):
		# we could use "xmmsv_coll_universe()" here
		# but this is easier. And coll_universe is just this
		Reference.__init__(self, "All Media")

class Equals(BaseCollection):
	def __init__(Collection self, parent=None, **kv):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_EQUALS)
		if parent is None:
			parent = Universe()
		self.operands.append(parent)
		for k,v in kv.items():
			xmmsv_coll_attribute_set (self.coll, k, v);

class Match(BaseCollection):
	def __init__(Collection self, parent=None, **kv):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_MATCH)
		if parent is None:
			parent = Universe()
		self.operands.append(parent)
		for k,v in kv.items():
			xmmsv_coll_attribute_set (self.coll, k, v);

class Smaller(BaseCollection):
	def __init__(Collection self, parent=None, **kv):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_SMALLER)
		if parent is None:
			parent = Universe()
		self.operands.append(parent)
		for k,v in kv.items():
			xmmsv_coll_attribute_set (self.coll, k, v);

class Greater(BaseCollection):
	def __init__(Collection self, parent=None, **kv):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_GREATER)
		if parent is None:
			parent = Universe()
		self.operands.append(parent)
		for k,v in kv.items():
			xmmsv_coll_attribute_set (self.coll, k, v);

class IDList(BaseCollection):
	def __init__(self):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_IDLIST)

class Queue(BaseCollection):
	def __init__(self):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_QUEUE)

class PShuffle(BaseCollection):
	def __init__(self, parent):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_PARTYSHUFFLE)
		self.operands.append(parent)

class Union(BaseCollection):
	def __init__(Collection self, *a):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_UNION)
		for o in a:
			self.operands.append(o)

class Intersection(BaseCollection):
	def __init__(Collection self, *a):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_INTERSECTION)
		for o in a:
			self.operands.append(o)

class Complement(BaseCollection):
	def __init__(Collection self, parent):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_COMPLEMENT)
		self.operands.append(parent)

class Has(BaseCollection):
	def __init__(Collection self, parent, field):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_HAS)
		self.operands.append(parent)
		self.attributes['field'] = field


cdef class XMMSValue

cdef create_coll(xmmsv_coll_t *coll):
	cdef xmmsv_coll_type_t typ
	cdef Collection c
	cdef CollectionAttributes atr
	cdef CollectionOperands opr
	cdef CollectionIDList idl
	cdef XMMSValue V
	
	typ = xmmsv_coll_get_type(coll)
	c = BaseCollection(typ, DontSetup)
	if typ == XMMS_COLLECTION_TYPE_REFERENCE:
		c.__class__ = Reference
	elif typ == XMMS_COLLECTION_TYPE_UNION:
		c.__class__ = Union
	elif typ == XMMS_COLLECTION_TYPE_INTERSECTION:
		c.__class__ = Intersection
	elif typ == XMMS_COLLECTION_TYPE_COMPLEMENT:
		c.__class__ = Complement
	elif typ == XMMS_COLLECTION_TYPE_HAS:
		c.__class__ = Has
	elif typ == XMMS_COLLECTION_TYPE_EQUALS:
		c.__class__ = Equals
	elif typ == XMMS_COLLECTION_TYPE_MATCH:
		c.__class__ = Match
	elif typ == XMMS_COLLECTION_TYPE_SMALLER:
		c.__class__ = Smaller
	elif typ == XMMS_COLLECTION_TYPE_GREATER:
		c.__class__ = Greater
	elif typ == XMMS_COLLECTION_TYPE_IDLIST:
		c.__class__ = IDList
	elif typ == XMMS_COLLECTION_TYPE_QUEUE:
		c.__class__ = Queue
	elif typ == XMMS_COLLECTION_TYPE_PARTYSHUFFLE:
		c.__class__ = PShuffle
	else:
		raise RuntimeError("Unknown collection typ")

	c.coll = coll
	xmmsv_coll_ref(coll)

	atr = c.attributes
	xmmsv_coll_ref(coll)
	atr.coll = coll

	#slightly hackish
	if typ == XMMS_COLLECTION_TYPE_REFERENCE:
		if atr.get("reference") == "All Media":
			c.__class__ = Universe
			

	idl = c.idl
	xmmsv_coll_ref(coll)
	idl.coll = coll

	opr = c.operands

	V = XMMSValue()
	V.set_value(xmmsv_coll_operands_get(coll))
	opr.pylist = V.value()

	xmmsv_coll_ref(coll)
	opr.coll = coll
	
	return c

def coll_parse(pattern):
	cdef xmmsv_coll_t *coll

	ptrn = from_unicode(pattern)
	if not xmmsv_coll_parse(ptrn, &coll):
		raise ValueError('unable to parse pattern')
	return create_coll(coll)

cdef xmmsv_t *create_native_value(value):
	cdef xmmsv_t *ret

	if value is None:
		ret = xmmsv_new_none()
	elif isinstance(value, int):
		ret = xmmsv_new_int(value)
	elif isinstance(value, basestring):
		tmp = from_unicode(value)
		ret = xmmsv_new_string(tmp)
	elif isinstance(value, list):
		ret = xmmsv_new_list()
		for item in value:
			xmmsv_list_append(ret, create_native_value(item))
	elif isinstance(value, dict):
		ret = xmmsv_new_dict()
		for key,item in r.iteritems():
			tmp = from_unicode(key)
			xmmsv_dict_set(ret, tmp, create_native_value(item))
	return ret

cdef class XMMSValue:
	cdef object sourcepref
	cdef xmmsv_t *val
	cdef int ispropdict

	def __init__(self, sourcepref=None):
		self.sourcepref = sourcepref
		self.ispropdict = 0

	cdef set_value(self, xmmsv_t *value):
		xmmsv_ref(value)
		self.val = value

	def get_type(self):
		"""
		Return the type of data contained in this result.
		The return value is one of the OBJECT_CMD_ARG_* constants.
		"""
		return xmmsv_get_type(self.val)

	def value(self):
		"""
		Return value of appropriate data type contained in this result.
		This can be used instead of most get_* functions in this class.
		"""
		cdef xmmsv_type_t typ

		typ = self.get_type()

		if typ == XMMSV_TYPE_NONE:
			return None
		elif typ == XMMSV_TYPE_ERROR:
			return self.get_error()
		elif typ == XMMSV_TYPE_INT32:
			return self.get_int()
		elif typ == XMMSV_TYPE_STRING:
			return self.get_string()
		elif typ == XMMSV_TYPE_COLL:
			return self.get_coll()
		elif typ == XMMSV_TYPE_BIN:
			return self.get_bin()
		elif typ == XMMSV_TYPE_LIST:
			return self.get_list()
		elif typ == XMMSV_TYPE_DICT:
			if self.ispropdict:
				return self.get_propdict()
			return self.get_dict()
		else:
			raise TypeError("Unknown type returned from the server: %d." % typ)

	def get_int(self):
		"""
		Get data from the result structure as an int.
		@rtype: int
		"""
		cdef int ret
		if xmmsv_get_int(self.val, &ret):
			return ret
		else:
			raise ValueError("Failed to retrieve value!")

	def get_string(self):
		"""
		Get data from the result structure as a string.
		@rtype: string
		"""
		cdef char *ret

		if xmmsv_get_string(self.val, <xmms_pyrex_constcharpp_t>&ret):
			return to_unicode(ret)
		else:
			raise ValueError("Failed to retrieve value!")

	def get_bin(self):
		"""
		Get data from the result structure as binary data.
		@rtype: string
		"""
		cdef unsigned char *ret
		cdef unsigned int rlen

		if xmmsv_get_bin(self.val, <xmms_pyrex_constucharpp_t> &ret, &rlen):
			return PyString_FromStringAndSize(<char *>ret, rlen)
		else:
			raise ValueError("Failed to retrieve value!")

	def get_coll(self):
		"""
		Get data from the result structure as a Collection.
		@rtype: Collection
		"""
		cdef xmmsv_coll_t *coll
		if not xmmsv_get_coll(self.val, &coll):
			raise ValueError("Failed to retrieve value!")

		return create_coll(coll)

	def get_dict(self) :
		"""
		@return: A dictionary containing media info.
		"""
		cdef xmmsv_dict_iter_t *it
		cdef xmms_pyrex_constcharp_t k
		cdef xmmsv_t *v
		cdef XMMSValue V
		ret = {}

		if not xmmsv_get_dict_iter(self.val, &it):
			raise RuntimeError("Failed to get dict iterator")

		while xmmsv_dict_iter_valid(it):
			xmmsv_dict_iter_pair(it, &k, &v)
			V=XMMSValue(self.sourcepref)
			V.set_value(v)

			ret[<char *>k] = V.value()
			xmmsv_dict_iter_next(it)
		xmmsv_dict_iter_explicit_destroy (it)
		return ret

	def get_propdict(self):
		"""
		@return: A source dict.
		"""
		ret = PropDict(self.sourcepref)
		for key, values in self.get_dict().iteritems():
			for source, value in values.iteritems():
				ret[(source,key)] = value
		return ret

	def get_list (self) :
		"""
		@return: A list of dicts from the result structure.
		"""
		cdef xmmsv_list_iter_t *iter
		cdef xmmsv_t *val
		cdef XMMSValue obj

		ret = []

		xmmsv_get_list_iter(self.val, &iter)
		while xmmsv_list_iter_valid(iter):
			xmmsv_list_iter_entry(iter, &val)

			obj = XMMSValue(self.sourcepref)
			obj.set_value(val)

			ret.append(obj.value())

			xmmsv_list_iter_next(iter)
		xmmsv_list_iter_explicit_destroy(iter)
		return ret

	def iserror(self):
		"""
		@deprecated
		@return: Whether the value represents an error or not.
		@rtype: Boolean
		"""

		return self.is_error()

	def is_error(self):
		"""
		@return: Whether the value represents an error or not.
		@rtype: Boolean
		"""
		return xmmsv_is_error(self.val)

	def get_error(self):
		"""
		@return: Error string from the result.
		@rtype: String
		"""
		cdef char *ret

		if xmmsv_get_error(self.val, <xmms_pyrex_constcharpp_t>&ret):
			return to_unicode(ret)
		else:
			raise ValueError("Failed to retrieve value!")

	def __dealloc__(self):
		if self.val != NULL:
			xmmsv_unref(self.val)
		self.val = NULL

cdef class XMMSResult:
	"""
	Class containing the results of some operation
	"""
	cdef xmmsc_result_t *res
	cdef object callback
	cdef object exc
	cdef object xmms
	cdef int ispropdict

	def __init__(self):
		self.callback = None
		self.ispropdict = 0

	cdef set_connection(self, conn):
		self.xmms = conn

	cdef set_result(self, xmmsc_result_t *res):
		self.res = res

	cdef set_callback(self, callback):
		assert(callback)

		self.callback = callback

		Py_INCREF(self)

		xmmsc_result_notifier_set_full(self.res, ResultNotifier, <void *> self, ObjectFreeer)

	def _callback(self):
		try:
			ret = self.callback(self._value())
		except:
			exc = sys.exc_info()
			traceback.print_exception (exc[0], exc[1], exc[2])
			return 0

		# None most likely means missing returvalue,
		# which most likely means user wants restart
		if ret is None:
			ret = 1

		return ret

	def wait(self):
		"""
		Wait for the result from the daemon.
		"""
		xmmsc_result_wait(self.res)
		if self.exc is not None:
			raise self.exc[0], self.exc[1], self.exc[2]

	def iserror(self):
		"""
		@return: Whether the result represents an error or not.
		@rtype: Boolean
		"""
		return self._value().iserror()

	def _value(self):
		cdef XMMSValue obj
		cdef xmmsv_t *value

		value = xmmsc_result_get_value(self.res)

		obj = XMMSValue(self.xmms.get_source_preference())
		obj.set_value(value)
		obj.ispropdict = self.ispropdict

		return obj

	def value(self):
		return self._value().value()

	def __dealloc__(self):
		"""
		Deallocate the result.
		"""
		if self.res != NULL:
			xmmsc_result_unref(self.res)
		self.res = NULL

cdef void python_need_out_fun(int i, void *obj):
	cdef object o
	o = <object> obj
	o._needout_cb(i)

cdef void python_disconnect_fun(void *obj):
	cdef object o
	o = <object> obj
	o._disconnect_cb()

def userconfdir_get():
	"""
	Get the user configuration directory, where XMMS2 stores its
	user-specific configuration files. Clients may store their
	configuration under the 'clients' subdirectory. This varies from
	platform to platform so should always be retrieved at runtime.
	"""
	cdef char path[XMMS_PATH_MAX]
	if xmmsc_userconfdir_get (path, XMMS_PATH_MAX) == NULL:
		return None
	return path

cdef class XMMS:
	"""
	This is the class representing the XMMS2 client itself. The methods in
	this class may be used to control and interact with XMMS2.
	"""
	cdef xmmsc_connection_t *conn
	cdef object do_loop
	cdef object wakeup
	cdef object disconnect_fun
	cdef object needout_fun
	cdef object sources

	def __cinit__(self, clientname = None):
		"""
		Initiates a connection to the XMMS2 daemon. All operations
		involving the daemon are done via this connection.
		"""
		if clientname is None:
			clientname = "UnnamedPythonClient"

		tmp = from_unicode(clientname)

		self.conn = xmmsc_init(tmp)
		if self.conn == NULL:
			raise ValueError("Failed to initialize xmmsclient library! Probably due to broken name.")

		self.sources = ["client/" + clientname, "server", "plugins/*", "client/*", "*"]

	def get_source_preference(self):
		return self.sources
	def set_source_preference(self, sources):
		self.sources = sources

	def __dealloc__(self):
		""" destroys it all! """
		if self.conn != NULL:
			xmmsc_unref(self.conn)
		self.conn = NULL

	def _needout_cb(self, i):
		if self.needout_fun is not None:
			self.needout_fun(i)

	def _disconnect_cb(self):
		if self.disconnect_fun is not None:
			self.disconnect_fun(self)

	def exit_loop(self):
		"""
		exit_loop()

		Exits from the L{loop} call
		"""
		self.do_loop = False
		write(self.wakeup, "42")

	def loop(self):
		"""
		loop()

		Main client loop for most python clients. Call this to run the
		client once everything has been set up. This function blocks
		until L{exit_loop} is called.
		"""
		fd = xmmsc_io_fd_get(self.conn)
		(r, w) = os.pipe()

		self.do_loop = True
		self.wakeup = w

		while self.do_loop:

			if self.want_ioout():
				w = [fd]
			else:
				w = []

			(i, o, e) = select([fd, r], w, [fd])

			if i and i[0] == fd:
				xmmsc_io_in_handle(self.conn)
			if o and o[0] == fd:
				xmmsc_io_out_handle(self.conn)
			if e and e[0] == fd:
				xmmsc_io_disconnect(self.conn)
				self.do_loop = False

	def ioin(self):
		"""
		ioin() -> bool

		Read data from the daemon, when available. Note: This is a low
		level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_in_handle(self.conn)

	def ioout(self):
		"""
		ioout() -> bool

		Write data out to the daemon, when available. Note: This is a
		low level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_out_handle(self.conn)

	def want_ioout(self):
		"""
		want_ioout() -> bool
		"""
		return xmmsc_io_want_out(self.conn)

	def set_need_out_fun(self, fun):
		"""
		set_need_out_fun(fun)
		"""
		Py_INCREF(self)
		xmmsc_io_need_out_callback_set_full(self.conn, python_need_out_fun, <void *>self, ObjectFreeer)
		self.needout_fun = fun
		
	def get_fd(self):
		"""
		get_fd() -> int

		Get the underlying file descriptor used for communication with
		the XMMS2 daemon. You can use this in a client to ensure that
		the IPC link is still active and safe to use.(e.g by calling
		select() or poll())
		@rtype: int
		@return: IPC file descriptor
		"""
		return xmmsc_io_fd_get(self.conn)

	def connect(self, path = None, disconnect_func = None):
		"""
		connect(path=None, disconnect_func=None)

		Connect to the appropriate IPC path, for communication with the
		XMMS2 daemon. This path defaults to /tmp/xmms-ipc-<username> if
		not specified. Call this once you have instantiated the object:

		C{import xmmsclient}

		C{xmms = xmmsclient.XMMS()}

		C{xmms.connect()}

		...
		
		You can provide a disconnect callback function to be activated
		when the daemon disconnects.(e.g. daemon quit) This function
		typically has to exit the main loop used by your application.
		For example, if using L{loop}, your callback should call
		L{exit_loop} at some point.
		"""
		if path:
			ret = xmmsc_connect(self.conn, path)
		else:
			ret = xmmsc_connect(self.conn, NULL)

		if not ret:
			raise IOError("Couldn't connect to server!")

		self.disconnect_fun = disconnect_func
		Py_INCREF(self)
		xmmsc_disconnect_callback_set_full(self.conn, python_disconnect_fun, <void *>self, ObjectFreeer)


	def quit(self, cb = None):
		"""
		quit(cb=None) -> XMMSResult

		Tell the XMMS2 daemon to quit.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_quit(self.conn))

	def plugin_list(self, typ, cb = None):
		"""
		plugin_list(typ, cb=None) -> XMMSResult

		Get a list of loaded plugins from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_plugin_list(self.conn, typ))

	def playback_start(self, cb = None):
		"""
		playback_start(cb=None) -> XMMSResult

		Instruct the XMMS2 daemon to start playing the currently
		selected file from the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_start(self.conn))

	def playback_stop(self, cb = None):
		"""
		playback_stop(cb=None) -> XMMSResult

		Instruct the XMMS2 daemon to stop playing the file
		currently being played.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_stop(self.conn))

	def playback_tickle(self, cb = None):
		"""
		playback_tickle(cb=None) -> XMMSResult

		Instruct the XMMS2 daemon to move on to the next playlist item.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_tickle(self.conn))

	def playback_pause(self, cb = None):
		"""
		playback_pause(cb=None) -> XMMSResult

		Instruct the XMMS2 daemon to pause playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_pause(self.conn))

	def playback_current_id(self, cb = None):
		"""
		playback_current_id(cb=None) -> XMMSResult

		@rtype: L{XMMSResult}(UInt)
		@return: The medialib id of the item currently selected.
		"""
		return self.create_result(cb, xmmsc_playback_current_id(self.conn))

	def playback_seek_ms(self, ms, cb = None):
		"""
		playback_seek_ms(ms, cb=None) -> XMMSResult

		Seek to an absolute time position in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_seek_ms_abs(self.conn, ms))

	def playback_seek_ms_rel(self, ms, cb = None):
		"""
		playback_seek_ms_rel(ms, cb=None) -> XMMSResult

		Seek to a time position by the given offset in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_seek_ms_rel(self.conn, ms))

	def playback_seek_samples(self, samples, cb = None):
		"""
		playback_seek_samples(samples, cb=None) -> XMMSResult

		Seek to an absolute number of samples in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_seek_samples_abs(self.conn, samples))

	def playback_seek_samples_rel(self, samples, cb = None):
		"""
		playback_seek_samples_rel(samples, cb=None) -> XMMSResult

		Seek to a number of samples by the given offset in the
		current file or stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_seek_samples_rel(self.conn, samples))

	def playback_status(self, cb = None):
		"""
		playback_status(cb=None) -> XMMSResult

		Get current playback status from XMMS2 daemon. This is
		essentially the more direct version of
		L{broadcast_playback_status}. Possible return values are:
		L{PLAYBACK_STATUS_STOP}, L{PLAYBACK_STATUS_PLAY},
		L{PLAYBACK_STATUS_PAUSE}
		@rtype: L{XMMSResult}(UInt)
		@return: Current playback status(UInt)
		"""
		return self.create_result(cb, xmmsc_playback_status(self.conn))

	def broadcast_playback_status(self, cb = None):
		"""
		broadcast_playback_status(cb=None) -> XMMSResult

		Set a method to handle the playback status broadcast from the
		XMMS2 daemon.
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_status(self.conn))

	def broadcast_playback_current_id(self, cb = None):
		"""
		broadcast_playback_current_id(cb=None) -> XMMSResult

		Set a method to handle the playback id broadcast from the
		XMMS2 daemon.
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_current_id(self.conn))

	cdef create_result(self, cb, xmmsc_result_t *res):
		cdef XMMSResult ret

		if res == NULL:
			raise RuntimeError("xmmsc_result_t couldn't be allocated")

		ret = XMMSResult()
		ret.set_result(res)
		ret.set_connection(self)
		if cb is not None:
			ret.set_callback(cb)

		return ret

	def playback_playtime(self, cb = None):
		"""
		playback_playtime(cb=None) -> XMMSResult

		Return playtime on current file/stream. This is essentially a
		more direct version of L{signal_playback_playtime}
		@rtype: L{XMMSResult}(UInt)
		@return: The result of the operation.(playtime in milliseconds)
		"""
		return self.create_result(cb, xmmsc_playback_playtime(self.conn))

	def signal_playback_playtime(self, cb = None):
		"""
		signal_playback_playtime(cb=None) -> XMMSResult

		Set a method to handle the playback playtime signal from the
		XMMS2 daemon.
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_signal_playback_playtime(self.conn))

	def playback_volume_set(self, channel, volume, cb = None):
		"""
		playback_volume_set(channel, volume, cb=None) -> XMMSResult

		Set the playback volume for specified channel
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_playback_volume_set(self.conn, channel, volume))

	def playback_volume_get(self, cb = None):
		"""
		playback_volume_get(cb=None) -> XMMSResult

		Get the playback for all channels
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_playback_volume_get(self.conn))

	def broadcast_playback_volume_changed(self, cb = None):
		"""
		broadcast_playback_volume_changed(cb=None) -> XMMSResult

		Set a broadcast callback for volume updates
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_volume_changed(self.conn))

	def broadcast_playlist_loaded(self, cb = None):
		"""
		broadcast_playlist_loaded(cb=None) -> XMMSResult

		Set a broadcast callback for loaded playlist event
		@rtype: L{XMMSResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_loaded(self.conn))

	def playlist_load(self, playlist = None, cb = None):
		"""
		playlist_load(playlist=None, cb=None) -> XMMSResult

		Load the playlist as current playlist
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef object p
		cdef char *pl
		pl = NULL
		if playlist is not None:
			p = from_unicode(playlist)
			pl = p
		return self.create_result(cb, xmmsc_playlist_load(self.conn, pl))

	def playlist_list(self, cb = None):
		"""
		playlist_list(cb=None) -> XMMSResult

		Lists the playlists
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playlist_list(self.conn))

	def playlist_remove(self, playlist = None, cb = None):
		"""
		playlist_remove(playlist=None, cb=None) -> XMMSResult

		Remove the playlist from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_remove(self.conn, pl))
		else:
			return self.create_result(cb, xmmsc_playlist_remove(self.conn, NULL))

	def playlist_shuffle(self, playlist = None, cb = None):
		"""
		playlist_shuffle(playlist=None, cb=None) -> XMMSResult

		Instruct the XMMS2 daemon to shuffle the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_shuffle(self.conn, pl))
		else:
			return self.create_result(cb, xmmsc_playlist_shuffle(self.conn, NULL))

	def playlist_rinsert(self, pos, url, playlist = None, cb = None):
		"""
		playlist_rinsert(pos, url, playlist=None, cb=None) -> XMMSResult

		Insert a directory in the playlist.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)

		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_rinsert(self.conn, pl, pos, c))
		else:
			return self.create_result(cb, xmmsc_playlist_rinsert(self.conn, NULL, pos, c))

	def playlist_rinsert_encoded(self, pos, url, playlist = None, cb = None):
		"""
		playlist_rinsert_encoded(pos, url, playlist=None, cb=None) -> XMMSResult

		Insert a directory in the playlist.
		Requires an int 'pos' and a string 'url' as argument.

		'url' argument has to be medialib encoded.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)

		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_rinsert_encoded(self.conn, pl, pos, c))
		else:
			return self.create_result(cb, xmmsc_playlist_rinsert_encoded(self.conn, NULL, pos, c))

	def playlist_insert_url(self, pos, url, playlist = None, cb = None):
		"""
		playlist_insert_url(pos, url, playlist=None, cb=None) -> XMMSResult

		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)

		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_insert_url(self.conn, pl, pos, c))
		else:
			return self.create_result(cb, xmmsc_playlist_insert_url(self.conn, NULL, pos, c))

	def playlist_insert_encoded(self, pos, url, playlist = None, cb = None):
		"""
		playlist_insert_encoded(pos, url, playlist=None, cb=None) -> XMMSResult

		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires an int 'pos' and a string 'url' as argument.

		The 'url' should be encoded to this function.

		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)
		
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_insert_encoded(self.conn, pl, pos, c))
		else:
			return self.create_result(cb, xmmsc_playlist_insert_encoded(self.conn, NULL, pos, c))
		
		return ret


	def playlist_insert_id(self, pos, id, playlist = None, cb = None):
		"""
		playlist_insert_id(pos, id, playlist=None, cb=None) -> XMMSResult

		Insert a medialib to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_insert_id(self.conn, pl, pos, id))
		else:
			return self.create_result(cb, xmmsc_playlist_insert_id(self.conn, NULL, pos, id))


	def playlist_insert_collection(self, pos, Collection coll, order = None, playlist = None, cb = None):
		"""
		playlist_insert_collection(pos, coll, order, playlist=None, cb=None) -> XMMSResult

		Insert the content of a collection to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if order is None:
			order = []

		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_insert_collection(self.conn, pl, pos, coll.coll, create_native_value(order)))
		else:
			return self.create_result(cb, xmmsc_playlist_insert_collection(self.conn, NULL, pos, coll.coll, create_native_value(order)))

	def playlist_radd(self, url, playlist = None, cb = None):
		"""
		playlist_radd(url, playlist=None, cb=None) -> XMMSResult

		Add a directory to the playlist.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)
		
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_radd(self.conn, pl, c))
		else:
			return self.create_result(cb, xmmsc_playlist_radd(self.conn, NULL, c))

	def playlist_radd_encoded(self, url, playlist = None, cb = None):
		"""
		playlist_radd_encoded(url, playlist=None, cb=None) -> XMMSResult

		Add a directory to the playlist.
		Requires a string 'url' as argument.
		'url' argument has to be medialib encoded.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)
		
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_radd_encoded(self.conn, pl, c))
		else:
			return self.create_result(cb, xmmsc_playlist_radd_encoded(self.conn, NULL, c))

	def playlist_add_url(self, url, playlist = None, cb = None):
		"""
		playlist_add_url(url, playlist=None, cb=None) -> XMMSResult

		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)

		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_add_url(self.conn, pl, c))
		else:
			return self.create_result(cb, xmmsc_playlist_add_url(self.conn, NULL, c))

	def playlist_add_encoded(self, url, playlist = None, cb = None):
		"""
		playlist_add_encoded(url, playlist=None, cb=None) -> XMMSResult

		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		The 'url' has to be medialib encoded.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(url)
		
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_add_encoded(self.conn, pl, c))
		else:
			return self.create_result(cb, xmmsc_playlist_add_encoded(self.conn, NULL, c))


	def playlist_add_id(self, id, playlist = None, cb = None):
		"""
		playlist_add_id(id, playlist=None, cb=None) -> XMMSResult

		Add a medialib id to the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_add_id(self.conn, pl, id))
		else:
			return self.create_result(cb, xmmsc_playlist_add_id(self.conn, NULL, id))


	def playlist_add_collection(self, Collection coll, order = None, playlist = None, cb = None):
		"""
		playlist_add_collection(coll, order, playlist=None, cb=None) -> XMMSResult

		Add the content of a collection to the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""

		if order is None:
			order = []

		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_add_collection(self.conn, pl, coll.coll, create_native_value(order)))
		else:
			return self.create_result(cb, xmmsc_playlist_add_collection(self.conn, NULL, coll.coll, create_native_value(order)))



	def playlist_remove_entry(self, id, playlist = None, cb = None):
		"""
		playlist_remove_entry(id, playlist=None, cb=None) -> XMMSResult

		Remove a certain media item from the playlist.
		Requires a number 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_remove_entry(self.conn, pl, id))
		else:
			return self.create_result(cb, xmmsc_playlist_remove_entry(self.conn, NULL, id))

	def playlist_clear(self, playlist = None, cb = None):
		"""
		playlist_clear(playlist=None, cb=None) -> XMMSResult

		Clear the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_clear(self.conn, pl))
		else:
			return self.create_result(cb, xmmsc_playlist_clear(self.conn, NULL))

	def playlist_list_entries(self, playlist = None, cb = None):
		"""
		playlist_list_entries(playlist=None, cb=None) -> XMMSResult

		Get the current playlist. This function returns a list of IDs
		of the files/streams currently in the playlist. Use
		L{medialib_get_info} to retrieve more specific information.
		@rtype: L{XMMSResult}(UIntList)
		@return: The current playlist.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_list_entries(self.conn, pl))
		else:
			return self.create_result(cb, xmmsc_playlist_list_entries(self.conn, NULL))

	def playlist_sort(self, props, playlist = None, cb = None):
		"""
		playlist_sort(props, playlist=None, cb=None) -> XMMSResult

		Sorts the playlist according to the properties specified.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_sort(self.conn, pl, create_native_value(props)))
		else:
			return self.create_result(cb, xmmsc_playlist_sort(self.conn, NULL, create_native_value(props)))

	def playlist_set_next_rel(self, position, cb = None):
		"""
		playlist_set_next_rel(position, cb=None) -> XMMSResult

		Sets the position in the playlist. Same as L{playlist_set_next}
		but sets the next position relative to the current position.
		You can do set_next_rel(-1) to move backwards for example.
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_playlist_set_next_rel(self.conn, position))


	def playlist_set_next(self, position, cb = None):
		"""
		playlist_set_next(position, cb=None) -> XMMSResult

		Sets the position to move to, next, in the playlist. Calling
		L{playback_tickle} will perform the jump to that position.
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_playlist_set_next(self.conn, position))

	def playlist_move(self, cur_pos, new_pos, playlist = None, cb = None):
		"""
		playlist_move(cur_pos, new_pos, playlist=None, cb=None) -> XMMSResult

		Moves a playlist entry to a new position.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_move_entry(self.conn, pl, cur_pos, new_pos))
		else:
			return self.create_result(cb, xmmsc_playlist_move_entry(self.conn, NULL, cur_pos, new_pos))

	def playlist_create(self, playlist, cb = None):
		"""
		playlist_create(playlist, cb=None) -> XMMSResult

		Create a new playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		pl = from_unicode(playlist)
		return self.create_result(cb, xmmsc_playlist_create(self.conn, pl))

	def playlist_current_pos(self, playlist = None, cb = None):
		"""
		playlist_current_pos(playlist=None, cb=None) -> XMMSResult

		Returns the current position in the playlist. This value will
		always be equal to, or larger than 0. The first entry in the
		list is 0.
		@rtype: L{XMMSResult}
		"""
		if playlist is not None:
			pl = from_unicode(playlist)
			return self.create_result(cb, xmmsc_playlist_current_pos(self.conn, pl))
		else:
			return self.create_result(cb, xmmsc_playlist_current_pos(self.conn, NULL))

	def playlist_current_active(self, cb = None):
		"""
		playlist_current_active(cb=None) -> XMMSResult

		Returns the name of the current active playlist
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_playlist_current_active(self.conn))

	def broadcast_playlist_current_pos(self, cb = None):
		"""
		broadcast_playlist_current_pos(cb=None) -> XMMSResult

		Set a method to handle the playlist current position updates
		from the XMMS2 daemon. This is triggered whenever the daemon
		jumps from one playlist position to another. (not when moving
		a playlist item from one position to another)
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_current_pos(self.conn))

	def broadcast_playlist_changed(self, cb = None):
		"""
		broadcast_playlist_changed(cb=None) -> XMMSResult

		Set a method to handle the playlist changed broadcast from the
		XMMS2 daemon. Updated data is sent whenever the daemon's
		playlist changes.
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_changed(self.conn))

	def broadcast_config_value_changed(self, cb = None):
		"""
		broadcast_config_value_changed(cb=None) -> XMMSResult

		Set a method to handle the config value changed broadcast
		from the XMMS2 daemon.(i.e. some configuration value has
		been modified) Updated data is sent whenever a config
		value is modified.
		@rtype: L{XMMSResult} (the modified config key and its value)
		"""
		return self.create_result(cb, xmmsc_broadcast_config_value_changed(self.conn))

	def broadcast_configval_changed(self, cb = None):
		"""
		@deprecated
		"""
		return self.broadcast_config_value_changed(cb)

	def config_set_value(self, key, val, cb = None):
		"""
		config_set_value(key, val, cb=None) -> XMMSResult

		Set a configuration value on the daemon, given a key.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c1 = from_unicode(key)
		c2 = from_unicode(val)
		return self.create_result(cb, xmmsc_config_set_value(self.conn, c1, c2))

	def configval_set(self, key, val, cb = None):
		"""
		@deprecated
		"""
		return self.config_set_value(key, val, cb)

	def config_get_value(self, key, cb = None):
		"""
		config_get_value(key, cb=None) -> XMMSResult

		Get the configuration value of a given key, from the daemon.
		@rtype: L{XMMSResult}(String)
		@return: The result of the operation.
		"""
		c = from_unicode(key)
		return self.create_result(cb, xmmsc_config_get_value(self.conn, c))

	def configval_get(self, key, cb = None):
		"""
		@deprecated
		"""
		return self.config_get_value(key, cb)

	def config_list_values(self, cb = None):
		"""
		config_list_values(cb=None) -> XMMSResult

		Get list of configuration keys on the daemon. Use
		L{config_get_value} to retrieve the values corresponding to the
		configuration keys.
		@rtype: L{XMMSResult}(StringList)
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_config_list_values(self.conn))

	def configval_list(self, cb = None):
		"""
		@deprecated
		"""
		return self.config_list_values(cb)

	def config_register_value(self, valuename, defaultvalue, cb = None):
		"""
		config_register_value(valuename, defaultvalue, cb=None) -> XMMSResult

		Register a new configvalue.
		This should be called in the initcode as XMMS2 won't allow
		set/get on values that haven't been registered.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c1 = from_unicode(valuename)
		c2 = from_unicode(defaultvalue)
		return self.create_result(cb, xmmsc_config_register_value(self.conn, c1, c2))

	def configval_register(self, valuename, defaultvalue, cb = None):
		"""
		@deprecated
		"""
		return self.config_register_value(valuename, defaultvalue, cb)

	def medialib_add_entry(self, file, cb = None):
		"""
		medialib_add_entry(file, cb=None) -> XMMSResult

		Add an entry to the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(file)
		return self.create_result(cb, xmmsc_medialib_add_entry(self.conn, c))

	def medialib_add_entry_encoded(self, file, cb = None):
		"""
		medialib_add_entry_encoded(file, cb=None) -> XMMSResult

		Add an entry to the MediaLib.
		Exactly the same as #medialib_add_entry but takes
		a encoded url instead.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(file)
		return self.create_result(cb, xmmsc_medialib_add_entry(self.conn, c))

	def medialib_remove_entry(self, id, cb=None):
		"""
		medialib_remove_entry(id, cb=None) -> XMMSResult

		Remove an entry from the medialib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_remove_entry(self.conn, id))

	def medialib_get_info(self, id, cb = None):
		"""
		medialib_get_info(id, cb=None) -> XMMSResult

		@rtype: L{XMMSResult}(HashTable)
		@return: Information about the medialib entry position
		specified.
		"""
		cdef XMMSResult res
		res = self.create_result(cb, xmmsc_medialib_get_info(self.conn, id))
		res.ispropdict = 1
		return res

	def medialib_rehash(self, id = 0, cb = None):
		"""
		medialib_rehash(id=0, cb=None) -> XMMSResult

		Force the medialib to check that metadata stored is up to
		date.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_rehash(self.conn, id))

	def medialib_get_id(self, url, cb = None):
		"""
		medialib_get_id(url, cb=None) -> XMMSResult

		Search for an entry (URL) in the medialib and return its ID
		number.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_get_id(self.conn, url))

	def medialib_path_import(self, path, cb = None):
		"""
		medialib_path_import(path, cb=None) -> XMMSResult

		Import metadata from all files recursively from the directory
		passed as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(path)
		return self.create_result(cb, xmmsc_medialib_path_import(self.conn, c))

	def medialib_path_import_encoded(self, path, cb = None):
		"""
		medialib_path_import_encoded(path, cb=None) -> XMMSResult

		Import metadata from all files recursively from the directory
		passed as argument. The 'path' argument has to be medialib encoded.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		c = from_unicode(path)
		return self.create_result(cb, xmmsc_medialib_path_import_encoded(self.conn, c))


	def medialib_property_set(self, id, key, value, source=None, cb=None):
		"""
		medialib_property_set(id, key, value, source=None, cb=None) -> XMMSResult

		Associate a value with a medialib entry. Source is optional.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		k = from_unicode(key)
		v = from_unicode(value)
	
		if source:
			s = from_unicode(source)
			if isinstance(value, int):
				return self.create_result(cb, xmmsc_medialib_entry_property_set_int_with_source(self.conn,id,s,k,v))
			else:
				return self.create_result(cb, xmmsc_medialib_entry_property_set_str_with_source(self.conn,id,s,k,v))
		else:
			if isinstance(value, basestring):
				return self.create_result(cb, xmmsc_medialib_entry_property_set_str(self.conn,id,k,v))
			else:
				return self.create_result(cb, xmmsc_medialib_entry_property_set_int(self.conn,id,k,v))

	def medialib_property_remove(self, id, key, source=None, cb=None):
		"""
		medialib_property_remove(id, key, source=None, cb=None) -> XMMSResult

		Remove a value from a medialib entry. Source is optional.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		k = from_unicode(key)

		if source:
			s = from_unicode(source)
			return self.create_result(cb, xmmsc_medialib_entry_property_remove_with_source(self.conn,id,s,k))
		else:
			return self.create_result(cb, xmmsc_medialib_entry_property_remove(self.conn,id,k))

	def broadcast_medialib_entry_added(self, cb = None):
		"""
		broadcast_medialib_entry_added(cb=None) -> XMMSResult

		Set a method to handle the medialib entry added broadcast
		from the XMMS2 daemon. (i.e. a new entry has been added)
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_added(self.conn))

	def broadcast_medialib_entry_changed(self, cb = None):
		"""
		broadcast_medialib_entry_changed(cb=None) -> XMMSResult

		Set a method to handle the medialib entry changed broadcast
		from the XMMS2 daemon.
		Updated data is sent when the metadata for a song is updated
		in the medialib.
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_changed(self.conn))

	def broadcast_collection_changed(self, cb = None):
		"""
		broadcast_collection_changed(cb=None) -> XMMSResult

		Set a method to handle the collection changed broadcast
		from the XMMS2 daemon.
		@rtype: L{XMMSResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_collection_changed(self.conn))

	def signal_mediainfo_reader_unindexed(self, cb = None):
		"""
		signal_mediainfo_reader_unindexed(cb=None) -> XMMSResult

		Tell daemon to send you the number of unindexed files in the mlib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_signal_mediainfo_reader_unindexed(self.conn))

	def broadcast_mediainfo_reader_status(self, cb = None):
		"""
		broadcast_mediainfo_reader_status(cb=None) -> XMMSResult

		Tell daemon to send you the status of the mediainfo reader
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_broadcast_mediainfo_reader_status(self.conn))

	def xform_media_browse(self, url, cb=None):
		"""
		xform_media_browse(url, cb=None) -> XMMSResult

		Browse files from xform plugins.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		u = from_unicode(url)
		return self.create_result(cb, xmmsc_xform_media_browse(self.conn,u))

	def xform_media_browse_encoded(self, url, cb=None):
		"""
		Browse files from xform plugins.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		u = from_unicode(url)
		return self.create_result(cb, xmmsc_xform_media_browse_encoded(self.conn,u))

	def coll_get(self, name, ns, cb=None):
		"""
		coll_get(name, ns, cb=None) -> XMMSResult

		Retrieve a Collection
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if ns == "Collections":
			n = XMMS_COLLECTION_NS_COLLECTIONS
		elif ns == "Playlists":
			n = XMMS_COLLECTION_NS_PLAYLISTS
		else:
			raise ValueError("Bad namespace")

		nam = from_unicode(name)
		
		return self.create_result(cb, xmmsc_coll_get(self.conn, nam, n))

	def coll_list(self, ns="*", cb=None):
		"""
		coll_list(name, ns="*", cb=None) -> XMMSResult

		List collections
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""

		if ns == "Collections":
			n = XMMS_COLLECTION_NS_COLLECTIONS
		elif ns == "Playlists":
			n = XMMS_COLLECTION_NS_PLAYLISTS
		elif ns == "*":
			n = XMMS_COLLECTION_NS_ALL
		else:
			raise ValueError("Bad namespace")

		return self.create_result(cb, xmmsc_coll_list(self.conn, n))

	def coll_save(self, Collection coll, name, ns, cb=None):
		"""
		coll_save(coll, name, ns, cb=None) -> XMMSResult

		Save a collection on server.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if ns == "Collections":
			n = XMMS_COLLECTION_NS_COLLECTIONS
		elif ns == "Playlists":
			n = XMMS_COLLECTION_NS_PLAYLISTS
		else:
			raise ValueError("Bad namespace")

		nam = from_unicode(name)
		
		return self.create_result(cb, xmmsc_coll_save(self.conn, coll.coll, nam, n))

	def coll_remove(self, name, ns, cb=None):
		"""
		coll_remove(name, ns, cb=None) -> XMMSResult

		Remove a collection on server.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if ns == "Collections":
			n = XMMS_COLLECTION_NS_COLLECTIONS
		elif ns == "Playlists":
			n = XMMS_COLLECTION_NS_PLAYLISTS
		else:
			raise ValueError("Bad namespace")
		
		nam = from_unicode(name)
		
		return self.create_result(cb, xmmsc_coll_remove(self.conn, nam, n))


	def coll_rename(self, oldname, newname, ns, cb=None):
		"""
		coll_rename(oldname, newname, ns, cb=None) -> XMMSResult

		Change the name of a Collection.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if ns == "Collections":
			n = XMMS_COLLECTION_NS_COLLECTIONS
		elif ns == "Playlists":
			n = XMMS_COLLECTION_NS_PLAYLISTS
		else:
			raise ValueError("Bad namespace")
		
		oldnam = from_unicode(oldname)
		newnam = from_unicode(newname)
		
		return self.create_result(cb, xmmsc_coll_rename(self.conn, oldnam, newnam, n))

	def coll_idlist_from_playlist_file(self, path):
		"""
		coll_idlist_from_playlist_file(path) -> XMMSResult

		Create an idlist from a playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		path = from_unicode(path)

		return self.create_result(cb, xmmsc_coll_idlist_from_playlist_file(self.conn, path))

	def coll_query_ids(self, Collection coll, start=0, leng=0, order=None, cb=None):
		"""
		coll_query_ids(coll, start=0, leng=0, order=None, cb=None) -> XMMSResult

		Retrive a list of ids of the media matching the collection
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		if order is None:
			order = []

		return self.create_result(cb, xmmsc_coll_query_ids(self.conn, coll.coll, create_native_value(order), start, leng))

	def coll_query_infos(self, Collection coll, fields, start=0, leng=0, order=None, groupby=None, cb=None):
		"""
		coll_query_infos(coll, fields, start=0, leng=0, order=None, groupby=None, cb=None) -> XMMSResult

		Retrive a list of mediainfo of the media matching the collection
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""

		if order is None:
			order = []

		if groupby is None:
			groupby = []

		return self.create_result(cb, xmmsc_coll_query_infos(self.conn, coll.coll, create_native_value(order), start, leng, create_native_value(fields), create_native_value(groupby)))


	def bindata_add(self, data, cb=None):
		"""
		bindata_add(data, cb=None) -> XMMSResult

		Add a datafile to the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef char *t
		t = data
		return self.create_result(cb, xmmsc_bindata_add(self.conn,<unsigned char *>t,len(data)))

	def bindata_retrieve(self, hash, cb=None):
		"""
		bindata_retrieve(hash, cb=None) -> XMMSResult

		Retrieve a datafile from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_bindata_retrieve(self.conn,hash))

	def bindata_remove(self, hash, cb=None):
		"""
		bindata_remove(hash, cb=None) -> XMMSResult

		Remove a datafile from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_bindata_remove(self.conn,hash))

	def bindata_list(self, cb=None):
		"""
		bindata_list(cb=None) -> XMMSResult

		List all bindata hashes stored on the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_bindata_list(self.conn))
