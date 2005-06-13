"""
Python bindings for XMMS2.
"""

cdef extern from "Python.h":
	object PyUnicode_DecodeUTF8(char *unicode, int size, char *errors)
	object PyUnicode_AsUTF8String(object o)

cdef extern from "string.h":
	int strcmp(signed char *s1, signed char *s2)

cdef extern from "xmmsc/xmmsc_idnumbers.h":
	ctypedef enum xmms_object_cmd_arg_type_t:
		XMMS_OBJECT_CMD_ARG_NONE,
		XMMS_OBJECT_CMD_ARG_UINT32,
		XMMS_OBJECT_CMD_ARG_INT32,
		XMMS_OBJECT_CMD_ARG_STRING,
		XMMS_OBJECT_CMD_ARG_DICT,
		XMMS_OBJECT_CMD_ARG_UINT32LIST,
		XMMS_OBJECT_CMD_ARG_INT32LIST,
		XMMS_OBJECT_CMD_ARG_STRINGLIST,
		XMMS_OBJECT_CMD_ARG_DICTLIST,
		XMMS_OBJECT_CMD_ARG_PLCH

# The following constants are meant for interpreting the return value of
# XMMSResult.get_type ()
OBJECT_CMD_ARG_NONE = XMMS_OBJECT_CMD_ARG_NONE
OBJECT_CMD_ARG_UINT32 = XMMS_OBJECT_CMD_ARG_UINT32
OBJECT_CMD_ARG_INT32 = XMMS_OBJECT_CMD_ARG_INT32
OBJECT_CMD_ARG_STRING = XMMS_OBJECT_CMD_ARG_STRING
OBJECT_CMD_ARG_DICT = XMMS_OBJECT_CMD_ARG_DICT
OBJECT_CMD_ARG_UINT32LIST = XMMS_OBJECT_CMD_ARG_UINT32LIST
OBJECT_CMD_ARG_INT32LIST = XMMS_OBJECT_CMD_ARG_INT32LIST
OBJECT_CMD_ARG_STRINGLIST = XMMS_OBJECT_CMD_ARG_STRINGLIST
OBJECT_CMD_ARG_DICTLIST = XMMS_OBJECT_CMD_ARG_DICTLIST
OBJECT_CMD_ARG_PLCH = XMMS_OBJECT_CMD_ARG_PLCH

cdef extern from "xmms/xmms_output.h":
	ctypedef enum xmms_output_status_t:
		XMMS_OUTPUT_STATUS_STOP,
		XMMS_OUTPUT_STATUS_PLAY,
		XMMS_OUTPUT_STATUS_PAUSE

# The following constants are meant for interpreting the return value of
# XMMS.playback_status ()
OUTPUT_STATUS_STOP = XMMS_OUTPUT_STATUS_STOP
OUTPUT_STATUS_PLAY = XMMS_OUTPUT_STATUS_PLAY
OUTPUT_STATUS_PAUSE = XMMS_OUTPUT_STATUS_PAUSE

cdef extern from "xmmsclient/xmmsclient.h":
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
	int xmmsc_result_cid(xmmsc_result_t *res)
	int xmmsc_result_get_type(xmmsc_result_t *res)

	signed int xmmsc_result_get_int(xmmsc_result_t *res, int *r)
	signed int xmmsc_result_get_uint(xmmsc_result_t *res, unsigned int *r)
	signed int xmmsc_result_get_string(xmmsc_result_t *res, signed char **r)
	signed int xmmsc_result_get_playlist_change(xmmsc_result_t *res, unsigned int *change, unsigned int *id, unsigned int *argument)

	ctypedef void(*xmmsc_foreach_func)(void *key, void *value, void *user_data)

	int xmmsc_result_get_dict_entry(xmmsc_result_t *res, char *key, char **r)
	int xmmsc_result_dict_foreach(xmmsc_result_t *res, xmmsc_foreach_func func, void *user_data)

	int xmmsc_result_list_next(xmmsc_result_t *res)
	int xmmsc_result_list_first(xmmsc_result_t *res)
	int xmmsc_result_list_valid(xmmsc_result_t *res)

	xmmsc_connection_t *xmmsc_init(char *clientname)
	void xmmsc_disconnect_callback_set(xmmsc_connection_t *c, object(*callback)(object), object userdata)
	signed int xmmsc_connect(xmmsc_connection_t *c, signed char *p)
	void xmmsc_unref(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_quit(xmmsc_connection_t *conn)

	xmmsc_result_t *xmmsc_playlist_shuffle(xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_playlist_add(xmmsc_connection_t *, char *)
	xmmsc_result_t *xmmsc_playlist_insert(xmmsc_connection_t *, int pos, char *)
	xmmsc_result_t *xmmsc_playlist_add_id(xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_remove(xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_clear(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_list(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_sort(xmmsc_connection_t *c, char *property) 
	xmmsc_result_t *xmmsc_playlist_set_next(xmmsc_connection_t *c, int pos)
	xmmsc_result_t *xmmsc_playlist_set_next_rel(xmmsc_connection_t *c, signed int)
	xmmsc_result_t *xmmsc_playlist_move(xmmsc_connection_t *c, unsigned int id, signed int movement)
	xmmsc_result_t *xmmsc_playlist_current_pos(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playlist_changed(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playlist_current_pos(xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_playback_stop(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_tickle(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_start(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_pause(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_current_id(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_seek_ms(xmmsc_connection_t *c, unsigned int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_samples(xmmsc_connection_t *c, unsigned int samples)
	xmmsc_result_t *xmmsc_playback_playtime(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_status(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playback_status(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_current_id(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_signal_playback_playtime(xmmsc_connection_t *c)


	xmmsc_result_t *xmmsc_configval_set(xmmsc_connection_t *c, char *key, char *val)
	xmmsc_result_t *xmmsc_configval_list(xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_configval_get(xmmsc_connection_t *c, char *key)

	xmmsc_result_t *xmmsc_broadcast_configval_changed(xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_medialib_select(xmmsc_connection_t *conn, char *query)
	xmmsc_result_t *xmmsc_medialib_playlist_save_current(xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_playlist_load(xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_add_entry(xmmsc_connection_t *conn, char *url)
	xmmsc_result_t *xmmsc_medialib_get_info(xmmsc_connection_t *, unsigned int id)
	xmmsc_result_t *xmmsc_medialib_add_to_playlist(xmmsc_connection_t *c, char *query)
	xmmsc_result_t *xmmsc_medialib_playlist_import(xmmsc_connection_t *c, char *name, char *url)
	xmmsc_result_t *xmmsc_medialib_playlist_export(xmmsc_connection_t *c, char *name, char *mime)
	xmmsc_result_t *xmmsc_medialib_rehash(xmmsc_connection_t *c, unsigned int)

	xmmsc_result_t *xmmsc_broadcast_medialib_entry_changed(xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_signal_visualisation_data(xmmsc_connection_t *c)

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

cdef to_unicode(char *s):
	ns = PyUnicode_DecodeUTF8(s, len(s), NULL)
	return ns

cdef from_unicode(object o):
	if isinstance(o, unicode):
		return PyUnicode_AsUTF8String(o)
	else:
		return o

cdef foreach_hash(signed char *key, signed char *value, udata):
	udata[key] = to_unicode(value)

ObjectRef = {}

cdef ResultNotifier(xmmsc_result_t *res, obj):
	obj.callback()
	if not obj.get_broadcast():
		xmmsc_result_unref(res)
		del ObjectRef[obj.get_cid()]
		
	
cdef class XMMSResult:
	"""
	Class containing the results of some operation
	"""
	cdef xmmsc_result_t *res
	cdef object notifier
	cdef object user_data
	cdef int cid
	cdef int broadcast

	def __new__(self):
		self.cid = 0

	def more_init(self, broadcast = 0):
		self.cid = xmmsc_result_cid(self.res)
		self.broadcast = broadcast
		xmmsc_result_notifier_set(self.res, ResultNotifier, self)
		ObjectRef[self.cid] = self

	def get_type (self):
		"""
		Return the type of data contained in this result.
		The return value is one of the OBJECT_CMD_ARG_* constants.
		"""
		self._check ()
	
		return xmmsc_result_get_type(self.res)

	def value(self):
		"""
		Return value of appropriate data type contained in this result.
		This can be used instead of most get_* functions in this class.
		"""
		self._check()
	
		type = xmmsc_result_get_type(self.res)

		if type == XMMS_OBJECT_CMD_ARG_UINT32:
			return self.get_uint()
		elif type == XMMS_OBJECT_CMD_ARG_DICT:
			return self.get_hashtable()
		elif type == XMMS_OBJECT_CMD_ARG_INT32:
			return self.get_int()
		elif type == XMMS_OBJECT_CMD_ARG_STRING:
			return self.get_string()
		elif type == XMMS_OBJECT_CMD_ARG_UINT32LIST:
			return self.get_uintlist()
		elif type == XMMS_OBJECT_CMD_ARG_INT32LIST:
			return self.get_intlist()
		elif type == XMMS_OBJECT_CMD_ARG_STRINGLIST:
			return self.get_stringlist()
		elif type == XMMS_OBJECT_CMD_ARG_DICTLIST:
			return self.get_hashlist()
		elif type == XMMS_OBJECT_CMD_ARG_PLCH:
			return self.get_playlistchange()

	def get_broadcast(self):
		return self.broadcast

	def get_cid(self):
		return self.cid

	def callback(self):
		""" Override me! """
		pass

	def _check(self):
		if not self.res:
			raise ValueError("The resultset did not contain a reply!")

	def wait(self):
		"""
		Wait for the result from the daemon.
		"""
		self._check()
		xmmsc_result_wait(self.res)

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

	def get_hashtable (self) : # alias for backward compatibility
		"""
		@return: A dictionary containing media info.
		"""
		return self.get_dict ()

	def get_dict (self) :
		"""
		@return: A dictionary containing media info.
		"""
		self._check()

		ret = {}
		if not xmmsc_result_dict_foreach(self.res, <xmmsc_foreach_func> foreach_hash, <void *>ret):
			raise ValueError("Failed to retrieve value!")
		return ret
			
	def get_intlist(self):
		"""
		@return: A list of ints from the result structure.
		"""
		cdef int i

		self._check()
		ret = []
		while xmmsc_result_list_valid(self.res):
			if not xmmsc_result_get_int(self.res, &i):
				raise ValueError("Failed to retrieve value!")
			ret.append(i)
			xmmsc_result_list_next(self.res)
		return ret

	def get_uintlist(self):
		"""
		@return: A list of unsigned ints from the result structure.
		"""
		cdef unsigned int i

		self._check()
		ret = []
		while xmmsc_result_list_valid(self.res):
			if not xmmsc_result_get_uint(self.res, &i):
				raise ValueError("Failed to retrieve value!")
			ret.append(i)
			xmmsc_result_list_next(self.res)
		return ret

	def get_hashlist (self) : # alias for backward compatibility
		"""
		@return: A list of dicts from the result structure.
		"""
		return self.get_dictlist ()

	def get_dictlist (self) :
		"""
		@return: A list of dicts from the result structure.
		"""
		self._check()
		ret = []
		while xmmsc_result_list_valid(self.res):
			hash = {}
			if not xmmsc_result_dict_foreach(self.res, <xmmsc_foreach_func> foreach_hash, <void *>hash):
				raise ValueError("Failed to retrieve value!")
			ret.append(hash)
			xmmsc_result_list_next(self.res)
		return ret


	def get_stringlist(self):
		"""
		@return: A list of strings from the result structure.
		"""
		cdef char *buf

		self._check()
		ret = []
		while xmmsc_result_list_valid(self.res):

			if not xmmsc_result_get_string(self.res, &buf):
				raise ValueError("Failed to retrieve value!")
			ret.append(to_unicode(buf))
			xmmsc_result_list_next(self.res)
		return ret

	def get_playlistchange(self):
		"""
		@return: a tuple with(changeid, id, argument)
		"""
		cdef unsigned int change
		cdef unsigned int id
		cdef unsigned int arg

		self._check()
		if xmmsc_result_get_playlist_change(self.res, &change, &id, &arg):
			return(change, id, arg)
		else:
			raise ValueError("Failed to retrieve value!")


	def restart(self):
		self.res = xmmsc_result_restart(self.res)
		self.cid = xmmsc_result_cid(self.res)
		ObjectRef[self.cid] = self
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

cdef class XMMS:
	"""
	This is the class representing the XMMS2 client itself. The methods in
	this class may be used to control and interact with XMMS2.
	"""
	cdef xmmsc_connection_t *conn
	cdef object loop
	cdef object wakeup
	cdef object disconnect_fun
	cdef object needout_fun

	def __new__(self, clientname = "Python XMMSClient"):
		"""
		Initiates a connection to the XMMS2 daemon. All operations
		involving the daemon are done via this connection.
		"""
		c = from_unicode(clientname)
		self.conn = xmmsc_init(c)

	def __dealloc__(self):
		""" destroys it all! """

		xmmsc_unref(self.conn)

	def _needout_cb(self, i):
		if self.needout_fun is not None:
			self.needout_fun(i)

	def _disconnect_cb(self):
		if self.disconnect_fun is not None:
			self.disconnect_fun(self)

	def exit_python_loop(self):
		""" Exits from the L{python_loop} call """
		self.loop = False
		write(self.wakeup, "42")

	def python_loop(self):
		"""
		Main client loop for most python clients. Call this to run the
		client once everything has been set up. This function blocks
		until L{exit_python_loop} is called.
		"""
		fd = xmmsc_io_fd_get(self.conn)
		(r, w) = os.pipe()

		self.loop = True
		self.wakeup = w

		while self.loop:

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
				self.loop = False

	def ioin(self):
		"""
		Read data from the daemon, when available. Note: This is a low
		level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		xmmsc_io_in_handle(self.conn)

	def ioout(self):
		"""
		Write data out to the daemon, when available. Note: This is a
		low level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		xmmsc_io_out_handle(self.conn)

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
		For example, if using L{python_loop}, your callback should call
		L{exit_python_loop} at some point.
		"""
		if path:
			ret = xmmsc_connect(self.conn, path) 
		else:
			ret = xmmsc_connect(self.conn, NULL)

		if not ret:
			raise IOError("Couldn't connect to server!")

		self.disconnect_fun = disconnect_func
		xmmsc_disconnect_callback_set(self.conn, python_disconnect_fun, self)


	def quit(self, myClass = None):
		"""
		Tell the XMMS2 daemon to quit.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret

		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		ret.res = xmmsc_quit(self.conn)
		ret.more_init()

		return ret

	def playback_start(self, myClass = None):
		"""
		Instruct the XMMS2 daemon to start playing the currently
		selected file from the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_start(self.conn)
		ret.more_init()
		
		return ret

	def playback_stop(self, myClass = None):
		"""
		Instruct the XMMS2 daemon to stop playing the file
		currently being played.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_stop(self.conn)
		ret.more_init()
		
		return ret

	def playback_tickle(self, myClass = None):
		"""
		Instruct the XMMS2 daemon to move on to the next playlist item.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_tickle(self.conn)
		ret.more_init()
		
		return ret

	def playback_pause(self, myClass = None):
		"""
		Instruct the XMMS2 daemon to pause playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_pause(self.conn)
		ret.more_init()
		
		return ret

	def playback_current_id(self, myClass = None):
		"""
		@rtype: L{XMMSResult}(UInt)
		@return: The medialib id of the item currently selected.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_current_id(self.conn)
		ret.more_init()
		
		return ret

	def playback_seek_ms(self, ms, myClass = None):
		"""
		Seek to an absolute time position in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_seek_ms(self.conn, ms)
		ret.more_init()
		
		return ret

	def playback_seek_samples(self, samples, myClass = None):
		"""
		Seek to an absolute number of samples in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_seek_samples(self.conn, samples)
		ret.more_init()
		
		return ret

	def playback_status(self, myClass = None):
		"""Get current playback status from XMMS2 daemon. This is
		essentially the more direct version of
		L{broadcast_playback_status}.
		@rtype: L{XMMSResult}(UInt)
		@return: Current playback status(UInt)
		"""
		cdef XMMSResult ret
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		ret.res = xmmsc_playback_status(self.conn)
		ret.more_init()
		return ret

	def broadcast_playback_status(self, myClass = None):
		"""
		Set a class to handle the playback status broadcast from the
		XMMS2 daemon. Note: the handler class is usually a child of the
		XMMSResult class. Updated data is sent when playback status
		changes.
		@rtype: L{XMMSResult}(UInt)
		@return: An XMMSResult object that is constantly updated with
		the appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_broadcast_playback_status(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_playback_current_id(self, myClass = None):
		"""
		Set a class to handle the playback id broadcast from the
		XMMS2 daemon. Note: the handler class is usually a child of the
		XMMSResult class. Updated data is sent when the mlib id of the
		current item changes.
		@rtype: L{XMMSResult}(UInt)
		@return: An XMMSResult object that is constantly updated with
		the appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_broadcast_playback_current_id(self.conn)
		ret.more_init(1)
		
		return ret

	def playback_playtime(self, myClass = None):
		"""
		Return playtime on current file/stream. This is essentially a
		more direct version of L{signal_playback_playtime}
		@rtype: L{XMMSResult}(UInt)
		@return: The result of the operation.(playtime in milliseconds)
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playback_playtime(self.conn)
		ret.more_init()

		return ret

	def signal_playback_playtime(self, myClass = None):
		"""
		Set a class to handle the playback playtime signal from the
		XMMS2 daemon. This can be used to keep track of the amount of
		time played on the current file/stream. Note: the handler
		class is usually a child of the XMMSResult class.
		@rtype: L{XMMSResult}(UInt)
		@return: The result of the operation.(playtime in milliseconds)
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_signal_playback_playtime(self.conn)
		ret.more_init()
		
		return ret

	def playlist_shuffle(self, myClass = None):
		"""
		Instruct the XMMS2 daemon to shuffle the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_shuffle(self.conn)
		ret.more_init()
		
		return ret

	def playlist_insert(self, pos, url, myClass = None):
		"""
		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a int 'pos' and a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_insert(self.conn, pos, c)
		ret.more_init()
		
		return ret

	def playlist_add(self, url, myClass = None):
		"""
		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(url)
		
		ret.res = xmmsc_playlist_add(self.conn, c)
		ret.more_init()
		
		return ret

	def playlist_add_id(self, id, myClass = None):
		"""
		Add a medialib id to the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_add_id(self.conn, id)
		ret.more_init()
		
		return ret


	def playlist_remove(self, id, myClass = None):
		"""
		Remove a certain media item from the playlist.
		Requires a number 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_remove(self.conn, id)
		ret.more_init()
		
		return ret

	def playlist_clear(self, myClass = None):
		"""
		Clear the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_clear(self.conn)
		ret.more_init()
		
		return ret

	def playlist_list(self, myClass = None):
		"""
		Get the current playlist. This function returns a list of IDs
		of the files/streams currently in the playlist. Use
		L{medialib_get_info} to retrieve more specific information.
		@rtype: L{XMMSResult}(UIntList)
		@return: The current playlist.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_list(self.conn)
		ret.more_init()
		
		return ret


	def playlist_sort(self, prop, myClass = None):
		"""
		Sorts the playlist according to the property specified.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(prop)
		
		ret.res = xmmsc_playlist_sort(self.conn, c)
		ret.more_init()
		
		return ret

	def playlist_set_next_rel(self, position, myClass = None):
		"""
		Sets the position in the playlist. Same as set_next but
		does it relative. You can do set_next_rel(-1) to move backwards
		for example
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_set_next_rel(self.conn, position)
		ret.more_init()
		
		return ret


	def playlist_set_next(self, position, myClass = None):
		"""
		Sets the position to move to, next, in the playlist. Calling
		L{playback_tickle} will perform the jump to that position.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_set_next(self.conn, position)
		ret.more_init()
		
		return ret

	def playlist_move(self, id, movement, myClass = None):
		"""
		Move a playlist entry relative to it's current position in
		the playlist. The movement should be a postive value when
		moving down in the playlist and a negative value when moving
		up in the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_move(self.conn, id, movement)
		ret.more_init()
		
		return ret

	def playlist_current_pos(self, myClass = None):
		"""
		Returns the current position in the playlist. This value will
		always be equal to, or larger than 0. The first entry in the
		list is 0.
		"""
		cdef XMMSResult ret

		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_playlist_current_pos(self.conn)
		ret.more_init()

		return ret

	def broadcast_playlist_current_pos(self, myClass = None):
		"""
		Set a class to handle the playlist current position updates 
		from the XMMS2 daemon. Note: the handler class is usually a
		child of the XMMSResult class. Updated data is sent when the
		current item changes position.(e.g. finish playing song 1,
		switch to song 2 & start playing) This is NOT when moving a
		playlist item from one position to another.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_broadcast_playlist_current_pos(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_playlist_changed(self, myClass = None):
		"""
		Set a class to handle the playlist changed broadcast from the
		XMMS2 daemon.(i.e. the player's playlist has changed) Note:
		the handler class is usually a child of the XMMSResult class.
		Updated data is sent whenever the playlist is modified.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_broadcast_playlist_changed(self.conn)
		ret.more_init(1)
		
		return ret

	def broadcast_configval_changed(self, myClass = None):
		"""
		Set a class to handle the config value changed broadcast
		from the XMMS2 daemon.(i.e. some configuration value has
		been modified) Note: the handler class is usually a child of
		the XMMSResult class. Updated data is sent whenever a config
		value is modified.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info.(the modified config key and its value)
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		ret.res = xmmsc_broadcast_configval_changed(self.conn)
		ret.more_init(1)

		return ret

	def configval_set(self, key, val, myClass = None):
		"""
		Set a configuration value on the daemon, given a key.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c1 = from_unicode(key)
		c2 = from_unicode(val)
		
		ret.res = xmmsc_configval_set(self.conn, c1, c2)
		ret.more_init()
		return ret

	def configval_get(self, key, myClass = None):
		"""
		Get the configuration value of a given key, from the daemon.
		@rtype: L{XMMSResult}(String)
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(key)
		
		ret.res = xmmsc_configval_get(self.conn, c)
		ret.more_init()
		return ret

	def configval_list(self, myClass = None):
		"""
		Get list of configuration keys on the daemon. Use
		L{configval_get} to retrieve the values corresponding to the
		configuration keys.
		@rtype: L{XMMSResult}(StringList)
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_configval_list(self.conn)
		ret.more_init()
		return ret

	def medialib_select(self, query, myClass = None):
		"""
		Query the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(query)
		
		ret.res = xmmsc_medialib_select(self.conn, c)
		ret.more_init()
		return ret

	def medialib_add_entry(self, file, myClass = None):
		"""
		Add an entry to the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(file)
		
		ret.res = xmmsc_medialib_add_entry(self.conn, c)
		ret.more_init()
		return ret

	def medialib_playlist_save_current(self, playlistname, myClass = None):
		"""
		Save the current playlist to a medialib playlist
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(playlistname)
		
		ret.res = xmmsc_medialib_playlist_save_current(self.conn, c)
		ret.more_init()
		return ret

	def medialib_playlist_load(self, playlistname, myClass = None):
		"""
		Load playlistname from the medialib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(playlistname)
		
		ret.res = xmmsc_medialib_playlist_load(self.conn, c)
		ret.more_init()
		return ret

	def medialib_get_info(self, id, myClass = None):
		"""
		@rtype: L{XMMSResult}(HashTable)
		@return: Information about the medialib entry
		position specified.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_medialib_get_info(self.conn, id)
		ret.more_init()
		
		return ret

	def medialib_add_to_playlist(self, query, myClass = None):
		"""
		Add items in the playlist by querying the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(query)
		
		ret.res = xmmsc_medialib_add_to_playlist(self.conn, c)
		ret.more_init()
		
		return ret

	def medialib_playlist_import(self, name, url, myClass = None):
		"""
		Import a playlist to the medialib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(name)
		c2 = from_unicode(url)
		
		ret.res = xmmsc_medialib_playlist_import(self.conn, c, c2)
		ret.more_init()
		
		return ret

	def medialib_rehash(self, id = 0, myClass = None):
		"""
		Force metadata info update on medialib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_medialib_rehash(self.conn, id)
		ret.more_init()
		
		return ret


	def medialib_playlist_export(self, name, mime, myClass = None):
		"""
		Export a playlist from medialib to another format
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()

		c = from_unicode(name)
		c2 = from_unicode(mime)
		
		ret.res = xmmsc_medialib_playlist_export(self.conn, c, c2)
		ret.more_init()
		
		return ret

	def broadcast_medialib_entry_changed(self, myClass = None):
		"""
		Set a class to handle the medialib entry changed broadcast
		from the XMMS2 daemon.(i.e. the current entry in the playlist
		has changed) Note: the handler class is usually a child of the
		XMMSResult class. Updated data is sent when the metadata for
		a song is updated in the medialib.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_broadcast_medialib_entry_changed(self.conn)
		ret.more_init(1)
		
		return ret



	def signal_visualisation_data(self, myClass = None):
		"""
		Tell server to send you VisData updates.
		For drawing peek analyzer.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass:
			ret = myClass()
		else:
			ret = XMMSResult()
		
		ret.res = xmmsc_signal_visualisation_data(self.conn)
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
	def __init__(self, xmms):
		"""
		This constructor takes a single argument which specifies the
		XMMS object to wrap.
		"""
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
