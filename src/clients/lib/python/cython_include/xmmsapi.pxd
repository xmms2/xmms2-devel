from xmmsvalue cimport Collection
from cxmmsvalue cimport *
from cxmmsclient cimport *

cdef bint ResultNotifier(xmmsv_t *res, void *o)
cdef void ResultDestroyNotifier(void *o)

from xmmsutils cimport *
cdef inline char *check_playlist(object playlist):
	if playlist is None:
		return NULL
	else:
		return to_charp(from_unicode(playlist))

cdef inline char *check_namespace(object ns, bint can_be_all) except NULL:
	cdef char *n
	if ns == "Collections":
		n = <char *>XMMS_COLLECTION_NS_COLLECTIONS
	elif ns == "Playlists":
		n = <char *>XMMS_COLLECTION_NS_PLAYLISTS
	elif can_be_all and ns == "*":
		n = <char *>XMMS_COLLECTION_NS_ALL
	else:
		raise ValueError("Bad namespace")
	return n

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
	cdef object _exc
	cdef XmmsSourcePreference source_pref
	cdef int ispropdict
	cdef XmmsResultTracker result_tracker

	cdef set_sourcepref(self, XmmsSourcePreference sourcepref)
	cdef set_result(self, xmmsc_result_t *res)
	cdef set_callback(self, XmmsResultTracker rt, cb)

	cpdef disconnect(self)
	cpdef wait(self)
	cpdef is_error(self)
	cpdef iserror(self)
	cpdef xmmsvalue(self)
	cpdef _value(self)
	cpdef value(self)

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
	cdef XmmsResult create_result(self, cb, xmmsc_result_t *res)

cdef void python_need_out_fun(int i, void *obj)
cdef void python_disconnect_fun(void *obj)
cpdef userconfdir_get()

cdef class XmmsApi(XmmsCore):
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
	cpdef XmmsResult playlist_load(self, playlist=*, cb=*)
	cpdef XmmsResult playlist_list(self, cb=*)
	cpdef XmmsResult playlist_remove(self, playlist=*, cb=*)
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
	cpdef XmmsResult medialib_get_info(self, int id, cb=*)
	cpdef XmmsResult medialib_rehash(self, int id=*, cb=*)
	cpdef XmmsResult medialib_get_id(self, url, cb=*, encoded=*)
	cpdef XmmsResult medialib_import_path(self, path, cb=*, encoded=*)
	cpdef XmmsResult medialib_property_set(self, int id, key, value, source=*, cb=*)
	cpdef XmmsResult medialib_property_remove(self, int id, key, source=*, cb=*)
	cpdef XmmsResult broadcast_medialib_entry_added(self, cb=*)
	cpdef XmmsResult broadcast_medialib_entry_changed(self, cb=*)
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
	cpdef XmmsResult coll_query_ids(self, Collection coll, start=*, leng=*, order=*, cb=*)
	cpdef XmmsResult coll_query_infos(self, Collection coll, fields, start=*, leng=*, order=*, groupby=*, cb=*)
	cpdef XmmsResult bindata_add(self, data, cb=*)
	cpdef XmmsResult bindata_retrieve(self, hash, cb=*)
	cpdef XmmsResult bindata_remove(self, hash, cb=*)
	cpdef XmmsResult bindata_list(self, cb=*)

cdef class XmmsLoop(XmmsApi):
	cdef bint do_loop
	cdef object wakeup

