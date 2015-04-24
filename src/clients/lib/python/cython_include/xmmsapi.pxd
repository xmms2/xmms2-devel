from xmmsvalue cimport Collection, XmmsValue
from cxmmsvalue cimport *
from cxmmsclient cimport *

cdef bint ResultNotifier(xmmsv_t *res, void *o)
cdef void ResultDestroyNotifier(void *o)

cdef class XmmsSourcePreference:
	cdef object sources

cdef class XmmsResult
cdef class XmmsResultTracker:
	cdef object results

	cdef track_result(self, XmmsResult r)
	cdef release_result(self, XmmsResult r)
	cdef disconnect_all(self, bint unset_result)

cdef class XmmsResult:
	cdef xmmsc_result_t *res
	cdef object _cb
	cdef bint _cb_issetup
	cdef XmmsSourcePreference source_pref
	cdef int ispropdict
	cdef XmmsResultTracker result_tracker

	cdef set_sourcepref(self, XmmsSourcePreference sourcepref)
	cdef set_result(self, xmmsc_result_t *res)
	cdef set_callback(self, XmmsResultTracker rt, cb)

	cpdef disconnect(self)
	cpdef wait(self)
	cpdef is_error(self)
	cpdef xmmsvalue(self)
	cpdef value(self)

ctypedef int VisResultCommand
cdef enum:
	VIS_RESULT_CMD_NONE = 0
	VIS_RESULT_CMD_INIT = 1
	VIS_RESULT_CMD_START = 2

cdef class XmmsVisResult(XmmsResult):
	cdef XmmsValue _val
	cdef VisResultCommand command
	cdef xmmsc_connection_t *conn

	cdef set_command(self, VisResultCommand cmd, xmmsc_connection_t *conn)
	cdef retrieve_error(self)
	cdef _init_xmmsvalue(self)
	cdef _start_xmmsvalue(self)
	cpdef xmmsvalue(self)

cdef class XmmsVisChunk:
	cdef short *data
	cdef int sample_count

	cdef set_data(self, short *data, int sample_count)
	cpdef get_buffer(self)
	cpdef get_data(self)

cdef class XmmsCore
cdef class XmmsServiceNamespace:
	cdef readonly XmmsCore xmms
	cdef readonly XmmsServiceNamespace parent
	cdef bint registered
	cdef object _bound_methods

	cpdef _walk(self, namespace=*, constant=*, method=*, broadcast=*, path=*, other=*, depth=*, deep=*)
	cpdef register_namespace(self, path, instance, infos)
	cpdef register_constant(self, path, instance, infos)
	cpdef register_method(self, path, instance, infos)
	cpdef register_broadcast(self, path, instance, infos)
	cpdef register(self)

cdef class service_broadcast:
	cdef public object name
	cdef public object doc
	cdef readonly XmmsServiceNamespace namespace
	cdef bind(self, XmmsServiceNamespace namespace, name=*)
	cpdef emit(self, value=*)

cdef xmmsv_t *service_method_proxy(xmmsv_t *pargs, xmmsv_t *nargs, void *udata)
cdef xmmsc_sc_namespace_t *_namespace_get(xmmsc_connection_t *c, path, bint create)

cdef class _XmmsServiceClient
cdef class client_broadcast:
	cdef XmmsCore _xmms
	cdef bint _async
	cdef readonly object _path
	cdef readonly int _clientid
	cdef readonly object name
	cdef readonly object docstring

cdef class client_method:
	cdef _XmmsServiceClient _parent
	cdef object _callback
	cdef readonly object _path
	cdef readonly int _clientid
	cdef readonly object name
	cdef readonly object docstring
	cdef readonly object inspect

cdef class _XmmsServiceClient:
	cdef readonly object _path
	cdef readonly bint _async
	cdef readonly XmmsCore _xmms
	cdef readonly int _clientid

cdef class XmmsProxy:
	cdef XmmsCore _get_xmms(self)

cdef class XmmsCore:
	cdef xmmsc_connection_t *conn
	cdef int isconnected
	cdef object disconnect_fun
	cdef object needout_fun
	cdef readonly XmmsSourcePreference source_preference
	cdef XmmsResultTracker result_tracker
	cdef readonly object clientname

	cdef new_connection(self)
	cpdef get_source_preference(self)
	cpdef set_source_preference(self, sources)
	cpdef _needout_cb(self, int i)
	cpdef _disconnect_cb(self)
	cpdef disconnect(self)
	cpdef ioin(self)
	cpdef ioout(self)
	cpdef want_ioout(self)
	cpdef set_need_out_fun(self, fun)
	cpdef get_fd(self)
	cpdef connect(self, path=*, disconnect_func=*)
	cdef XmmsResult _create_result(self, cb, xmmsc_result_t *res, Cls)
	cdef XmmsResult create_result(self, cb, xmmsc_result_t *res)
	cdef XmmsResult create_vis_result(self, cb, xmmsc_result_t *res, VisResultCommand cmd)

cdef void python_need_out_fun(int i, void *obj)
cdef void python_disconnect_fun(void *obj)
cpdef userconfdir_get()

cdef class XmmsApi(XmmsCore):
	cpdef int c2c_get_own_id(self)
	cpdef XmmsResult quit(self, cb=*)
	cpdef XmmsResult plugin_list(self, typ, cb=*)
	cpdef XmmsResult playback_start(self, cb=*)
	cpdef XmmsResult playback_stop(self, cb=*)
	cpdef XmmsResult playback_tickle(self, cb=*)
	cpdef XmmsResult playback_pause(self, cb=*)
	cpdef XmmsResult playback_current_id(self, cb=*)
	cpdef XmmsResult playback_seek_ms(self, int ms, xmms_playback_seek_mode_t whence=*, cb=*)
	cpdef XmmsResult playback_seek_samples(self, int samples, xmms_playback_seek_mode_t whence=*, cb=*)
	cpdef XmmsResult playback_status(self, cb=*)
	cpdef XmmsResult broadcast_playback_status(self, cb=*)
	cpdef XmmsResult broadcast_playback_current_id(self, cb=*)
	cpdef XmmsResult playback_playtime(self, cb=*)
	cpdef XmmsResult signal_playback_playtime(self, cb=*)
	cpdef XmmsResult playback_volume_set(self, channel, int volume, cb=*)
	cpdef XmmsResult playback_volume_get(self, cb=*)
	cpdef XmmsResult broadcast_playback_volume_changed(self, cb=*)
	cpdef XmmsResult broadcast_playlist_loaded(self, cb=*)
	cpdef XmmsResult playlist_load(self, playlist, cb=*)
	cpdef XmmsResult playlist_list(self, cb=*)
	cpdef XmmsResult playlist_remove(self, playlist, cb=*)
	cpdef XmmsResult playlist_shuffle(self, playlist=*, cb=*)
	cpdef XmmsResult playlist_rinsert(self, int pos, url, playlist=*, cb=*, encoded=*)
	cpdef XmmsResult playlist_insert_url(self, int pos, url, playlist=*, cb=*, encoded=*)
	cpdef XmmsResult playlist_insert_id(self, int pos, int id, playlist=*, cb=*)
	cpdef XmmsResult playlist_insert_collection(self, int pos, Collection coll, order=*, playlist=*, cb=*)
	cpdef XmmsResult playlist_radd(self, url, playlist=*, cb=*, encoded=*)
	cpdef XmmsResult playlist_add_url(self, url, playlist=*, cb=*, encoded=*)
	cpdef XmmsResult playlist_add_id(self, int id, playlist=*, cb=*)
	cpdef XmmsResult playlist_add_collection(self, Collection coll, order=*, playlist=*, cb=*)
	cpdef XmmsResult playlist_remove_entry(self, int id, playlist=*, cb=*)
	cpdef XmmsResult playlist_clear(self, playlist=*, cb=*)
	cpdef XmmsResult playlist_list_entries(self, playlist=*, cb=*)
	cpdef XmmsResult playlist_sort(self, props, playlist=*, cb=*)
	cpdef XmmsResult playlist_set_next_rel(self, int position, cb=*)
	cpdef XmmsResult playlist_set_next(self, int position, cb=*)
	cpdef XmmsResult playlist_move(self, int cur_pos, int new_pos, playlist=*, cb=*)
	cpdef XmmsResult playlist_create(self, playlist, cb=*)
	cpdef XmmsResult playlist_current_pos(self, playlist=*, cb=*)
	cpdef XmmsResult playlist_current_active(self, cb=*)
	cpdef XmmsResult broadcast_playlist_current_pos(self, cb=*)
	cpdef XmmsResult broadcast_playlist_changed(self, cb=*)
	cpdef XmmsResult broadcast_config_value_changed(self, cb=*)
	cpdef XmmsResult config_set_value(self, key, val, cb=*)
	cpdef XmmsResult config_get_value(self, key, cb=*)
	cpdef XmmsResult config_list_values(self, cb=*)
	cpdef XmmsResult config_register_value(self, valuename, defaultvalue, cb=*)
	cpdef XmmsResult medialib_add_entry(self, path, cb=*, encoded=*)
	cpdef XmmsResult medialib_remove_entry(self, int id, cb=*)
	cpdef XmmsResult medialib_move_entry(self, int id, url, cb=*, encoded=*)
	cpdef XmmsResult medialib_get_info(self, int id, cb=*)
	cpdef XmmsResult medialib_rehash(self, int id=*, cb=*)
	cpdef XmmsResult medialib_get_id(self, url, cb=*, encoded=*)
	cpdef XmmsResult medialib_import_path(self, path, cb=*, encoded=*)
	cpdef XmmsResult medialib_property_set(self, int id, key, value, source=*, cb=*)
	cpdef XmmsResult medialib_property_remove(self, int id, key, source=*, cb=*)
	cpdef XmmsResult broadcast_medialib_entry_added(self, cb=*)
	cpdef XmmsResult broadcast_medialib_entry_updated(self, cb=*)
	cpdef XmmsResult broadcast_medialib_entry_removed(self, cb=*)
	cpdef XmmsResult broadcast_collection_changed(self, cb=*)
	cpdef XmmsResult signal_mediainfo_reader_unindexed(self, cb=*)
	cpdef XmmsResult broadcast_mediainfo_reader_status(self, cb=*)
	cpdef XmmsResult xform_media_browse(self, url, cb=*, encoded=*)
	cpdef XmmsResult coll_get(self, name, ns=*, cb=*)
	cpdef XmmsResult coll_list(self, ns=*, cb=*)
	cpdef XmmsResult coll_save(self, Collection coll, name, ns=*, cb=*)
	cpdef XmmsResult coll_remove(self, name, ns=*, cb=*)
	cpdef XmmsResult coll_rename(self, oldname, newname, ns=*, cb=*)
	cpdef XmmsResult coll_idlist_from_playlist_file(self, path, cb=*)
	cpdef XmmsResult coll_query(self, Collection coll, fetch, cb=*)
	cpdef XmmsResult coll_query_ids(self, Collection coll, start=*, leng=*, order=*, cb=*)
	cpdef XmmsResult coll_query_infos(self, Collection coll, fields, start=*, leng=*, order=*, groupby=*, cb=*)
	#C2C
	#Low level API (internal ?) that should not be used directly.
	#cdef xmmsc_c2c_reply_policy_t get_c2c_policy(self, reply_policy=*)
	#cpdef XmmsResult c2c_send(self, int dest, payload, reply_policy=*, cb=*)
	#cpdef XmmsResult c2c_reply(self, int msgid, payload, reply_policy=*, cb=*)
	#cpdef XmmsResult broadcast_c2c_message(self, cb=*)
	cpdef XmmsResult c2c_ready(self, cb=*)
	cpdef XmmsResult c2c_get_connected_clients(self, cb=*)
	cpdef XmmsResult c2c_get_ready_clients(self, cb=*)
	cpdef XmmsResult broadcast_c2c_ready(self, cb=*)
	cpdef XmmsResult broadcast_c2c_client_connected(self, cb=*)
	cpdef XmmsResult broadcast_c2c_client_disconnected(self, cb=*)
	#Services
	cpdef bint sc_init(self)
	cpdef bint sc_broadcast_emit(self, broadcast, value=*)
	cpdef XmmsResult sc_broadcast_subscribe(self, int dest, broadcast, cb=*)
	cpdef XmmsResult sc_call(self, int dest, method, args=*, kargs=*, cb=*)
	cpdef XmmsResult sc_introspect_namespace(self, int dest, path=*, cb=*)
	cpdef XmmsResult sc_introspect_method(self, int dest, path, cb=*)
	cpdef XmmsResult sc_introspect_broadcast(self, int dest, path, cb=*)
	cpdef XmmsResult sc_introspect_constant(self, int dest, path, cb=*)
	cpdef XmmsResult sc_introspect_docstring(self, int dest, path, cb=*)
	#
	cpdef XmmsResult bindata_add(self, data, cb=*)
	cpdef XmmsResult bindata_retrieve(self, hash, cb=*)
	cpdef XmmsResult bindata_remove(self, hash, cb=*)
	cpdef XmmsResult bindata_list(self, cb=*)
	cpdef XmmsResult stats(self, cb=*)
	cpdef XmmsResult visualization_version(self, cb=*)
	cpdef XmmsResult visualization_init(self, cb=*)
	cpdef XmmsResult visualization_start(self, int handle, cb=*)
	cpdef bint visualization_started(self, int handle)
	cpdef bint visualization_errored(self, int handle)
	cpdef XmmsResult visualization_property_set(self, int handle, key, value, cb=*)
	cpdef XmmsResult visualization_properties_set(self, int handle, props=*, cb=*)
	cpdef XmmsVisChunk visualization_chunk_get(self, int handle, int drawtime=*, bint blocking=*)
	cpdef visualization_shutdown(self, int handle)

cdef class XmmsLoop(XmmsApi):
	cdef bint do_loop
	cdef object wakeup
