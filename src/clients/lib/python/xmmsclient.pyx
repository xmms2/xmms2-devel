"""
Python bindings for XMMS2.
"""

cdef extern from "stdlib.h":
	void *malloc(int size)

cdef extern from "Python.h":
	object PyUnicode_DecodeUTF8(char *unicode, int size, char *errors)
	object PyUnicode_AsUTF8String(object o)
	object PyString_FromStringAndSize(char *, int)

cdef extern from "string.h":
	int strcmp(signed char *s1, signed char *s2)

cdef extern from "xmmsc/xmmsc_idnumbers.h":
	ctypedef enum xmms_object_cmd_arg_type_t:
		XMMS_OBJECT_CMD_ARG_NONE,
		XMMS_OBJECT_CMD_ARG_UINT32,
		XMMS_OBJECT_CMD_ARG_INT32,
		XMMS_OBJECT_CMD_ARG_STRING,
		XMMS_OBJECT_CMD_ARG_DICT,
		XMMS_OBJECT_CMD_ARG_LIST,
		XMMS_OBJECT_CMD_ARG_PROPDICT,
		XMMS_OBJECT_CMD_ARG_BIN


# The following constants are meant for interpreting the return value of
# XMMSResult.get_type ()
OBJECT_CMD_ARG_NONE = XMMS_OBJECT_CMD_ARG_NONE
OBJECT_CMD_ARG_UINT32 = XMMS_OBJECT_CMD_ARG_UINT32
OBJECT_CMD_ARG_INT32 = XMMS_OBJECT_CMD_ARG_INT32
OBJECT_CMD_ARG_STRING = XMMS_OBJECT_CMD_ARG_STRING
OBJECT_CMD_ARG_DICT = XMMS_OBJECT_CMD_ARG_DICT
OBJECT_CMD_ARG_LIST = XMMS_OBJECT_CMD_ARG_LIST
OBJECT_CMD_ARG_BIN = XMMS_OBJECT_CMD_ARG_BIN

cdef extern from "xmmsc/xmmsc_idnumbers.h":
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
		XMMS_PLAYLIST_CHANGED_SORT

	ctypedef enum xmms_plugin_type_t:
		XMMS_PLUGIN_TYPE_ALL,
		XMMS_PLUGIN_TYPE_XFORM,
		XMMS_PLUGIN_TYPE_OUTPUT,
		XMMS_PLUGIN_TYPE_PLAYLIST,
		XMMS_PLUGIN_TYPE_EFFECT

cdef extern from "xmms/xmms_defs.h":
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

PLUGIN_TYPE_ALL = XMMS_PLUGIN_TYPE_ALL
PLUGIN_TYPE_XFORM = XMMS_PLUGIN_TYPE_XFORM
PLUGIN_TYPE_OUTPUT = XMMS_PLUGIN_TYPE_OUTPUT
PLUGIN_TYPE_PLAYLIST = XMMS_PLUGIN_TYPE_PLAYLIST
PLUGIN_TYPE_EFFECT = XMMS_PLUGIN_TYPE_EFFECT

cdef extern from "xmmsclient/xmmsclient.h":
	ctypedef enum xmmsc_result_value_type_t:
		XMMSC_RESULT_VALUE_TYPE_NONE,
		XMMSC_RESULT_VALUE_TYPE_UINT32,
		XMMSC_RESULT_VALUE_TYPE_INT32,
		XMMSC_RESULT_VALUE_TYPE_STRING,
		XMMSC_RESULT_VALUE_TYPE_BIN

	ctypedef struct xmmsc_connection_t:
		pass
	ctypedef struct xmmsc_result_t
	ctypedef object(*xmmsc_result_notifier_t)(xmmsc_result_t *res, object user_data)

	xmmsc_result_t *xmmsc_result_restart(xmmsc_result_t *res)
	void xmmsc_result_ref(xmmsc_result_t *res)
	void xmmsc_result_unref(xmmsc_result_t *res)
	void xmmsc_result_notifier_set(xmmsc_result_t *res, xmmsc_result_notifier_t func, object user_data)
	void xmmsc_result_wait(xmmsc_result_t *res)
	signed int xmmsc_result_iserror(xmmsc_result_t *res)
	signed char *xmmsc_result_get_error(xmmsc_result_t *res)
	int xmmsc_result_get_type(xmmsc_result_t *res)

	signed int xmmsc_result_get_int(xmmsc_result_t *res, int *r)
	signed int xmmsc_result_get_uint(xmmsc_result_t *res, unsigned int *r)
	signed int xmmsc_result_get_string(xmmsc_result_t *res, signed char **r)
	signed int xmmsc_result_get_bin(xmmsc_result_t *res, unsigned char **r, unsigned int *rlen)
	signed int xmmsc_result_get_playlist_change(xmmsc_result_t *res, unsigned int *change, unsigned int *id, unsigned int *argument)

	ctypedef void(*xmmsc_dict_foreach_func)(void *key, xmmsc_result_value_type_t type, void *value, void *user_data)
	ctypedef void(*xmmsc_propdict_foreach_func)(void *key, xmmsc_result_value_type_t type, void *value, char *source, void *user_data)

	int xmmsc_result_get_dict_entry(xmmsc_result_t *res, char *key, char **r)
	int xmmsc_result_dict_foreach(xmmsc_result_t *res, xmmsc_dict_foreach_func func, void *user_data)
	int xmmsc_result_propdict_foreach(xmmsc_result_t *res, xmmsc_propdict_foreach_func func, void *user_data)

	int xmmsc_result_is_list(xmmsc_result_t *res)
	int xmmsc_result_list_next(xmmsc_result_t *res)
	int xmmsc_result_list_first(xmmsc_result_t *res)
	int xmmsc_result_list_valid(xmmsc_result_t *res)

	xmmsc_connection_t *xmmsc_init(char *clientname)
	void xmmsc_disconnect_callback_set(xmmsc_connection_t *c, object(*callback)(object), object userdata)
	signed int xmmsc_connect(xmmsc_connection_t *c, signed char *p)
	void xmmsc_unref(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_plugin_list (xmmsc_connection_t *c, unsigned int type)

	void xmmsc_result_disconnect(xmmsc_result_t *res)

	xmmsc_result_t *xmmsc_playlist_shuffle(xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_playlist_add(xmmsc_connection_t *, char *)
	xmmsc_result_t *xmmsc_playlist_add_encoded(xmmsc_connection_t *, char *)
	xmmsc_result_t *xmmsc_playlist_insert(xmmsc_connection_t *, int pos, char *)
	xmmsc_result_t *xmmsc_playlist_add_id(xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_insert_id(xmmsc_connection_t *, int pos, unsigned int)
	xmmsc_result_t *xmmsc_playlist_insert_encoded(xmmsc_connection_t *, int pos, char *)
	xmmsc_result_t *xmmsc_playlist_remove(xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_clear(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_list(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_sort(xmmsc_connection_t *c, char *property)
	xmmsc_result_t *xmmsc_playlist_set_next(xmmsc_connection_t *c, int pos)
	xmmsc_result_t *xmmsc_playlist_set_next_rel(xmmsc_connection_t *c, signed int)
	xmmsc_result_t *xmmsc_playlist_move(xmmsc_connection_t *c, unsigned int id, signed int movement)
	xmmsc_result_t *xmmsc_playlist_current_pos(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_radd(xmmsc_connection_t *c, char *path)
	xmmsc_result_t *xmmsc_playlist_radd_encoded(xmmsc_connection_t *c, char *path)

	xmmsc_result_t *xmmsc_broadcast_playlist_changed(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playlist_current_pos(xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_playback_stop(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_tickle(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_start(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_pause(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_current_id(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_seek_ms(xmmsc_connection_t *c, unsigned int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_ms_rel(xmmsc_connection_t *c, int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_samples(xmmsc_connection_t *c, unsigned int samples)
	xmmsc_result_t *xmmsc_playback_seek_samples_rel(xmmsc_connection_t *c, int samples)
	xmmsc_result_t *xmmsc_playback_playtime(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_status(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playback_status(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_current_id(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_signal_playback_playtime(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_playback_volume_set (xmmsc_connection_t *c, char *channel, unsigned int volume)
	xmmsc_result_t *xmmsc_playback_volume_get (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_volume_changed (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_configval_set(xmmsc_connection_t *c, char *key, char *val)
	xmmsc_result_t *xmmsc_configval_list(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_configval_get(xmmsc_connection_t *c, char *key)
	xmmsc_result_t *xmmsc_configval_register(xmmsc_connection_t *c, char *valuename, char *defaultvalue)

	xmmsc_result_t *xmmsc_broadcast_configval_changed(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_medialib_select(xmmsc_connection_t *conn, char *query)
	char *xmmsc_sqlite_prepare_string(char *query)
	xmmsc_result_t *xmmsc_medialib_playlist_save_current(xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_playlist_load(xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_add_entry(xmmsc_connection_t *conn, char *url)
	xmmsc_result_t *xmmsc_medialib_add_entry_encoded(xmmsc_connection_t *conn, char *url)
	xmmsc_result_t *xmmsc_medialib_get_info(xmmsc_connection_t *, unsigned int id)
	xmmsc_result_t *xmmsc_medialib_add_to_playlist(xmmsc_connection_t *c, char *query)
	xmmsc_result_t *xmmsc_medialib_playlists_list (xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_medialib_playlist_list (xmmsc_connection_t *, char *playlist)
	xmmsc_result_t *xmmsc_medialib_playlist_import(xmmsc_connection_t *c, char *name, char *url)
	xmmsc_result_t *xmmsc_medialib_playlist_export(xmmsc_connection_t *c, char *name, char *mime)
	xmmsc_result_t *xmmsc_medialib_playlist_remove (xmmsc_connection_t *c, char *name)
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
	xmmsc_result_t *xmmsc_bindata_add (xmmsc_connection_t *c, char *, int len)
	xmmsc_result_t *xmmsc_bindata_retrieve (xmmsc_connection_t *c, char *hash)
	xmmsc_result_t *xmmsc_bindata_remove (xmmsc_connection_t *c, char *hash)

	xmmsc_result_t *xmmsc_broadcast_medialib_entry_added(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_medialib_entry_changed(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_medialib_playlist_loaded(xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_signal_visualisation_data(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_mediainfo_reader_status (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_signal_mediainfo_reader_unindexed (xmmsc_connection_t *c)

	char *xmmsc_userconfdir_get (char *buf, int len)

	void xmmsc_io_need_out_callback_set(xmmsc_connection_t *c, object(*callback)(int, object), object userdata)
	void xmmsc_io_disconnect(xmmsc_connection_t *c)
	int xmmsc_io_want_out(xmmsc_connection_t *c)
	int xmmsc_io_out_handle(xmmsc_connection_t *c)
	int xmmsc_io_in_handle(xmmsc_connection_t *c)
	int xmmsc_io_fd_get(xmmsc_connection_t *c)


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

cdef foreach_source_hash(signed char *key, xmmsc_result_value_type_t type, void *value, char *source, udata):
	if type == XMMSC_RESULT_VALUE_TYPE_STRING:
		v = to_unicode(<char *>value)
	elif type == XMMSC_RESULT_VALUE_TYPE_UINT32:
		v = <unsigned int>value
	elif type == XMMSC_RESULT_VALUE_TYPE_INT32:
		v = <int>value

	udata[(source,key)]=v

cdef foreach_hash(signed char *key, xmmsc_result_value_type_t type, void *value, udata):
	if type == XMMSC_RESULT_VALUE_TYPE_STRING:
		v = to_unicode(<char *>value)
	elif type == XMMSC_RESULT_VALUE_TYPE_UINT32:
		v = <unsigned int>value
	elif type == XMMSC_RESULT_VALUE_TYPE_INT32:
		v = <int>value

	udata[key]=v


cdef ResultNotifier(xmmsc_result_t *res, obj):
	if not obj.get_broadcast():
		obj._del_ref()
	try:
		obj._cb()
	finally:
		if not obj.get_broadcast():
			xmmsc_result_unref(res)

class PropDict(dict):
	def __init__(self, srcs):
		dict.__init__(self)
		self._sources = srcs

	def set_source_preference(self, sources):
		"""
		Change list of source preference
		This method has been deprecated and should no longer be used.
		"""
		raise DeprecationWarning("This method has been deprecated and should no longer be used. Set the sources list using the 'sources' property.")
		self._set_sources(sources)

	def has_key(self, item):
		try:
			self.__getitem__(item)
			return True
		except KeyError:
			return False

	def __contains__(self, item):
		return self.has_key(item)

	def __getitem__(self, item):
		if isinstance(item, basestring):
			for src in self._sources:
				if src.endswith('*'):
					for k,v in self.iteritems():
						if k[0].startswith(src[:-1]) and k[1] == item:
							return v
				try:
					t = dict.__getitem__(self, (src, item))
					return t
				except KeyError:
					pass
			raise KeyError, item
		return dict.__getitem__(self, item)

	def get(self, item, default=None):
		try:
			return self[item]
		except KeyError:
			return default
	
	def _get_sources(self):
		return self._sources
	def _set_sources(self, val):
		if not isinstance(val, list):
			raise TypeError("Need a list of sources")
		for i in val:
			if not isinstance(i, basestring):
				raise TypeError("Sources need to be strings")
		self._sources = val
	sources = property(_get_sources, _set_sources)
	
cdef class XMMSResult:
	"""
	Class containing the results of some operation
	"""
	cdef xmmsc_result_t *res
	cdef xmmsc_result_t *orig
	cdef object notifier
	cdef object user_data
	cdef int broadcast
	cdef object callback
	cdef object c
	cdef object exc

	def __new__(self, c):
		self.c = c
		self.exc = None

	def more_init(self, broadcast = 0):
		self.orig = self.res
		self.broadcast = broadcast
		xmmsc_result_notifier_set(self.res, ResultNotifier, self)
		self.c._add_ref(self)

	def _del_ref(self):
		self.c._del_ref(self)

	def _cb(self):
		self._check()
		if not self.callback:
			return
		try:
			self.callback(self)
		except:
			exc = sys.exc_info()
			traceback.print_exception (exc[0], exc[1], exc[2])

	def get_type(self):
		"""
		Return the type of data contained in this result.
		The return value is one of the OBJECT_CMD_ARG_* constants.
		"""
		self._check ()
	
		return xmmsc_result_get_type(self.res)

	def _value(self):
		type = xmmsc_result_get_type(self.res)

		if type == XMMS_OBJECT_CMD_ARG_UINT32:
			return self.get_uint()
		elif type == XMMS_OBJECT_CMD_ARG_DICT:
			return self.get_dict()
		elif type == XMMS_OBJECT_CMD_ARG_PROPDICT:
			return self.get_propdict()
		elif type == XMMS_OBJECT_CMD_ARG_INT32:
			return self.get_int()
		elif type == XMMS_OBJECT_CMD_ARG_STRING:
			return self.get_string()
		elif type == XMMS_OBJECT_CMD_ARG_BIN:
			return self.get_bin()

	def value(self):
		"""
		Return value of appropriate data type contained in this result.
		This can be used instead of most get_* functions in this class.
		"""
		self._check()
		
		if xmmsc_result_is_list(self.res):
			return self.get_list()
		else:
			return self._value()
	

	def get_broadcast(self):
		return self.broadcast

	def _check(self):
		if not self.res:
			raise ValueError("The resultset did not contain a reply!")

	def wait(self):
		"""
		Wait for the result from the daemon.
		"""
		self._check()
		xmmsc_result_wait(self.res)
		if self.exc is not None:
			raise self.exc[0], self.exc[1], self.exc[2]

	def disconnect(self):
		""" @todo: Fail if this result isn't a signal or a broadcast """
		xmmsc_result_disconnect(self.orig)

	def get_int(self):
		"""
		Get data from the result structure as an int.
		@rtype: int
		"""
		cdef signed int ret
		self._check()
		if xmmsc_result_get_int(self.res, &ret):
			return ret
		else:
			raise ValueError("Failed to retrieve value!")

	def get_uint(self):
		"""
		Get data from the result structure as an unsigned int.
		@rtype: uint
		"""
		cdef unsigned int ret
		self._check()
		if xmmsc_result_get_uint(self.res, &ret):
			return ret
		else:
			raise ValueError("Failed to retrieve value!")

	def get_string(self):
		"""
		Get data from the result structure as a string.
		@rtype: string
		"""
		cdef signed char *ret

		self._check()
		if xmmsc_result_get_string(self.res, &ret):
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

		self._check()
		if xmmsc_result_get_bin(self.res, &ret, &rlen):
			return PyString_FromStringAndSize(<char *>ret, rlen)
		else:
			raise ValueError("Failed to retrieve value!")

	def get_dict (self) :
		"""
		@return: A dictionary containing media info.
		"""
		self._check()
		
		ret = {}
		if not xmmsc_result_dict_foreach(self.res, <xmmsc_dict_foreach_func> foreach_hash, <void *>ret):
			raise ValueError("Failed to retrieve value!")
		return ret

	def get_propdict(self):
		"""
		@return: A source dict.
		"""
		self._check()
		ret = PropDict(self.c.get_source_preference())
		if not xmmsc_result_propdict_foreach(self.res, <xmmsc_propdict_foreach_func> foreach_source_hash, <void *>ret):
			raise ValueError("Failed to retrieve value!")
		return ret
			
	def get_list (self) :
		"""
		@return: A list of dicts from the result structure.
		"""
		self._check()
		ret = []
		while xmmsc_result_list_valid(self.res):
			ret.append(self._value())
			xmmsc_result_list_next(self.res)
		return ret

	def restart(self):
		self.res = xmmsc_result_restart(self.res)
		self.c._add_ref(self)
		xmmsc_result_unref(self.res)

	def iserror(self):
		"""
		@return: Whether the result represents an error or not.
		@rtype: Boolean
		"""
		return xmmsc_result_iserror(self.res)

	def get_error(self):
		"""
		@return: Error string from the result.
		@rtype: String
		"""
		if xmmsc_result_get_error(self.res) == NULL:
			return None
		else:
			return xmmsc_result_get_error(self.res)

	def __dealloc__(self):
		"""
		Deallocate the result.
		"""

		if self.res:
			xmmsc_result_unref(self.res)

cdef python_need_out_fun(int i, obj):
	obj._needout_cb(i)

cdef python_disconnect_fun(obj):
	obj._disconnect_cb()

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
	cdef object ObjectRef
	cdef object sources

	def __new__(self, clientname = None):
		"""
		Initiates a connection to the XMMS2 daemon. All operations
		involving the daemon are done via this connection.
		"""
		if clientname is None:
			clientname = "UnnamedPythonClient"
		c = from_unicode(clientname)
		self.conn = xmmsc_init(c)
		self.ObjectRef = []
		self.sources = ["client/" + clientname, "server", "plugins/*", "client/*", "*"]

	def get_source_preference(self):
		return self.sources
	def set_source_preference(self, sources):
		self.sources = sources

	def _add_ref(self, res):
		self.ObjectRef.append(res)
	
	def _del_ref(self, res):
		self.ObjectRef.remove(res)

	def __dealloc__(self):
		""" destroys it all! """

		xmmsc_unref(self.conn)

	def _needout_cb(self, i):
		if self.needout_fun is not None:
			self.needout_fun(i)

	def _disconnect_cb(self):
		if self.disconnect_fun is not None:
			self.disconnect_fun(self)

	def exit_loop(self):
		""" Exits from the L{loop} call """
		self.do_loop = False
		write(self.wakeup, "42")

	def loop(self):
		"""
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
		Read data from the daemon, when available. Note: This is a low
		level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_in_handle(self.conn)

	def ioout(self):
		"""
		Write data out to the daemon, when available. Note: This is a
		low level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_out_handle(self.conn)

	def want_ioout(self):
		return xmmsc_io_want_out(self.conn)

	def set_need_out_fun(self, fun):
		xmmsc_io_need_out_callback_set(self.conn, python_need_out_fun, self)
		self.needout_fun = fun
		
	def get_fd(self):
		"""
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
		xmmsc_disconnect_callback_set(self.conn, python_disconnect_fun, self)


	def quit(self, cb = None):
		"""
		Tell the XMMS2 daemon to quit.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_quit(self.conn)
		ret.more_init()

		return ret

	def plugin_list(self, type, cb = None):
		"""
		Get a list of loaded plugins from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_plugin_list(self.conn, type)
		ret.more_init()
		return ret

	def playback_start(self, cb = None):
		"""
		Instruct the XMMS2 daemon to start playing the currently
		selected file from the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_playback_start(self.conn)
		ret.more_init()
		
		return ret

	def playback_stop(self, cb = None):
		"""
		Instruct the XMMS2 daemon to stop playing the file
		currently being played.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_playback_stop(self.conn)
		ret.more_init()
		
		return ret

	def playback_tickle(self, cb = None):
		"""
		Instruct the XMMS2 daemon to move on to the next playlist item.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_playback_tickle(self.conn)
		ret.more_init()
		
		return ret

	def playback_pause(self, cb = None):
		"""
		Instruct the XMMS2 daemon to pause playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_pause(self.conn)
		ret.more_init()
		
		return ret

	def playback_current_id(self, cb = None):
		"""
		@rtype: L{XMMSResult}(UInt)
		@return: The medialib id of the item currently selected.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_current_id(self.conn)
		ret.more_init()
		
		return ret

	def playback_seek_ms(self, ms, cb = None):
		"""
		Seek to an absolute time position in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_seek_ms(self.conn, ms)
		ret.more_init()
		
		return ret

	def playback_seek_ms_rel(self, ms, cb = None):
		"""
		Seek to a time position by the given offset in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_seek_ms_rel(self.conn, ms)
		ret.more_init()
		
		return ret

	def playback_seek_samples(self, samples, cb = None):
		"""
		Seek to an absolute number of samples in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_seek_samples(self.conn, samples)
		ret.more_init()
		
		return ret

	def playback_seek_samples_rel(self, samples, cb = None):
		"""
		Seek to a number of samples by the given offset in the
		current file or stream in playback.
                @rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_seek_samples_rel(self.conn, samples)
		ret.more_init()
		
		return ret

	def playback_status(self, cb = None):
		"""Get current playback status from XMMS2 daemon. This is
		essentially the more direct version of
		L{broadcast_playback_status}. Possible return values are:
		L{PLAYBACK_STATUS_STOP}, L{PLAYBACK_STATUS_PLAY},
		L{PLAYBACK_STATUS_PAUSE}
		@rtype: L{XMMSResult}(UInt)
		@return: Current playback status(UInt)
		"""
		cdef XMMSResult ret
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_playback_status(self.conn)
		ret.more_init()
		return ret

	def broadcast_playback_status(self, cb = None):
		"""
		Set a method to handle the playback status broadcast from the
		XMMS2 daemon.
		@rtype: L{XMMSResult}(UInt)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_playback_status(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_playback_current_id(self, cb = None):
		"""
		Set a method to handle the playback id broadcast from the
		XMMS2 daemon.
		@rtype: L{XMMSResult}(UInt)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_playback_current_id(self.conn)
		ret.more_init(1)
		
		return ret

	def playback_playtime(self, cb = None):
		"""
		Return playtime on current file/stream. This is essentially a
		more direct version of L{signal_playback_playtime}
		@rtype: L{XMMSResult}(UInt)
		@return: The result of the operation.(playtime in milliseconds)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_playtime(self.conn)
		ret.more_init()

		return ret

	def signal_playback_playtime(self, cb = None):
		"""
		Set a method to handle the playback playtime signal from the
		XMMS2 daemon.
		@rtype: L{XMMSResult}(UInt)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_signal_playback_playtime(self.conn)
		ret.more_init()
		
		return ret

	def playback_volume_set(self, channel, volume, cb = None):
		"""
		Set the playback volume for specified channel
		@rtype: L{XMMSResult}(UInt)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_volume_set(self.conn, channel, volume)
		ret.more_init()
		
		return ret

	def playback_volume_get(self, cb = None):
		"""
		Get the playback for all channels
		@rtype: L{XMMSResult}(UInt)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playback_volume_get(self.conn)
		ret.more_init()
		
		return ret

	def broadcast_playback_volume_changed(self, cb = None):
		"""
		Set a broadcast callback for volume updates
		@rtype: L{XMMSResult}(UInt)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_playback_volume_changed(self.conn)
		ret.more_init(1)
		
		return ret

	def playlist_shuffle(self, cb = None):
		"""
		Instruct the XMMS2 daemon to shuffle the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_shuffle(self.conn)
		ret.more_init()
		
		return ret

	def playlist_insert(self, pos, url, cb = None):
		"""
		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_insert(self.conn, pos, c)
		ret.more_init()
		
		return ret

	def playlist_insert_encoded(self, pos, url, cb = None):
		"""
		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires an int 'pos' and a string 'url' as argument.

		The 'url' should be encoded to this function.

		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_insert_encoded(self.conn, pos, c)
		ret.more_init()
		
		return ret


	def playlist_insert_id(self, pos, id, cb = None):
		"""
		Insert a medialib to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_playlist_insert_id(self.conn, pos, id)
		ret.more_init()
		
		return ret

	def playlist_radd(self, url, cb = None):
		"""
		Add a directory to the playlist.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_radd(self.conn, c)
		ret.more_init()
		
		return ret

	def playlist_radd_encoded(self, url, cb = None):
		"""
		Add a directory to the playlist.
		Requires a string 'url' as argument.
		'url' argument has to be medialib encoded.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_radd_encoded(self.conn, c)
		ret.more_init()
		
		return ret


	def playlist_add(self, url, cb = None):
		"""
		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_add(self.conn, c)
		ret.more_init()
		
		return ret

	def playlist_add_encoded(self, url, cb = None):
		"""
		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		The 'url' has to be medialib encoded.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_add_encoded(self.conn, c)
		ret.more_init()
		
		return ret


	def playlist_add_id(self, id, cb = None):
		"""
		Add a medialib id to the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_add_id(self.conn, id)
		ret.more_init()
		
		return ret


	def playlist_remove(self, id, cb = None):
		"""
		Remove a certain media item from the playlist.
		Requires a number 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_remove(self.conn, id)
		ret.more_init()
		
		return ret

	def playlist_clear(self, cb = None):
		"""
		Clear the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_clear(self.conn)
		ret.more_init()
		
		return ret

	def playlist_list(self, cb = None):
		"""
		Get the current playlist. This function returns a list of IDs
		of the files/streams currently in the playlist. Use
		L{medialib_get_info} to retrieve more specific information.
		@rtype: L{XMMSResult}(UIntList)
		@return: The current playlist.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_list(self.conn)
		ret.more_init()
		
		return ret


	def playlist_sort(self, prop, cb = None):
		"""
		Sorts the playlist according to the property specified.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(prop)
		
		ret.res = xmmsc_playlist_sort(self.conn, c)
		ret.more_init()
		
		return ret

	def playlist_set_next_rel(self, position, cb = None):
		"""
		Sets the position in the playlist. Same as L{playlist_set_next}
		but sets the next position relative to the current position.
		You can do set_next_rel(-1) to move backwards for example.
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_set_next_rel(self.conn, position)
		ret.more_init()
		
		return ret


	def playlist_set_next(self, position, cb = None):
		"""
		Sets the position to move to, next, in the playlist. Calling
		L{playback_tickle} will perform the jump to that position.
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_set_next(self.conn, position)
		ret.more_init()
		
		return ret

	def playlist_move(self, cur_pos, new_pos, cb = None):
		"""
		Moves a playlist entry to a new position.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_move(self.conn, cur_pos, new_pos)
		ret.more_init()
		
		return ret

	def playlist_current_pos(self, cb = None):
		"""
		Returns the current position in the playlist. This value will
		always be equal to, or larger than 0. The first entry in the
		list is 0.
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_playlist_current_pos(self.conn)
		ret.more_init()

		return ret

	def broadcast_playlist_current_pos(self, cb = None):
		"""
		Set a method to handle the playlist current position updates
		from the XMMS2 daemon. This is triggered whenever the daemon
		jumps from one playlist position to another. (not when moving
		a playlist item from one position to another)
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_playlist_current_pos(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_playlist_changed(self, cb = None):
		"""
		Set a method to handle the playlist changed broadcast from the
		XMMS2 daemon. Updated data is sent whenever the daemon's
		playlist changes.
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_playlist_changed(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_configval_changed(self, cb = None):
		"""
		Set a method to handle the config value changed broadcast
		from the XMMS2 daemon.(i.e. some configuration value has
		been modified) Updated data is sent whenever a config
		value is modified.
		@rtype: L{XMMSResult} (the modified config key and its value)
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_broadcast_configval_changed(self.conn)
		ret.more_init(1)

		return ret

	def configval_set(self, key, val, cb = None):
		"""
		Set a configuration value on the daemon, given a key.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c1 = from_unicode(key)
		c2 = from_unicode(val)
		
		ret.res = xmmsc_configval_set(self.conn, c1, c2)
		ret.more_init()
		return ret

	def configval_get(self, key, cb = None):
		"""
		Get the configuration value of a given key, from the daemon.
		@rtype: L{XMMSResult}(String)
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(key)
		
		ret.res = xmmsc_configval_get(self.conn, c)
		ret.more_init()
		return ret

	def configval_list(self, cb = None):
		"""
		Get list of configuration keys on the daemon. Use
		L{configval_get} to retrieve the values corresponding to the
		configuration keys.
		@rtype: L{XMMSResult}(StringList)
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_configval_list(self.conn)
		ret.more_init()
		return ret

	def configval_register(self, valuename, defaultvalue, cb = None):
		"""
		Register a new configvalue.
		This should be called in the initcode as XMMS2 won't allow
		set/get on values that haven't been registered.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c1 = from_unicode(valuename)
		c2 = from_unicode(defaultvalue)
		
		ret.res = xmmsc_configval_register(self.conn, c1, c2)
		ret.more_init()
		return ret

	def medialib_select(self, query, cb = None):
		"""
		Query the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(query)
		
		ret.res = xmmsc_medialib_select(self.conn, c)
		ret.more_init()
		return ret

	def prepare_string(self, value):
		"""
		Prepare a string for SQL queries.
		@rtype: string
		@return: The escaped string enclosed by quotations.
		"""
		return xmmsc_sqlite_prepare_string(value)

	def medialib_add_entry(self, file, cb = None):
		"""
		Add an entry to the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(file)
		
		ret.res = xmmsc_medialib_add_entry(self.conn, c)
		ret.more_init()
		return ret

	def medialib_add_entry_encoded(self, file, cb = None):
		"""
		Add an entry to the MediaLib.
		Exactly the same as #medialib_add_entry but takes
		a encoded url instead.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(file)
		
		ret.res = xmmsc_medialib_add_entry(self.conn, c)
		ret.more_init()
		return ret

	def medialib_playlist_save_current(self, playlistname, cb = None):
		"""
		Save the current playlist to a medialib playlist
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(playlistname)
		
		ret.res = xmmsc_medialib_playlist_save_current(self.conn, c)
		ret.more_init()
		return ret

	def medialib_playlist_load(self, playlistname, cb = None):
		"""
		Load playlistname from the medialib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(playlistname)
		
		ret.res = xmmsc_medialib_playlist_load(self.conn, c)
		ret.more_init()
		return ret

	def medialib_get_info(self, id, cb = None):
		"""
		@rtype: L{XMMSResult}(HashTable)
		@return: Information about the medialib entry position
		specified.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_medialib_get_info(self.conn, id)
		ret.more_init()
		
		return ret

	def medialib_add_to_playlist(self, query, cb = None):
		"""
		Add items in the playlist by querying the medialib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(query)
		
		ret.res = xmmsc_medialib_add_to_playlist(self.conn, c)
		ret.more_init()
		
		return ret

	def medialib_playlists_list(self, cb = None):
		"""
		Returns a list of all available playlists in the medialib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_medialib_playlists_list(self.conn)
		ret.more_init()
		
		return ret

	def medialib_playlist_list(self, name, cb = None):
		"""
		Get the specified playlist from medialib.
		This function returns a list of IDs the files/streams
		currently in the playlist. Use L{medialib_get_info} to
		retrieve more specific information.
		@rtype:	L{XMMSResult}(UIntList)
		@return: The playlist with the given name.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_medialib_playlist_list(self.conn, name)
		ret.more_init()
		
		return ret

	def medialib_playlist_import(self, name, url, cb = None):
		"""
		Import a playlist into the medialib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(name)
		c2 = from_unicode(url)
		
		ret.res = xmmsc_medialib_playlist_import(self.conn, c, c2)
		ret.more_init()
		
		return ret

	def medialib_rehash(self, id = 0, cb = None):
		"""
		Force the medialib to check that metadata stored is up to
		date.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_medialib_rehash(self.conn, id)
		ret.more_init()
		
		return ret

	def medialib_get_id(self, url, cb = None):
		"""
		Search for an entry (URL) in the medialib and return its ID
		number.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_medialib_get_id(self.conn, url)
		ret.more_init()
		
		return ret


	def medialib_playlist_export(self, name, mime, cb = None):
		"""
		Export a playlist from the medialib to another format.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(name)
		c2 = from_unicode(mime)
		
		ret.res = xmmsc_medialib_playlist_export(self.conn, c, c2)
		ret.more_init()
		
		return ret

	def medialib_playlist_remove(self, name, cb = None):
		"""
		Remove a playlist from the medialib. This does not affect the
		songs listed in the playlist itself.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(name)
		
		ret.res = xmmsc_medialib_playlist_remove(self.conn, c)
		ret.more_init()
		
		return ret

	def medialib_path_import(self, path, cb = None):
		"""
		Import metadata from all files recursively from the directory
		passed as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(path)
		
		ret.res = xmmsc_medialib_path_import(self.conn, c)
		ret.more_init()
		
		return ret

	def medialib_path_import_encoded(self, path, cb = None):
		"""
		Import metadata from all files recursively from the directory
		passed as argument. The 'path' argument has to be medialib encoded.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		c = from_unicode(path)
		
		ret.res = xmmsc_medialib_path_import_encoded(self.conn, c)
		ret.more_init()
		
		return ret


	def medialib_property_set(self, id, key, value, source=None, cb=None):
		"""
		Associate a value with a medialib entry. Source is optional.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb

		k = from_unicode(key)
		v = from_unicode(value)
	
		if source:
			s = from_unicode(source)
			if isinstance(value, int):
				ret.res = xmmsc_medialib_entry_property_set_int_with_source(self.conn,id,s,k,v)
			else:
				ret.res = xmmsc_medialib_entry_property_set_str_with_source(self.conn,id,s,k,v)
		else:
			if isinstance(value, str):
				ret.res = xmmsc_medialib_entry_property_set_str(self.conn,id,k,v)
			else:
				ret.res = xmmsc_medialib_entry_property_set_int(self.conn,id,k,v)

		ret.more_init()
		return ret

	def medialib_property_remove(self, id, key, source=None, cb=None):
		"""
		Remove a value from a medialib entry. Source is optional.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		k = from_unicode(key)

		if source:
			s = from_unicode(source)
			ret.res = xmmsc_medialib_entry_property_remove_with_source(self.conn,id,s,k)
		else:
			ret.res = xmmsc_medialib_entry_property_remove(self.conn,id,k)

		ret.more_init()
		return ret

	def broadcast_medialib_entry_added(self, cb = None):
		"""
		Set a method to handle the medialib entry added broadcast
		from the XMMS2 daemon. (i.e. a new entry has been added)
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_broadcast_medialib_entry_added(self.conn)
		ret.more_init(1)
	
		return ret

	def broadcast_medialib_entry_changed(self, cb = None):
		"""
		Set a method to handle the medialib entry changed broadcast
		from the XMMS2 daemon.
		Updated data is sent when the metadata for a song is updated
		in the medialib.
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_medialib_entry_changed(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_medialib_playlist_loaded(self, cb = None):
		"""
		Set a method to handle the medialib playlist loaded broadcast
		from the XMMS2 daemon.(i.e. a playlist is loaded from medialib). 		
		@rtype: L{XMMSResult}
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		
		ret.res = xmmsc_broadcast_medialib_playlist_loaded(self.conn)
		ret.more_init(1)
		
		return ret


	def signal_visualisation_data(self, cb = None):
		"""
		Tell daemon to send you visualisation data updates for drawing
		peak analyzer.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_signal_visualisation_data(self.conn)
		ret.more_init()
		return ret

	def signal_mediainfo_reader_unindexed(self, cb = None):
		"""
		Tell daemon to send you the number of unindexed files in the mlib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_signal_mediainfo_reader_unindexed(self.conn)
		ret.more_init()
		return ret

	def broadcast_mediainfo_reader_status(self, cb = None):
		"""
		Tell daemon to send you the status of the mediainfo reader
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		ret = XMMSResult(self)
		ret.callback = cb
		ret.res = xmmsc_broadcast_mediainfo_reader_status(self.conn)
		ret.more_init(1)
		return ret

	def xform_media_browse(self, url, cb=None):
		"""
		Browse files from xform plugins.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		u = from_unicode(url)

		ret.res = xmmsc_xform_media_browse(self.conn,u)

		ret.more_init()
		return ret

	def bindata_add(self, data, cb=None):
		"""
		Add a datafile to the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_bindata_add(self.conn,data,len(data))

		ret.more_init()
		return ret

	def bindata_retrieve(self, hash, cb=None):
		"""
		Retrieve a datafile from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_bindata_retrieve(self.conn,hash)

		ret.more_init()
		return ret

	def bindata_remove(self, hash, cb=None):
		"""
		Remove a datafile from the server
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		ret = XMMSResult(self)
		ret.callback = cb

		ret.res = xmmsc_bindata_remove(self.conn,hash)

		ret.more_init()
		return ret


class XMMSSync:
	"""
	A wrapper for the xmmsclient.XMMS class which simplifies synchronous
	communication with the XMMS2 daemon.
	
	Instances of this class may be used just like regular xmmsclient.XMMS
	objects, except that instead of returning an XMMSResult instance, the
	value associated with the result is returned.  If the XMMSResult
	indicates an error, an XMMSError is raised instead of returning the
	value.
	"""
	# This implementation avoids using nested function definitions, as they
	# are not supported by Pyrex.
	def __init__(self, clientname=None, xmms=None):
		"""
		This constructor takes two optional arguments. If xmms is omitted
		it will create a new underlying XMMS class otherwise it will use
		the one supplied. Clientname is the name of the client and will
		default to "Unnamed Python Client"
		"""
		if not xmms:
			self.__xmms = XMMS(clientname)
		else:
			self.__xmms = xmms

	def __getattr__(self, name):
		attr = getattr(self.__xmms, name)
		if callable(attr):
			return XMMSSyncMethod(attr)
		else:
			return attr


class XMMSSyncMethod:
	"""
	A factory which uses a bound XMMS object method to create a callable
	object that wraps the XMMS method for more convenient synchronous use.
	This is meant for use by the XMMSSync class.
	"""
	def __init__(self, method):
		self.method = method

	def __call__(self, *args):
		ret = self.method(*args)
		if isinstance(ret, XMMSResult):
			ret.wait()
			if ret.iserror():
				raise XMMSError(ret.get_error())
			return ret.value()
		return ret


class XMMSError(Exception):
	"""
	Thrown when a synchronous method call on an XMMS client object fails.
	"""
	pass
