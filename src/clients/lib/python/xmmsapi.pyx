"""
Python bindings for XMMS2.
"""

# CImports
from xmmsutils cimport from_unicode, to_charp
from cxmmsvalue cimport *
from cxmmsclient cimport *
from xmmsvalue cimport *

# The following constants are meant for interpreting the return value of
# XMMS.playback_status ()
PLAYBACK_STATUS_STOP  = XMMS_PLAYBACK_STATUS_STOP
PLAYBACK_STATUS_PLAY  = XMMS_PLAYBACK_STATUS_PLAY
PLAYBACK_STATUS_PAUSE = XMMS_PLAYBACK_STATUS_PAUSE

PLAYLIST_CHANGED_ADD     = XMMS_PLAYLIST_CHANGED_ADD
PLAYLIST_CHANGED_INSERT  = XMMS_PLAYLIST_CHANGED_INSERT
PLAYLIST_CHANGED_SHUFFLE = XMMS_PLAYLIST_CHANGED_SHUFFLE
PLAYLIST_CHANGED_REMOVE  = XMMS_PLAYLIST_CHANGED_REMOVE
PLAYLIST_CHANGED_CLEAR   = XMMS_PLAYLIST_CHANGED_CLEAR
PLAYLIST_CHANGED_MOVE    = XMMS_PLAYLIST_CHANGED_MOVE
PLAYLIST_CHANGED_SORT    = XMMS_PLAYLIST_CHANGED_SORT
PLAYLIST_CHANGED_UPDATE  = XMMS_PLAYLIST_CHANGED_UPDATE

PLUGIN_TYPE_ALL    = XMMS_PLUGIN_TYPE_ALL
PLUGIN_TYPE_XFORM  = XMMS_PLUGIN_TYPE_XFORM
PLUGIN_TYPE_OUTPUT = XMMS_PLUGIN_TYPE_OUTPUT

MEDIALIB_ENTRY_STATUS_NEW           = XMMS_MEDIALIB_ENTRY_STATUS_NEW
MEDIALIB_ENTRY_STATUS_OK            = XMMS_MEDIALIB_ENTRY_STATUS_OK
MEDIALIB_ENTRY_STATUS_RESOLVING     = XMMS_MEDIALIB_ENTRY_STATUS_RESOLVING
MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE = XMMS_MEDIALIB_ENTRY_STATUS_NOT_AVAILABLE
MEDIALIB_ENTRY_STATUS_REHASH        = XMMS_MEDIALIB_ENTRY_STATUS_REHASH

COLLECTION_CHANGED_ADD    = XMMS_COLLECTION_CHANGED_ADD
COLLECTION_CHANGED_UPDATE = XMMS_COLLECTION_CHANGED_UPDATE
COLLECTION_CHANGED_RENAME = XMMS_COLLECTION_CHANGED_RENAME
COLLECTION_CHANGED_REMOVE = XMMS_COLLECTION_CHANGED_REMOVE

PLAYBACK_SEEK_CUR = XMMS_PLAYBACK_SEEK_CUR
PLAYBACK_SEEK_SET = XMMS_PLAYBACK_SEEK_SET

COLLECTION_NS_COLLECTIONS = <char *>XMMS_COLLECTION_NS_COLLECTIONS
COLLECTION_NS_PLAYLISTS = <char *>XMMS_COLLECTION_NS_PLAYLISTS
COLLECTION_NS_ALL = <char *>XMMS_COLLECTION_NS_ALL

#####################################################################

cdef int show_deprecated = 0
from os import getenv
if getenv('XMMS_PYTHON_SHOW_DEPRECATED'):
	show_deprecated = 1
del getenv

def deprecated(f):
	def deprecated_decorator(*a, **kw):
		if show_deprecated:
			from sys import stderr
			print >>stderr, "DEPRECATED: %s()" % f.__name__
		return f(*a, **kw)
	return deprecated_decorator

# Trick to not expose select in the module, but keep it accessible from builtins
# methods.
cdef object select
cdef _install_select():
	global select
	from select import select as _sel
	select = _sel
_install_select()

cdef bint ResultNotifier(xmmsv_t *res, void *o):
	cdef object xres
	xres = <object> o
	try:
		return xres()
	except:
		import traceback, sys
		exc = sys.exc_info()
		traceback.print_exception(*exc)
		return False

cdef void ResultDestroyNotifier(void *o):
	cdef XmmsResult obj
	obj = <XmmsResult>o
	obj._cb = None
	obj.result_tracker.release_result(obj)

cdef class XmmsSourcePreference:
	"""
	Source preference manager
	"""
	#cdef object sources

	def __init__(self, sources=None):
		self.set(sources)

	def get(self):
		return self.sources

	def set(self, sources):
		if sources is None:
			self.sources = []
		else:
			self.sources = map(enforce_unicode, [s for s in sources])

cdef class XmmsResultTracker:
	"""
	Class used by XmmsCore to track results that set a notifier.
	"""
	#cdef object results

	def __cinit__(self):
		self.results = []

	cdef track_result(self, XmmsResult r):
		self.results.append(r)

	cdef release_result(self, XmmsResult r):
		self.results.remove(r)

	cdef disconnect_all(self, bint unset_result):
		cdef XmmsResult r
		for r in self.results[:]: # Create a copy because r.disconnect() will remove the result from the list.
			r.disconnect()
			if unset_result: #This would become useless if results didn't xmmsc_ref() the connection.
				xmmsc_result_unref(r.res)
				r.res = NULL

cdef class XmmsResult:
	"""
	Class containing the results of some operation
	"""
	#cdef xmmsc_result_t *res
	#cdef object _cb
	#cdef bint _cb_issetup
	#cdef object _exc
	#cdef XmmsSourcePreference source_pref
	#cdef int ispropdict
	#cdef XmmsResultTracker result_tracker

	def __cinit__(self):
		self.res = NULL
		self.ispropdict = 0
		self._cb_issetup = False

	def __dealloc__(self):
		if self.res != NULL:
			xmmsc_result_unref(self.res)
			self.res = NULL

	cdef set_sourcepref(self, XmmsSourcePreference sourcepref):
		self.source_pref = sourcepref
	cdef set_result(self, xmmsc_result_t *res):
		self.res = res
	cdef set_callback(self, XmmsResultTracker rt, cb):
		"""
		Set a callback function for a result
		"""
		if cb is not None and not hasattr(cb, '__call__'):
			raise TypeError("Type '%s' is not callable"%cb.__class__.__name__)
		self._cb = cb
		if cb is not None and not self._cb_issetup:
			self.result_tracker = rt
			rt.track_result(self)
			xmmsc_result_notifier_set_full(self.res, ResultNotifier, <void *> self, ResultDestroyNotifier)
			self._cb_issetup = True

	cpdef disconnect(self):
		self._cb = None
		if self._cb_issetup and self.res != NULL:
			self._cb_issetup = False
			#xmmsc_result_ref(self.res) # Needed ? oO
			xmmsc_result_disconnect(self.res) #self.result_tracker.release_result() called in ResultDestroyNotifier()

	property callback:
		def __get__(self):
			return self._cb

	# XXX Kept for compatibility.
	@deprecated
	def _callback(self):
		"""
		@deprecated
		Use __call__ instead.
		"""
		try:
			ret = self()
		except:
			import traceback, sys
			exc = sys.exc_info()
			traceback.print_exception(exc[0], exc[1], exc[2])
			return False
		return ret

	def __call__(self):
		cb = self.callback
		if cb is not None:
			ret = cb(self.xmmsvalue())
			return True if ret is None else ret
		return False

	cpdef wait(self):
		"""
		Wait for the result from the daemon.
		"""
		if self.res == NULL:
			raise RuntimeError("Uninitialized result")

		xmmsc_result_wait(self.res)

		# XXX Never used.
		if self._exc is not None:
			raise self._exc[0], self._exc[1], self._exc[2]

	cpdef is_error(self):
		"""
		@return: Whether the result represents an error or not.
		@rtype: Boolean
		"""
		return self._value().is_error()

	cpdef iserror(self):
		return self.is_error()

	cpdef xmmsvalue(self):
		cdef XmmsValue obj
		cdef xmmsv_t *value
		if self.res == NULL:
			raise RuntimeError("Uninitialized result")
		value = xmmsc_result_get_value(self.res)
		obj = XmmsValue(self.source_pref.get())
		obj.set_value(value, self.ispropdict)
		return obj

	cpdef _value(self):
		return self.xmmsvalue()

	cpdef value(self):
		return self.xmmsvalue().value()


cdef void python_need_out_fun(int i, void *obj):
	cdef object o
	o = <object> obj
	o._needout_cb(i)

cdef void python_disconnect_fun(void *obj):
	cdef object o
	o = <object> obj
	o._disconnect_cb()

cpdef userconfdir_get():
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

def enforce_unicode(object o):
	if isinstance(o, unicode):
		s = o
	else:
		try:
			s = unicode(o, "UTF-8")
		except:
			s = o
	return s

cdef class XmmsCore:
	"""
	This is the class representing the XMMS2 client itself. The methods in
	this class may be used to control and interact with XMMS2.
	"""
	#cdef xmmsc_connection_t *conn
	#cdef int isconnected
	#cdef object disconnect_fun
	#cdef object needout_fun
	#cdef readonly XmmsSourcePreference source_preference
	#cdef XmmsResultTracker result_tracker
	#cdef readonly object clientname

	def __cinit__(self, *args, **kargs): #Trick to allow subclass with init arguments
		"""
		Initiates a connection to the XMMS2 daemon. All operations
		involving the daemon are done via this connection.
		"""
		if 'clientname' in kargs:
			clientname = kargs['clientname']
		elif len(args):
			clientname = args[0]
		else:
			clientname = None
		if clientname is None:
			clientname = "UnnamedPythonClient"
		self.clientname = clientname
		self.source_preference = XmmsSourcePreference(["client/"+clientname, "server", "plugins/*", "client/*", "*"])
		self.result_tracker = XmmsResultTracker() # Keep track of all results that set a notifier.
		self.new_connection()

	def __dealloc__(self):
		if self.conn != NULL:
			xmmsc_unref(self.conn)
			self.conn = NULL

	def __del__(self):
		self.disconnect()

	cdef new_connection(self):
		self.conn = xmmsc_init(to_charp(from_unicode(self.clientname)))
		if self.conn == NULL:
			raise ValueError("Failed to initialize xmmsclient library! Probably due to broken name.")

	cpdef get_source_preference(self):
		return self.source_preference.get()
	cpdef set_source_preference(self, sources):
		self.source_preference.set(sources)

	cpdef _needout_cb(self, int i):
		if self.needout_fun is not None:
			self.needout_fun(i)
	cpdef _disconnect_cb(self):
		if self.disconnect_fun is not None:
			self.disconnect_fun(self)

	cpdef disconnect(self):
		# Disconnect all results.
		self.result_tracker.disconnect_all(True)
		if self.conn is not NULL:
			xmmsc_unref(self.conn)
			self.conn = NULL
		self.new_connection()

	cpdef ioin(self):
		"""
		ioin() -> bool

		Read data from the daemon, when available.
		Note: This is a low level function that should only be used in
		certain circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_in_handle(self.conn)

	cpdef ioout(self):
		"""
		ioout() -> bool

		Write data out to the daemon, when available. Note: This is a
		low level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_out_handle(self.conn)

	cpdef want_ioout(self):
		"""
		want_ioout() -> bool
		"""
		return xmmsc_io_want_out(self.conn)

	cpdef set_need_out_fun(self, fun):
		"""
		set_need_out_fun(fun)
		"""
		self.needout_fun = fun

	cpdef get_fd(self):
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

	cpdef connect(self, path=None, disconnect_func=None):
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
		cdef char *p
		if path:
			p = to_charp(from_unicode(path))
			ret = xmmsc_connect(self.conn, p)
		else:
			ret = xmmsc_connect(self.conn, NULL)

		if not ret:
			raise IOError("Couldn't connect to the server")
		self.isconnected = 1

		self.disconnect_fun = disconnect_func
		xmmsc_disconnect_callback_set_full(self.conn, python_disconnect_fun, <void *>self, NULL)
		xmmsc_io_need_out_callback_set_full(self.conn, python_need_out_fun, <void *> self, NULL)

	def is_connected(self):
		return self.isconnected != 0

	cdef XmmsResult create_result(self, cb, xmmsc_result_t *res):
		cdef XmmsResult ret
		if res == NULL:
			raise RuntimeError("xmmsc_result_t couldn't be allocated")
		ret = XmmsResult()
		ret.set_sourcepref(self.source_preference)
		ret.set_result(res)
		if cb is not None:
			ret.set_callback(self.result_tracker, cb) # property that setup all.
		return ret


cdef class XmmsApi(XmmsCore):
	cpdef XmmsResult quit(self, cb=None):
		"""
		quit(cb=None) -> XmmsResult

		Tell the XMMS2 daemon to quit.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_quit(self.conn))

	cpdef XmmsResult plugin_list(self, typ, cb = None):
		"""
		plugin_list(typ, cb=None) -> XmmsResult

		Get a list of loaded plugins from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_main_list_plugins(self.conn, typ))

	cpdef XmmsResult playback_start(self, cb = None):
		"""
		playback_start(cb=None) -> XmmsResult

		Instruct the XMMS2 daemon to start playing the currently
		selected file from the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_start(self.conn))

	cpdef XmmsResult playback_stop(self, cb = None):
		"""
		playback_stop(cb=None) -> XmmsResult

		Instruct the XMMS2 daemon to stop playing the file
		currently being played.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_stop(self.conn))

	cpdef XmmsResult playback_tickle(self, cb = None):
		"""
		playback_tickle(cb=None) -> XmmsResult

		Instruct the XMMS2 daemon to move on to the next playlist item.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_tickle(self.conn))

	cpdef XmmsResult playback_pause(self, cb = None):
		"""
		playback_pause(cb=None) -> XmmsResult

		Instruct the XMMS2 daemon to pause playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_pause(self.conn))

	cpdef XmmsResult playback_current_id(self, cb = None):
		"""
		playback_current_id(cb=None) -> XmmsResult

		@rtype: L{XmmsResult}(UInt)
		@return: The medialib id of the item currently selected.
		"""
		return self.create_result(cb, xmmsc_playback_current_id(self.conn))

	cpdef XmmsResult playback_seek_ms(self, int ms, xmms_playback_seek_mode_t whence = PLAYBACK_SEEK_SET, cb = None):
		"""
		playback_seek_ms(ms, whence=PLAYBACK_SEEK_SET, cb=None) -> XmmsResult

		Seek to a time position in the current file or stream in playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		if whence == PLAYBACK_SEEK_SET or whence == PLAYBACK_SEEK_CUR:
			return self.create_result(cb, xmmsc_playback_seek_ms(self.conn, ms, whence))
		else:
			raise ValueError("Bad whence parameter")

	@deprecated
	def playback_seek_ms_rel(self, int ms, cb = None):
		"""
		@deprecated
		playback_seek_ms_rel(ms, cb=None) -> XmmsResult

		Seek to a time position by the given offset in the current file or
		stream in playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.playback_seek_ms(ms, PLAYBACK_SEEK_CUR, cb=cb)

	cpdef XmmsResult playback_seek_samples(self, int samples, xmms_playback_seek_mode_t whence=PLAYBACK_SEEK_SET, cb = None):
		"""
		playback_seek_samples(samples, cb=None) -> XmmsResult

		Seek to a number of samples in the current file or stream in playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		if whence == PLAYBACK_SEEK_SET or whence == PLAYBACK_SEEK_CUR:
			return self.create_result(cb, xmmsc_playback_seek_samples(self.conn, samples, whence))
		else:
			raise ValueError("Bad whence parameter")

	@deprecated
	def playback_seek_samples_rel(self, int samples, cb = None):
		"""
		@deprecated
		playback_seek_samples_rel(samples, cb=None) -> XmmsResult

		Seek to a number of samples by the given offset in the
		current file or stream in playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.playback_seek_samples(samples, XMMS_PLAYBACK_SEEK_CUR, cb=cb)

	cpdef XmmsResult playback_status(self, cb = None):
		"""
		playback_status(cb=None) -> XmmsResult

		Get current playback status from XMMS2 daemon. This is
		essentially the more direct version of
		L{broadcast_playback_status}. Possible return values are:
		L{PLAYBACK_STATUS_STOP}, L{PLAYBACK_STATUS_PLAY},
		L{PLAYBACK_STATUS_PAUSE}
		@rtype: L{XmmsResult}(UInt)
		@return: Current playback status(UInt)
		"""
		return self.create_result(cb, xmmsc_playback_status(self.conn))

	cpdef XmmsResult broadcast_playback_status(self, cb = None):
		"""
		broadcast_playback_status(cb=None) -> XmmsResult

		Set a method to handle the playback status broadcast from the
		XMMS2 daemon.
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_status(self.conn))

	cpdef XmmsResult broadcast_playback_current_id(self, cb = None):
		"""
		broadcast_playback_current_id(cb=None) -> XmmsResult

		Set a method to handle the playback id broadcast from the
		XMMS2 daemon.
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_current_id(self.conn))

	cpdef XmmsResult playback_playtime(self, cb = None):
		"""
		playback_playtime(cb=None) -> XmmsResult

		Return playtime on current file/stream. This is essentially a
		more direct version of L{signal_playback_playtime}
		@rtype: L{XmmsResult}(UInt)
		@return: The result of the operation.(playtime in milliseconds)
		"""
		return self.create_result(cb, xmmsc_playback_playtime(self.conn))

	cpdef XmmsResult signal_playback_playtime(self, cb = None):
		"""
		signal_playback_playtime(cb=None) -> XmmsResult

		Set a method to handle the playback playtime signal from the
		XMMS2 daemon.
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_signal_playback_playtime(self.conn))

	cpdef XmmsResult playback_volume_set(self, channel, int volume, cb = None):
		"""
		playback_volume_set(channel, volume, cb=None) -> XmmsResult

		Set the playback volume for specified channel
		@rtype: L{XmmsResult}(UInt)
		"""
		cdef char *c
		c = to_charp(from_unicode(channel))
		return self.create_result(cb, xmmsc_playback_volume_set(self.conn, c, volume))

	cpdef XmmsResult playback_volume_get(self, cb = None):
		"""
		playback_volume_get(cb=None) -> XmmsResult

		Get the playback for all channels
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_playback_volume_get(self.conn))

	cpdef XmmsResult broadcast_playback_volume_changed(self, cb = None):
		"""
		broadcast_playback_volume_changed(cb=None) -> XmmsResult

		Set a broadcast callback for volume updates
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_volume_changed(self.conn))

	cpdef XmmsResult broadcast_playlist_loaded(self, cb = None):
		"""
		broadcast_playlist_loaded(cb=None) -> XmmsResult

		Set a broadcast callback for loaded playlist event
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_loaded(self.conn))

	cpdef XmmsResult playlist_load(self, playlist = None, cb = None):
		"""
		playlist_load(playlist=None, cb=None) -> XmmsResult

		Load the playlist as current playlist
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_load(self.conn, p))

	cpdef XmmsResult playlist_list(self, cb = None):
		"""
		playlist_list(cb=None) -> XmmsResult

		Lists the playlists
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playlist_list(self.conn))

	cpdef XmmsResult playlist_remove(self, playlist = None, cb = None):
		"""
		playlist_remove(playlist=None, cb=None) -> XmmsResult

		Remove the playlist from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_remove(self.conn, p))

	cpdef XmmsResult playlist_shuffle(self, playlist = None, cb = None):
		"""
		playlist_shuffle(playlist=None, cb=None) -> XmmsResult

		Instruct the XMMS2 daemon to shuffle the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_shuffle(self.conn, p))

	cpdef XmmsResult playlist_rinsert(self, int pos, url, playlist = None, cb = None, encoded=False):
		"""
		playlist_rinsert(pos, url, playlist=None, cb=None) -> XmmsResult

		Insert a directory in the playlist.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c
		cdef char *p
		cdef xmmsc_result_t *res

		c = to_charp(from_unicode(url))
		p = check_playlist(playlist)

		if encoded:
			res = xmmsc_playlist_rinsert_encoded(self.conn, p, pos, c)
		else:
			res = xmmsc_playlist_rinsert(self.conn, p, pos, c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_rinsert_encoded(self, int pos, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_rinsert(pos, url, ..., encoded=True) instead
		"""
		return self.playlist_rinsert(pos, url, playlist, cb=cb, encoded=True)

	cpdef XmmsResult playlist_insert_url(self, int pos, url, playlist = None, cb = None, encoded = False):
		"""
		playlist_insert_url(pos, url, playlist=None, cb=None) -> XmmsResult

		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c
		cdef char *p
		cdef xmmsc_result_t *res

		c = to_charp(from_unicode(url))
		p = check_playlist(playlist)

		if encoded:
			res = xmmsc_playlist_insert_encoded(self.conn, p, pos, c)
		else:
			res = xmmsc_playlist_insert_url(self.conn, p, pos, c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_insert_encoded(self, int pos, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_insert_url(pos, url, ..., encoded=True) instead
		"""
		return self.playlist_insert_url(pos, url, playlist, cb=cb, encoded=True)


	cpdef XmmsResult playlist_insert_id(self, int pos, int id, playlist = None, cb = None):
		"""
		playlist_insert_id(pos, id, playlist=None, cb=None) -> XmmsResult

		Insert a medialib to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_insert_id(self.conn, p, pos, id))


	cpdef XmmsResult playlist_insert_collection(self, int pos, Collection coll, order = None, playlist = None, cb = None):
		"""
		playlist_insert_collection(pos, coll, order, playlist=None, cb=None) -> XmmsResult

		Insert the content of a collection to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		if order is None:
			order = []
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_insert_collection(self.conn, p, pos, coll.coll, create_native_value(order)))

	cpdef XmmsResult playlist_radd(self, url, playlist = None, cb = None, encoded=False):
		"""
		playlist_radd(url, playlist=None, cb=None) -> XmmsResult

		Add a directory to the playlist.
		Requires a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c
		cdef char *p
		cdef xmmsc_result_t *res

		c = to_charp(from_unicode(url))
		p = check_playlist(playlist)
		if encoded:
			res = xmmsc_playlist_radd_encoded(self.conn, p, c)
		else:
			res = xmmsc_playlist_radd(self.conn, p, c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_radd_encoded(self, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_radd(url, ..., encoded=True) instead
		"""
		return self.playlist_radd(url, playlist, cb = cb, encoded=True)

	cpdef XmmsResult playlist_add_url(self, url, playlist = None, cb = None, encoded=False):
		"""
		playlist_add_url(url, playlist=None, cb=None) -> XmmsResult

		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c
		cdef char *p
		cdef xmmsc_result_t *res

		c = to_charp(from_unicode(url))
		p = check_playlist(playlist)
		if encoded:
			res = xmmsc_playlist_add_encoded(self.conn, p, c)
		else:
			res = xmmsc_playlist_add_url(self.conn, p, c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_add_encoded(self, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_add_url(url, ..., encoded=True) instead
		"""
		return self.playlist_add_url(url, playlist, cb=cb, encoded=True)

	cpdef XmmsResult playlist_add_id(self, int id, playlist = None, cb = None):
		"""
		playlist_add_id(id, playlist=None, cb=None) -> XmmsResult

		Add a medialib id to the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_add_id(self.conn, p, id))

	cpdef XmmsResult playlist_add_collection(self, Collection coll, order = None, playlist = None, cb = None):
		"""
		playlist_add_collection(coll, order, playlist=None, cb=None) -> XmmsResult

		Add the content of a collection to the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)

		if order is None:
			order = []

		return self.create_result(cb, xmmsc_playlist_add_collection(self.conn, p, coll.coll, create_native_value(order)))

	cpdef XmmsResult playlist_remove_entry(self, int id, playlist = None, cb = None):
		"""
		playlist_remove_entry(id, playlist=None, cb=None) -> XmmsResult

		Remove a certain media item from the playlist.
		Requires a number 'id' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_remove_entry(self.conn, p, id))

	cpdef XmmsResult playlist_clear(self, playlist = None, cb = None):
		"""
		playlist_clear(playlist=None, cb=None) -> XmmsResult

		Clear the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_clear(self.conn, p))

	cpdef XmmsResult playlist_list_entries(self, playlist = None, cb = None):
		"""
		playlist_list_entries(playlist=None, cb=None) -> XmmsResult

		Get the current playlist. This function returns a list of IDs
		of the files/streams currently in the playlist. Use
		L{medialib_get_info} to retrieve more specific information.
		@rtype: L{XmmsResult}(UIntList)
		@return: The current playlist.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_list_entries(self.conn, p))

	cpdef XmmsResult playlist_sort(self, props, playlist = None, cb = None):
		"""
		playlist_sort(props, playlist=None, cb=None) -> XmmsResult

		Sorts the playlist according to the properties specified.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_sort(self.conn, p, create_native_value(props)))

	cpdef XmmsResult playlist_set_next_rel(self, int position, cb = None):
		"""
		playlist_set_next_rel(position, cb=None) -> XmmsResult

		Sets the position in the playlist. Same as L{playlist_set_next}
		but sets the next position relative to the current position.
		You can do set_next_rel(-1) to move backwards for example.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_playlist_set_next_rel(self.conn, position))

	cpdef XmmsResult playlist_set_next(self, int position, cb = None):
		"""
		playlist_set_next(position, cb=None) -> XmmsResult

		Sets the position to move to, next, in the playlist. Calling
		L{playback_tickle} will perform the jump to that position.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_playlist_set_next(self.conn, position))

	cpdef XmmsResult playlist_move(self, int cur_pos, int new_pos, playlist = None, cb = None):
		"""
		playlist_move(cur_pos, new_pos, playlist=None, cb=None) -> XmmsResult

		Moves a playlist entry to a new position.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_move_entry(self.conn, p, cur_pos, new_pos))

	cpdef XmmsResult playlist_create(self, playlist, cb = None):
		"""
		playlist_create(playlist, cb=None) -> XmmsResult

		Create a new playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_create(self.conn, p))

	cpdef XmmsResult playlist_current_pos(self, playlist = None, cb = None):
		"""
		playlist_current_pos(playlist=None, cb=None) -> XmmsResult

		Returns the current position in the playlist. This value will
		always be equal to, or larger than 0. The first entry in the
		list is 0.
		@rtype: L{XmmsResult}
		"""
		cdef char *p
		p = check_playlist(playlist)
		return self.create_result(cb, xmmsc_playlist_current_pos(self.conn, p))

	cpdef XmmsResult playlist_current_active(self, cb = None):
		"""
		playlist_current_active(cb=None) -> XmmsResult

		Returns the name of the current active playlist
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_playlist_current_active(self.conn))

	cpdef XmmsResult broadcast_playlist_current_pos(self, cb = None):
		"""
		broadcast_playlist_current_pos(cb=None) -> XmmsResult

		Set a method to handle the playlist current position updates
		from the XMMS2 daemon. This is triggered whenever the daemon
		jumps from one playlist position to another. (not when moving
		a playlist item from one position to another)
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_current_pos(self.conn))

	cpdef XmmsResult broadcast_playlist_changed(self, cb = None):
		"""
		broadcast_playlist_changed(cb=None) -> XmmsResult

		Set a method to handle the playlist changed broadcast from the
		XMMS2 daemon. Updated data is sent whenever the daemon's
		playlist changes.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_changed(self.conn))

	cpdef XmmsResult broadcast_config_value_changed(self, cb = None):
		"""
		broadcast_config_value_changed(cb=None) -> XmmsResult

		Set a method to handle the config value changed broadcast
		from the XMMS2 daemon.(i.e. some configuration value has
		been modified) Updated data is sent whenever a config
		value is modified.
		@rtype: L{XmmsResult} (the modified config key and its value)
		"""
		return self.create_result(cb, xmmsc_broadcast_config_value_changed(self.conn))

	@deprecated
	def broadcast_configval_changed(self, cb = None):
		"""
		@deprecated
		Use broadcast_config_value_changed(...) instead
		"""
		return self.broadcast_config_value_changed(cb)

	cpdef XmmsResult config_set_value(self, key, val, cb = None):
		"""
		config_set_value(key, val, cb=None) -> XmmsResult

		Set a configuration value on the daemon, given a key.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c1
		cdef char *c2
		c1 = to_charp(from_unicode(key))
		c2 = to_charp(from_unicode(val))
		return self.create_result(cb, xmmsc_config_set_value(self.conn, c1, c2))

	@deprecated
	def configval_set(self, key, val, cb = None):
		"""
		@deprecated
		Use config_set_value(key, val, ...) instead
		"""
		return self.config_set_value(key, val, cb)

	cpdef XmmsResult config_get_value(self, key, cb = None):
		"""
		config_get_value(key, cb=None) -> XmmsResult

		Get the configuration value of a given key, from the daemon.
		@rtype: L{XmmsResult}(String)
		@return: The result of the operation.
		"""
		cdef char *c
		c = to_charp(from_unicode(key))
		return self.create_result(cb, xmmsc_config_get_value(self.conn, c))

	@deprecated
	def configval_get(self, key, cb = None):
		"""
		@deprecated
		Use config_get_value(key, ...) instead
		"""
		return self.config_get_value(key, cb)

	cpdef XmmsResult config_list_values(self, cb = None):
		"""
		config_list_values(cb=None) -> XmmsResult

		Get list of configuration keys on the daemon. Use
		L{config_get_value} to retrieve the values corresponding to the
		configuration keys.
		@rtype: L{XmmsResult}(StringList)
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_config_list_values(self.conn))

	@deprecated
	def configval_list(self, cb = None):
		"""
		@deprecated
		Use config_list_values(...) instead
		"""
		return self.config_list_values(cb)

	cpdef XmmsResult config_register_value(self, valuename, defaultvalue, cb = None):
		"""
		config_register_value(valuename, defaultvalue, cb=None) -> XmmsResult

		Register a new configvalue.
		This should be called in the initcode as XMMS2 won't allow
		set/get on values that haven't been registered.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c1
		cdef char *c2
		c1 = to_charp(from_unicode(valuename))
		c2 = to_charp(from_unicode(defaultvalue))
		return self.create_result(cb, xmmsc_config_register_value(self.conn, c1, c2))

	@deprecated
	def configval_register(self, valuename, defaultvalue, cb = None):
		"""
		@deprecated
		Use config_register_value(valuename, defaultvalue, ...) instead
		"""
		return self.config_register_value(valuename, defaultvalue, cb)

	cpdef XmmsResult medialib_add_entry(self, path, cb = None, encoded=False):
		"""
		medialib_add_entry(file, cb=None) -> XmmsResult

		Add an entry to the MediaLib.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *c
		cdef xmmsc_result_t *res
		c = to_charp(from_unicode(path))
		if encoded:
			res = xmmsc_medialib_add_entry_encoded(self.conn, c)
		else:
			res = xmmsc_medialib_add_entry(self.conn, c)
		return self.create_result(cb, res)

	@deprecated
	def medialib_add_entry_encoded(self, path, cb = None):
		"""
		@deprecated
		Use medialib_add_entry(file, ..., encoded=True) instead
		"""
		return self.medialib_add_entry(path, cb=cb, encoded=True)

	cpdef XmmsResult medialib_remove_entry(self, int id, cb=None):
		"""
		medialib_remove_entry(id, cb=None) -> XmmsResult

		Remove an entry from the medialib.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_remove_entry(self.conn, id))

	cpdef XmmsResult medialib_get_info(self, int id, cb = None):
		"""
		medialib_get_info(id, cb=None) -> XmmsResult

		@rtype: L{XmmsResult}(HashTable)
		@return: Information about the medialib entry position
		specified.
		"""
		cdef XmmsResult res
		res = self.create_result(cb, xmmsc_medialib_get_info(self.conn, id))
		res.ispropdict = 1
		return res

	cpdef XmmsResult medialib_rehash(self, int id = 0, cb = None):
		"""
		medialib_rehash(id=0, cb=None) -> XmmsResult

		Force the medialib to check that metadata stored is up to
		date.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_rehash(self.conn, id))

	cpdef XmmsResult medialib_get_id(self, url, cb = None, encoded=False):
		"""
		medialib_get_id(url, cb=None, encoded=False) -> XmmsResult

		Search for an entry (URL) in the medialib and return its ID
		number.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *u
		cdef xmmsc_result_t *res
		u = to_charp(from_unicode(url))
		if encoded:
			res = xmmsc_medialib_get_id_encoded(self.conn, u)
		else:
			res = xmmsc_medialib_get_id(self.conn, u)
		return self.create_result(cb, res)

	cpdef XmmsResult medialib_import_path(self, path, cb = None, encoded=False):
		"""
		medialib_import_path(path, cb=None, encoded=False) -> XmmsResult

		Import metadata from all files recursively from the directory
		passed as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		cdef xmmsc_result_t *res
		p = to_charp(from_unicode(path))
		if encoded:
			res = xmmsc_medialib_import_path_encoded(self.conn, p)
		else:
			res = xmmsc_medialib_import_path(self.conn, p)
		return self.create_result(cb, res)

	@deprecated
	def medialib_path_import(self, path, cb = None, encoded=False):
		"""
		@deprecated
		Use medialib_import_path(path, ...) instead
		"""
		return self.medialib_import_path(self, path, cb=cb, encoded=encoded)

	@deprecated
	def medialib_path_import_encoded(self, path, cb = None):
		"""
		@deprecated
		Use medialib_import_path(path, ..., encoded=True) instead
		"""
		return self.medialib_import_path(self, path, cb=cb, encoded=True)

	cpdef XmmsResult medialib_property_set(self, int id, key, value, source=None, cb=None):
		"""
		medialib_property_set(id, key, value, source=None, cb=None) -> XmmsResult

		Associate a value with a medialib entry. Source is optional.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *k
		cdef char *v
		cdef char *s
		cdef xmmsc_result_t *res
		k = to_charp(from_unicode(key))
		if isinstance(value, int):
			if source:
				s = to_charp(from_unicode(source))
				res = xmmsc_medialib_entry_property_set_int_with_source(self.conn, id, s, k, value)
			else:
				res = xmmsc_medialib_entry_property_set_int(self.conn, id, k, value)
		else:
			v = to_charp(from_unicode(value))
			if source:
				s = to_charp(from_unicode(source))
				res = xmmsc_medialib_entry_property_set_str_with_source(self.conn, id, s, k, v)
			else:
				res = xmmsc_medialib_entry_property_set_str(self.conn, id, k, v)

	cpdef XmmsResult medialib_property_remove(self, int id, key, source=None, cb=None):
		"""
		medialib_property_remove(id, key, source=None, cb=None) -> XmmsResult

		Remove a value from a medialib entry. Source is optional.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *k
		cdef char *s
		k = to_charp(from_unicode(key))
		if source:
			s = to_charp(from_unicode(source))
			return self.create_result(cb, xmmsc_medialib_entry_property_remove_with_source(self.conn,id,s,k))
		else:
			return self.create_result(cb, xmmsc_medialib_entry_property_remove(self.conn,id,k))

	cpdef XmmsResult broadcast_medialib_entry_added(self, cb = None):
		"""
		broadcast_medialib_entry_added(cb=None) -> XmmsResult

		Set a method to handle the medialib entry added broadcast
		from the XMMS2 daemon. (i.e. a new entry has been added)
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_added(self.conn))

	cpdef XmmsResult broadcast_medialib_entry_changed(self, cb = None):
		"""
		broadcast_medialib_entry_changed(cb=None) -> XmmsResult

		Set a method to handle the medialib entry changed broadcast
		from the XMMS2 daemon.
		Updated data is sent when the metadata for a song is updated
		in the medialib.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_changed(self.conn))

	cpdef XmmsResult broadcast_collection_changed(self, cb = None):
		"""
		broadcast_collection_changed(cb=None) -> XmmsResult

		Set a method to handle the collection changed broadcast
		from the XMMS2 daemon.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_collection_changed(self.conn))

	cpdef XmmsResult signal_mediainfo_reader_unindexed(self, cb = None):
		"""
		signal_mediainfo_reader_unindexed(cb=None) -> XmmsResult

		Tell daemon to send you the number of unindexed files in the mlib
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_signal_mediainfo_reader_unindexed(self.conn))

	cpdef XmmsResult broadcast_mediainfo_reader_status(self, cb = None):
		"""
		broadcast_mediainfo_reader_status(cb=None) -> XmmsResult

		Tell daemon to send you the status of the mediainfo reader
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_broadcast_mediainfo_reader_status(self.conn))

	cpdef XmmsResult xform_media_browse(self, url, cb=None, encoded=False):
		"""
		xform_media_browse(url, cb=None, encoded=False) -> XmmsResult

		Browse files from xform plugins.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *u
		cdef xmmsc_result_t *res
		u = to_charp(from_unicode(url))
		if encoded:
			res = xmmsc_xform_media_browse_encoded(self.conn, u)
		else:
			res = xmmsc_xform_media_browse(self.conn, u)
		return self.create_result(cb, res)

	@deprecated
	def xform_media_browse_encoded(self, url, cb=None):
		"""
		@deprecated
		Use xform_media_browse(url, ..., encoded=True) instead
		"""
		return self.xform_media_browse(url, cb=cb, encoded=True)

	cpdef XmmsResult coll_get(self, name, ns="Collections", cb=None):
		"""
		coll_get(name, ns, cb=None) -> XmmsResult

		Retrieve a Collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		cdef char *nam
		n = check_namespace(ns, False)
		nam = to_charp(from_unicode(name))
		return self.create_result(cb, xmmsc_coll_get(self.conn, nam, n))

	cpdef XmmsResult coll_list(self, ns="*", cb=None):
		"""
		coll_list(name, ns="*", cb=None) -> XmmsResult

		List collections
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		n = check_namespace(ns, True)
		return self.create_result(cb, xmmsc_coll_list(self.conn, n))

	cpdef XmmsResult coll_save(self, Collection coll, name, ns="Collections", cb=None):
		"""
		coll_save(coll, name, ns, cb=None) -> XmmsResult

		Save a collection on server.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		cdef char *nam
		n = check_namespace(ns, False)
		nam = to_charp(from_unicode(name))
		return self.create_result(cb, xmmsc_coll_save(self.conn, coll.coll, nam, n))

	cpdef XmmsResult coll_remove(self, name, ns="Collections", cb=None):
		"""
		coll_remove(name, ns, cb=None) -> XmmsResult

		Remove a collection on server.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		cdef char *nam
		n = check_namespace(ns, False)
		nam = to_charp(from_unicode(name))
		return self.create_result(cb, xmmsc_coll_remove(self.conn, nam, n))

	cpdef XmmsResult coll_rename(self, oldname, newname, ns="Collections", cb=None):
		"""
		coll_remove(name, ns, cb=None) -> XmmsResult

		Remove a collection on server.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		cdef char *oldnam
		cdef char *newnam
		n = check_namespace(ns, False)

		oldnam = to_charp(from_unicode(oldname))
		newnam = to_charp(from_unicode(newname))
		return self.create_result(cb, xmmsc_coll_rename(self.conn, oldnam, newnam, n))

	cpdef XmmsResult coll_idlist_from_playlist_file(self, path, cb=None):
		"""
		coll_idlist_from_playlist_file(path, cb=None) -> XmmsResult

		Create an idlist from a playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *p
		p = to_charp(from_unicode(path))
		return self.create_result(cb, xmmsc_coll_idlist_from_playlist_file(self.conn, p))

	cpdef XmmsResult coll_query_ids(self, Collection coll, start=0, leng=0, order=None, cb=None):
		"""
		coll_query_ids(coll, start=0, leng=0, order=None, cb=None) -> XmmsResult

		Retrive a list of ids of the media matching the collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		if order is None:
			order = []

		return self.create_result(cb, xmmsc_coll_query_ids(self.conn, coll.coll, create_native_value(order), start, leng))

	cpdef XmmsResult coll_query_infos(self, Collection coll, fields, start=0, leng=0, order=None, groupby=None, cb=None):
		"""
		coll_query_infos(coll, fields, start=0, leng=0, order=None, groupby=None, cb=None) -> XmmsResult

		Retrive a list of mediainfo of the media matching the collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		if order is None:
			order = []
		if groupby is None:
			groupby = []

		return self.create_result(cb, xmmsc_coll_query_infos(self.conn, coll.coll, create_native_value(order), start, leng, create_native_value(fields), create_native_value(groupby)))

	cpdef XmmsResult bindata_add(self, data, cb=None):
		"""
		bindata_add(data, cb=None) -> XmmsResult

		Add a datafile to the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *t
		t = to_charp(data)
		return self.create_result(cb, xmmsc_bindata_add(self.conn,<unsigned char *>t,len(data)))

	cpdef XmmsResult bindata_retrieve(self, hash, cb=None):
		"""
		bindata_retrieve(hash, cb=None) -> XmmsResult

		Retrieve a datafile from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *h
		h = to_charp(hash)
		return self.create_result(cb, xmmsc_bindata_retrieve(self.conn, h))

	cpdef XmmsResult bindata_remove(self, hash, cb=None):
		"""
		bindata_remove(hash, cb=None) -> XmmsResult

		Remove a datafile from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *h
		h = to_charp(hash)
		return self.create_result(cb, xmmsc_bindata_remove(self.conn, h))

	cpdef XmmsResult bindata_list(self, cb=None):
		"""
		bindata_list(cb=None) -> XmmsResult

		List all bindata hashes stored on the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_bindata_list(self.conn))



class XmmsDisconnectException(Exception):
	pass

from os import write
cdef class XmmsLoop(XmmsApi):
	#cdef bint do_loop
	#cdef object wakeup

	def __cinit__(self):
		self.do_loop = 0

	def _loop_set_wakeup(self, fd):
		"""
		Set the file descriptor for wakeup operations.
		This method should not be used outside the XmmsLoop class implementation.
		It is provided to allow subclasses to add some locking on it in
		multithreaded applications.

		Use a Rlock if you want to synchronize other methods using the same lock.
		"""
		self.wakeup = fd
	def _loop_get_wakeup(self):
		"""
		Get the file descriptor for wakeup operations.
		This method should not be used outside the XmmsLoop class implementation.
		It is provided to allow subclasses to add some locking on it in
		multithreaded applications.

		Use a Rlock if you want to synchronize other methods using the same lock.
		"""
		return self.wakeup

	def exit_loop(self):
		"""
		exit_loop()

		Exits from the L{loop} call
		"""
		self.do_loop = False
		self.loop_tickle()

	def loop_tickle(self):
		w = self._loop_get_wakeup()
		if w is not None:
			write(w, "1")

	def loop_iter(self, infd=None, outfd=None, errfd=None, timeout=-1):
		"""
		loop_iter(infd=None, outfd=None, errfd=None, timeout=None)
		Run one iteration of the main loop. Should be overridden to add
		custom operations in the main loop.
		@return The tuple returned by select.select() to be used by overridding
		methods in subclasses.
		"""
		cdef int fd
		fd = xmmsc_io_fd_get(self.conn)
		if self.want_ioout():
			w = [fd]
		else:
			w = []
		r = [fd]
		err = [fd]
		if infd:
			r += infd
		if outfd:
			w += outfd
		if errfd:
			err += errfd
		if timeout < 0:
			(i, o, e) = select(r, w, err)
		else:
			(i, o, e) = select(r, w, err, timeout)
		if e and fd in e:
			xmmsc_io_disconnect(self.conn)
			raise XmmsDisconnectException()
		if i and fd in i:
			if not xmmsc_io_in_handle(self.conn):
				raise XmmsDisconnectException()
		if o and fd in o:
			if not xmmsc_io_out_handle(self.conn):
				raise XmmsDisconnectException()
		return (i, o, e) #Can be used by overridding methods for extra handling.

	def loop(self):
		"""
		loop()

		Main client loop for most python clients. Call this to run the
		client once everything has been set up. This function blocks
		until L{exit_loop} is called. One can override L {loop_iter} to
		perform extra processing.
		"""
		from os import pipe, read
		(r, w) = pipe()

		self.do_loop = True
		self._loop_set_wakeup(w)

		while self.do_loop:
			try:
				(i, o, e) = self.loop_iter(infd=[r])
				if r in i:
					read(r, 1) # Purge wakeup stream (each wakeup signal should not write more than one byte)
			except XmmsDisconnectException:
				self.do_loop = False

		self._loop_set_wakeup(None)

# Compatibility
XMMS = XmmsLoop
XMMSResult = XmmsResult
