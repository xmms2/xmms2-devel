"""
Python bindings for XMMS2.
"""

cdef extern from "string.h" :
	int strcmp (signed char *s1, signed char *s2)

cdef extern from "internal/xlist-int.h" :
	ctypedef struct x_list_t :
		void *data
		x_list_t *next
		x_list_t *prev

cdef extern from "internal/xhash-int.h" :
	ctypedef struct x_hash_t

	ctypedef object (*XHFunc) (signed char *key, signed char *value, object user_data)

	void x_hash_foreach (x_hash_t *hash_table, XHFunc func, object user_data)

cdef extern from "xmms/object.h":
	int XMMS_OBJECT_CMD_ARG_NONE
	int XMMS_OBJECT_CMD_ARG_UINT32,
	int XMMS_OBJECT_CMD_ARG_INT32,
	int XMMS_OBJECT_CMD_ARG_STRING,
	int XMMS_OBJECT_CMD_ARG_HASHTABLE,
	int XMMS_OBJECT_CMD_ARG_UINTLIST,
	int XMMS_OBJECT_CMD_ARG_INTLIST,
	int XMMS_OBJECT_CMD_ARG_STRINGLIST,
	int XMMS_OBJECT_CMD_ARG_PLCH,
	int XMMS_OBJECT_CMD_ARG_HASHLIST,

# Actually we don't want a GLib Mainloop here. but for the time beeing....
cdef extern from "glib.h" :
	ctypedef struct GMainContext
	ctypedef struct GMainLoop

	void g_main_loop_run (GMainLoop *loop)
	GMainLoop *g_main_loop_new (GMainContext *context, signed int is_running)

cdef extern from "internal/client_ipc.h" :
	ctypedef struct xmmsc_ipc_t

	int xmmsc_ipc_io_in_callback (xmmsc_ipc_t *ipc)
	int xmmsc_ipc_io_out_callback (xmmsc_ipc_t *ipc)
	int xmmsc_ipc_io_out (xmmsc_ipc_t *ipc)
	int xmmsc_ipc_fd_get (xmmsc_ipc_t *ipc)
	void xmmsc_ipc_disconnect (xmmsc_ipc_t *ipc)
	void xmmsc_ipc_error_set (xmmsc_ipc_t *ipc, char *error)

cdef extern from "internal/xmmsclient_int.h" :
	ctypedef struct xmmsc_connection_t :
		xmmsc_ipc_t *ipc
	
cdef extern from "xmms/xmmsclient.h" :

	ctypedef struct xmmsc_result_t
	ctypedef object (*xmmsc_result_notifier_t) (xmmsc_result_t *res, object user_data)

	xmmsc_result_t *xmmsc_result_restart (xmmsc_result_t *res)
	void xmmsc_result_ref (xmmsc_result_t *res)
	void xmmsc_result_unref (xmmsc_result_t *res)
	void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, object user_data)
	void xmmsc_result_wait (xmmsc_result_t *res)
	signed int xmmsc_result_iserror (xmmsc_result_t *res)
	signed char *xmmsc_result_get_error (xmmsc_result_t *res)
	int xmmsc_result_cid (xmmsc_result_t *res)
	int xmmsc_result_get_type (xmmsc_result_t *res)

	signed int xmmsc_result_get_int (xmmsc_result_t *res, int *r)
	signed int xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r)
	signed int xmmsc_result_get_string (xmmsc_result_t *res, signed char **r)
	signed int xmmsc_result_get_hashtable (xmmsc_result_t *res, x_hash_t **r)
	signed int xmmsc_result_get_stringlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_intlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_uintlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_hashlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_playlist_change (xmmsc_result_t *res, unsigned int *change, unsigned int *id, unsigned int *argument)

	xmmsc_connection_t *xmmsc_init (char *clientname)
	void xmmsc_disconnect_callback_set (xmmsc_connection_t *c, object (*callback) (object), object userdata)
	signed int xmmsc_connect (xmmsc_connection_t *c, signed char *p)
	void xmmsc_unref (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_quit (xmmsc_connection_t *conn)

	xmmsc_result_t *xmmsc_playlist_shuffle (xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_playlist_add (xmmsc_connection_t *, char *)
	xmmsc_result_t *xmmsc_playlist_remove (xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_clear (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_save (xmmsc_connection_t *c, char *filename)
	xmmsc_result_t *xmmsc_playlist_list (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_sort (xmmsc_connection_t *c, char *property) 
	xmmsc_result_t *xmmsc_playlist_set_next (xmmsc_connection_t *c, int pos)
	xmmsc_result_t *xmmsc_playlist_set_next_rel (xmmsc_connection_t *c, signed int)
	xmmsc_result_t *xmmsc_playlist_move (xmmsc_connection_t *c, unsigned int id, signed int movement)
	xmmsc_result_t *xmmsc_playlist_current_pos (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playlist_changed (xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_playback_stop (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_tickle (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_start (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_pause (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_current_id (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples)
	xmmsc_result_t *xmmsc_playback_playtime (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_status (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_broadcast_playback_status (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_broadcast_playback_current_id (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_signal_playback_playtime (xmmsc_connection_t *c)


	xmmsc_result_t *xmmsc_configval_set (xmmsc_connection_t *c, char *key, char *val)
	xmmsc_result_t *xmmsc_configval_list (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_configval_get (xmmsc_connection_t *c, char *key)

	xmmsc_result_t *xmmsc_broadcast_configval_changed (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_medialib_select (xmmsc_connection_t *conn, char *query)
	xmmsc_result_t *xmmsc_medialib_playlist_save_current (xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_playlist_load (xmmsc_connection_t *conn, char *name)
	xmmsc_result_t *xmmsc_medialib_add_entry (xmmsc_connection_t *conn, char *url)
	xmmsc_result_t *xmmsc_medialib_get_info (xmmsc_connection_t *, unsigned int id)
	xmmsc_result_t *xmmsc_medialib_add_to_playlist (xmmsc_connection_t *c, char *query)

	xmmsc_result_t *xmmsc_broadcast_medialib_entry_changed (xmmsc_connection_t *c)
	
	xmmsc_result_t *xmmsc_signal_visualisation_data (xmmsc_connection_t *c)

	char *xmmsc_encode_path (char *path)
	char *xmmsc_decode_path (char *path)

cdef extern from "xmms/xmmsclient-glib.h" :
	void xmmsc_ipc_setup_with_gmain (xmmsc_connection_t *connection)

#####################################################################

from select import select
from os import write
import os

cdef foreach_hash (signed char *key, signed char *value, udata) :
	udata[key] = value

ObjectRef = {}

cdef ResultNotifier (xmmsc_result_t *res, obj) :
	obj.callback ()
	if not obj.get_broadcast () :
		xmmsc_result_unref(res)
		del ObjectRef[obj.get_cid ()]
		
	
cdef class XMMSResult :
	"""
	Class containing the results of some operation
	"""
	cdef xmmsc_result_t *res
	cdef object notifier
	cdef object user_data
	cdef int cid
	cdef int broadcast

	def __new__ (self) :
		self.cid = 0

	def more_init (self, broadcast = 0) :
		self.cid = xmmsc_result_cid (self.res)
		self.broadcast = broadcast
		xmmsc_result_notifier_set (self.res, ResultNotifier, self)
		ObjectRef[self.cid] = self

	def value(self):
		self._check()
	
		type = xmmsc_result_get_type(self.res)

		if type == XMMS_OBJECT_CMD_ARG_UINT32:
			return self.get_uint()
		elif type == XMMS_OBJECT_CMD_ARG_INT32:
			return self.get_int()
		elif type == XMMS_OBJECT_CMD_ARG_STRING:
			return self.get_string()
		elif type == XMMS_OBJECT_CMD_ARG_UINTLIST:
			return self.get_uintlist()
		elif type == XMMS_OBJECT_CMD_ARG_INTLIST:
			return self.get_intlist()
		elif type == XMMS_OBJECT_CMD_ARG_STRINGLIST:
			return self.get_stringlist()
		elif type == XMMS_OBJECT_CMD_ARG_HASHTABLE:
			return self.get_hashtable()
		elif type == XMMS_OBJECT_CMD_ARG_HASHLIST:
			return self.get_hashlist()
		elif type == XMMS_OBJECT_CMD_ARG_PLCH:
			return self.get_playlistchange()

	def get_broadcast (self) :
		return self.broadcast

	def get_cid (self) :
		return self.cid

	def callback (self) :
		""" Override me! """
		pass

	def _check (self) :
		if not self.res :
			raise ValueError ("The resultset did not contain a reply!")

	def wait (self) :
		"""
		Wait for the result from the daemon.
		"""
		self._check ()
		xmmsc_result_wait (self.res)

	def get_int (self) :
		"""
		Get data from the result structure as an int.
		@rtype: int
		"""
		cdef signed int ret
		self._check ()
		if xmmsc_result_get_int (self.res, &ret) :
			return ret
		else :
			raise ValueError ("Failed to retrieve value!")

	def get_uint (self) :
		"""
		Get data from the result structure as an unsigned int.
		@rtype: uint
		"""
		cdef unsigned int ret
		self._check ()
		if xmmsc_result_get_uint (self.res, &ret) :
			return ret
		else :
			raise ValueError ("Failed to retrieve value!")

	def get_string (self) :
		"""
		Get data from the result structure as a string.
		@rtype: string
		"""
		cdef signed char *ret

		self._check ()
		if xmmsc_result_get_string (self.res, &ret) :
			return ret
		else :
			raise ValueError ("Failed to retrieve value!")


	def get_hashtable (self) :
		"""
		@return: A hash table containing media info.
		"""
		cdef x_hash_t *hash
		self._check ()

		if xmmsc_result_get_hashtable (self.res, &hash) :
			ret = {}
			x_hash_foreach (hash, foreach_hash, ret)
			return ret
		else :
			raise ValueError ("Failed to retrieve value!")
			
	def get_intlist (self) :
		"""
		@return: A list of ints from the result structure.
		"""
		cdef x_list_t *l
		cdef x_list_t *n

		self._check ()
		if xmmsc_result_get_intlist (self.res, &l) :
			ret = []
			n = l
			while n :
				ret.append (<signed int>n.data)
				n = n.next

			return ret
		else :
			raise ValueError ("Failed to retrieve value!")

	def get_uintlist (self) :
		"""
		@return: A list of unsigned ints from the result structure.
		"""
		cdef x_list_t *l
		cdef x_list_t *n

		self._check ()
		if xmmsc_result_get_uintlist (self.res, &l) :
			ret = []
			n = l
			while n :
				ret.append (<unsigned int>n.data)
				n = n.next

			return ret
		else :
			raise ValueError ("Failed to retrieve value!")

	def get_hashlist (self) :
		"""
		@return: A list of dicts from the result structure.
		"""
		cdef x_list_t *l
		cdef x_list_t *n

		self._check ()
		if xmmsc_result_get_hashlist (self.res, &l) :
			ret = []
			n = l
			while n :
				hash = {}
				x_hash_foreach (<x_hash_t *>n.data, foreach_hash, hash)
				ret.append (hash)
				n = n.next
			return ret
		else :
			raise ValueError ("Failed to retrieve value!")


	def get_stringlist (self) :
		"""
		@return: A list of strings from the result structure.
		"""
		cdef x_list_t *l
		cdef x_list_t *n

		self._check ()
		if xmmsc_result_get_stringlist (self.res, &l) :
			ret = []
			n = l
			while n :
				ret.append (<signed char *>n.data)
				n = n.next

			return ret
		else :
			raise ValueError ("Failed to retrieve value!")

	def get_playlistchange (self) :
		"""
		@return: a tuple with (changeid, id, argument)
		"""
		cdef unsigned int change
		cdef unsigned int id
		cdef unsigned int arg

		self._check ()
		if xmmsc_result_get_playlist_change (self.res, &change, &id, &arg) :
			return (change, id, arg)
		else :
			raise ValueError ("Failed to retrieve value!")


	def restart (self) :
		cdef XMMSResult r
		r = self

		xmmsc_result_unref (r.res)
		r.res = xmmsc_result_restart (self.res)
		self.more_init ()
		# unref once more, due to ref in
		# more_init->xmmsc_result_notifier_set
		xmmsc_result_unref (r.res)

		return r

	def iserror (self) :
		"""
		@return: Whether the result represents an error or not.
		@rtype: Boolean
		"""
		return xmmsc_result_iserror (self.res)

	def get_error (self) :
		"""
		@return: Error string from the result.
		@rtype: String
		"""
		return xmmsc_result_get_error (self.res)

	def __dealloc__ (self) :
		"""
		Deallocate the result.
		"""

		if self.res :
			xmmsc_result_unref (self.res)

cdef python_disconnect_fun (obj) :
	obj._disconnect_cb ()

cdef class XMMS :
	"""
	This is the class representing the XMMS2 client itself. The methods in
	this class may be used to control and interact with XMMS2.
	"""
	cdef xmmsc_connection_t *conn
	cdef object loop
	cdef object wakeup
	cdef object disconnect_fun

	def __new__ (self, clientname = "Python XMMSClient") :
		"""
		Initiates a connection to the XMMS2 daemon. All operations
		involving the daemon are done via this connection.
		"""
		self.conn = xmmsc_init (clientname)

	def __dealloc__ (self) :
		""" destroys it all! """

		xmmsc_unref (self.conn)

	def _disconnect_cb (self) :
		self.disconnect_fun (self)

	def encode_path (self, path):
		return xmmsc_encode_path(path)

	def decode_path (self, path):
		return xmmsc_decode_path(path)

	def glib_loop (self) :
		"""
		Main client loop for clients using GLib. Call this to run the
		client once everything has been set up. Note: This should not
		be used for pyGTK clients, which require a call to gtk.main()
		pyGTK clients should use L{SetupWithGmain} This function
		blocks in a g_main_loop_run call - see appropriate GLib
		documentation for details.
		"""
		cdef GMainLoop *ml
		ml = g_main_loop_new (NULL, 0)
		xmmsc_ipc_setup_with_gmain (self.conn)

		g_main_loop_run (ml)

	def setup_with_gmain (self) :
		"""
		Adds the IPC connection to a GMainLoop. pyGTK clients need to
		call this after L{Connect} and before gtk.main()
		"""
		xmmsc_ipc_setup_with_gmain (self.conn)

	def exit_python_loop (self) :
		""" Exits from the PythonLoop() call """
		self.loop = False
		write (self.wakeup, "42")

	def python_loop (self) :
		"""
		Main client loop for most python clients. Call this to run the
		client once everything has been set up. This function blocks
		until L{ExitPythonLoop} is called.
		"""
		fd = xmmsc_ipc_fd_get (self.conn.ipc)
		(r, w) = os.pipe ()

		self.loop = True
		self.wakeup = w

		while self.loop :

			(i, o, e) = select ([fd, r], [], [fd])

			if i and i[0] == fd :
				xmmsc_ipc_io_in_callback (self.conn.ipc)
			if e and e[0] == fd :
				xmmsc_ipc_disconnect (self.conn.ipc)
				self.loop = False

	def ioin (self) :
		"""
		Use this relatively low level function to write your own
		custom main loops, if needed. See the PythonLoop code for an
		example
		"""
		xmmsc_ipc_io_in_callback (self.conn.ipc)

	def ioout(self):
		xmmsc_ipc_io_out_callback (self.conn.ipc)

	def want_ioout(self):
		return xmmsc_ipc_io_out (self.conn.ipc)
		
	def get_fd (self) :
		"""
		Get the underlying file descriptor used for communication with
		the XMMS2 daemon. You can use this in a client to ensure that
		the IPC link is still active and safe to use. (e.g by calling
		select() or poll())
		@rtype: int
		@return: IPC file descriptor
		"""
		return xmmsc_ipc_fd_get (self.conn.ipc)

	def connect (self, path = None, disconnect_func = None) :
		"""
		Connect to the appropriate IPC path, for communication with the
		XMMS2 daemon. This path defaults to /tmp/xmms-ipc-<username> if
		not specified. Call this once you have instantiated the object:

		C{import xmmsclient}

		C{xmms = xmmsclient.XMMS ()}

		C{xmms.Connect ()}

		...
		"""
		if path :
			ret = xmmsc_connect (self.conn, path) 
		else :
			ret = xmmsc_connect (self.conn, NULL)

		if not ret :
			raise IOError ("Couldn't connect to server!")

		self.disconnect_fun = disconnect_func
		xmmsc_disconnect_callback_set (self.conn, python_disconnect_fun, self)


	def quit (self, myClass = None):
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

	def playback_start (self, myClass = None) :
		"""
		Instruct the XMMS2 daemon to start playing the currently
		selected file from the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_start (self.conn)
		ret.more_init ()
		
		return ret

	def playback_stop (self, myClass = None) :
		"""
		Instruct the XMMS2 daemon to stop playing the file
		currently being played.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_stop (self.conn)
		ret.more_init ()
		
		return ret

	def playback_tickle (self, myClass = None) :
		"""
		Instruct the XMMS2 daemon to move on to the next file in the
		playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_tickle (self.conn)
		ret.more_init ()
		
		return ret

	def playback_pause (self, myClass = None) :
		"""
		Instruct the XMMS2 daemon to pause playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_pause (self.conn)
		ret.more_init ()
		
		return ret

	def playback_current_id (self, myClass = None) :
		"""
		@rtype: L{XMMSResult} (UInt)
		@return: The playlist id of the item currently selected.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_current_id (self.conn)
		ret.more_init ()
		
		return ret

	def playback_seek_ms (self, ms, myClass = None) :
		"""
		Seek to an absolute time position in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_seek_ms (self.conn, ms)
		ret.more_init ()
		
		return ret

	def playback_seek_samples (self, samples, myClass = None) :
		"""
		Seek to an absolute number of samples in the current file or
		stream in playback.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_seek_samples (self.conn, samples)
		ret.more_init ()
		
		return ret

	def playback_status (self, myClass = None) :
		"""Get current playback status from XMMS2 daemon. This is
		essentially the more direct version of
		L{BroadcastPlaybackStatus}.
		@rtype: L{XMMSResult} (UInt)
		@return: Current playback status (UInt)
		"""
		cdef XMMSResult ret
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		ret.res = xmmsc_playback_status (self.conn)
		ret.more_init ()
		return ret

	def broadcast_playback_status (self, myClass = None) :
		"""
		Set a class to handle the playback status broadcast from the
		XMMS2 daemon. Note: the handler class is usually a child of the
		XMMSResult class.
		@rtype: L{XMMSResult} (UInt)
		@return: An XMMSResult object that is constantly updated with
		the appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_broadcast_playback_status (self.conn)
		ret.more_init (1)
		
		return ret

	def broadcast_playback_current_id (self, myClass = None) :
		"""
		Set a class to handle the playback id broadcast from the
		XMMS2 daemon. Note: the handler class is usually a child of the
		XMMSResult class.
		@rtype: L{XMMSResult} (UInt)
		@return: An XMMSResult object that is constantly updated with
		the appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_broadcast_playback_current_id (self.conn)
		ret.more_init (1)
		
		return ret

	def playback_playtime (self, myClass = None) :
		"""
		Return playtime on current file/stream. This is essentially a
		more direct version of L{SignalPlaybackPlaytime}
		@rtype: L{XMMSResult} (UInt)
		@return: The result of the operation. (playtime in milliseconds)
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playback_playtime (self.conn)
		ret.more_init ()

		return ret

	def signal_playback_playtime (self, myClass = None) :
		"""
		Set a class to handle the playback playtime signal from the
		XMMS2 daemon. This can be used to keep track of the amount of
		time played on the current file/stream. Note: the handler
		class is usually a child of the XMMSResult class.
		@rtype: L{XMMSResult} (UInt)
		@return: The result of the operation. (playtime in milliseconds)
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_signal_playback_playtime (self.conn)
		ret.more_init ()
		
		return ret

	def playlist_shuffle (self, myClass = None) :
		"""
		Instruct the XMMS2 daemon to shuffle the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_shuffle (self.conn)
		ret.more_init ()
		
		return ret

	def playlist_add (self, url, myClass = None) :
		"""
		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a string 'url' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_add (self.conn, url)
		ret.more_init ()
		
		return ret

	def playlist_remove (self, id, myClass = None) :
		"""
		Remove a certain media item from the playlist.
		Requires a number 'id' as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_remove (self.conn, id)
		ret.more_init ()
		
		return ret

	def playlist_clear (self, myClass = None) :
		"""
		Clear the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_clear (self.conn)
		ret.more_init ()
		
		return ret

	def playlist_save (self, filename, myClass = None) :
		"""
		Save the current playlist to file.
		Requires a string 'fname' (filename to save to) as argument.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_save (self.conn, filename)
		ret.more_init ()
		
		return ret

	def playlist_list (self, myClass = None) :
		"""
		Get the current playlist. This function returns a list of IDs
		of the files/streams currently in the playlist. Use
		L{PlaylistGetMediainfo} to retrieve more specific information.
		@rtype: L{XMMSResult} (UIntList)
		@return: The current playlist.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_list (self.conn)
		ret.more_init ()
		
		return ret


	def playlist_sort (self, prop, myClass = None) :
		"""
		Sorts the playlist according to the property specified.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_sort (self.conn, prop)
		ret.more_init ()
		
		return ret

	def playlist_set_next_rel (self, position, myClass = None) :
		"""
		Sets the position in the playlist. Same as set_next but
		does it relative. You can do set_next_rel(-1) to move backwards
		for example
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_set_next_rel (self.conn, position)
		ret.more_init ()
		
		return ret


	def playlist_set_next (self, position, myClass = None) :
		"""
		Sets the position in the playlist. 
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_set_next (self.conn, position)
		ret.more_init ()
		
		return ret

	def playlist_move (self, id, movement, myClass = None) :
		"""
		Move a playlist entry relative to it's current position in
		the playlist. The movement should be a postive value when
		moving down in the playlist and a negative value when moving
		up in the playlist.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_move (self.conn, id, movement)
		ret.more_init ()
		
		return ret

	def playlist_current_pos (self, myClass = None) :
		"""
		Returns the current position in the playlist. This value will
		always be biggern than 0. The first entry in the list is 1
		"""
		cdef XMMSResult ret

		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_playlist_current_pos (self.conn)
		ret.more_init ()

		return ret


	def broadcast_playlist_changed (self, myClass = None) :
		"""
		Set a class to handle the playlist changed broadcast from the
		XMMS2 daemon. (i.e. the player's playlist has changed) Note:
		the handler class is usually a child of the XMMSResult class.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_broadcast_playlist_changed (self.conn)
		ret.more_init (1)
		
		return ret

	def broadcast_configval_changed (self, myClass = None) :
		"""
		Set a class to handle the config value changed broadcast
		from the XMMS2 daemon. (i.e. some configuration value has
		been modified) Note: the handler class is usually a child of
		the XMMSResult class.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info. (the modified config key and its value)
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else:
			ret = XMMSResult ()

		ret.res = xmmsc_broadcast_configval_changed (self.conn)
		ret.more_init (1)

		return ret

	def configval_set (self, key, val, myClass = None) :
		"""
		Set a configuration value on the daemon, given a key.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_configval_set (self.conn, key, val)
		ret.more_init ()
		return ret

	def configval_get (self, key, myClass = None) :
		"""
		Get the configuration value of a given key, from the daemon.
		@rtype: L{XMMSResult} (String)
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_configval_get (self.conn, key)
		ret.more_init ()
		return ret

	def configval_list (self, myClass = None) :
		"""
		Get list of configuration keys on the daemon. Use
		L{ConfigvalGet} to retrieve the values corresponding to the
		configuration keys.
		@rtype: L{XMMSResult} (StringList)
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_configval_list (self.conn)
		ret.more_init ()
		return ret

	def medialib_select (self, query, myClass = None) :
		"""
		Query the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_medialib_select (self.conn, query)
		ret.more_init ()
		return ret

	def medialib_add_entry (self, file, myClass = None) :
		"""
		Add a entry to the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_medialib_add_entry (self.conn, file)
		ret.more_init ()
		return ret

	def medialib_playlist_save_current (self, playlistname, myClass = None) :
		"""
		Save the current playlist to a medialib playlist
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_medialib_playlist_save_current (self.conn, playlistname)
		ret.more_init ()
		return ret

	def medialib_playlist_load (self, playlistname, myClass = None) :
		"""
		Load playlistname from the medialib
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_medialib_playlist_load (self.conn, playlistname)
		ret.more_init ()
		return ret

	def medialib_get_info (self, id, myClass = None) :
		"""
		@rtype: L{XMMSResult} (HashTable)
		@return: Information about the medialib entry
		position specified.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_medialib_get_info (self.conn, id)
		ret.more_init ()
		
		return ret

	def medialib_add_to_playlist (self, query, myClass = None) :
		"""
		Add items in the playlist by querying the MediaLib.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_medialib_add_to_playlist (self.conn, query)
		ret.more_init ()
		
		return ret

	def broadcast_medialib_entry_changed (self, myClass = None) :
		"""
		Set a class to handle the playlist entry changed broadcast
		from the XMMS2 daemon. (i.e. the current entry in the playlist
		has changed) Note: the handler class is usually a child of the
		XMMSResult class.
		@rtype: L{XMMSResult}
		@return: An XMMSResult object that is updated with the
		appropriate info.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_broadcast_medialib_entry_changed (self.conn)
		ret.more_init (1)
		
		return ret



	def signal_visualisation_data (self, myClass = None) :
		"""
		Tell server to send you VisData updates.
		For drawing peek analyzer.
		@rtype: L{XMMSResult}
		@return: The result of the operation.
		"""
		cdef XMMSResult ret
		
		if myClass :
			ret = myClass ()
		else :
			ret = XMMSResult ()
		
		ret.res = xmmsc_signal_visualisation_data (self.conn)
		ret.more_init ()
		return ret

