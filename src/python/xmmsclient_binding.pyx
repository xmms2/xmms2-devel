cdef extern from "string.h" :
	int strcmp (signed char *s1, signed char *s2)

cdef extern from "internal/xlist-int.h" :
	ctypedef struct x_list_t :
		void *data
		x_list_t *next
		x_list_t *prev

cdef extern from "internal/xhash-int.h" :
	ctypedef struct x_hash_t

	ctypedef object (*XHFunc) (signed char *key, signed char *value, void user_data)

	void x_hash_foreach (x_hash_t *hash_table, XHFunc func, object user_data)

# Actually we don't want a GLib Mainloop here. but for the time beeing....
cdef extern from "glib.h" :
	ctypedef struct GMainContext
	ctypedef struct GMainLoop

	void g_main_loop_run (GMainLoop *loop)
	GMainLoop *g_main_loop_new (GMainContext *context, signed int is_running)

cdef extern from "xmms/xmmsclient-result.h" :
	ctypedef struct xmmsc_result_t
	ctypedef void (*xmmsc_result_notifier_t) (xmmsc_result_t *res, object user_data)

	xmmsc_result_t *xmmsc_result_restart (xmmsc_result_t *res)
	void xmmsc_result_ref (xmmsc_result_t *res)
	void xmmsc_result_unref (xmmsc_result_t *res)
	void xmmsc_result_notifier_set (xmmsc_result_t *res, xmmsc_result_notifier_t func, object user_data)
	void xmmsc_result_wait (xmmsc_result_t *res)
	signed int xmmsc_result_iserror (xmmsc_result_t *res)
	signed char *xmmsc_result_get_error (xmmsc_result_t *res)

	signed int xmmsc_result_get_int (xmmsc_result_t *res, int *r)
	signed int xmmsc_result_get_uint (xmmsc_result_t *res, unsigned int *r)
	signed int xmmsc_result_get_string (xmmsc_result_t *res, signed char **r)
	signed int xmmsc_result_get_mediainfo (xmmsc_result_t *res, x_hash_t **r)
	signed int xmmsc_result_get_stringlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_intlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_uintlist (xmmsc_result_t *res, x_list_t **r)
	signed int xmmsc_result_get_double_array (xmmsc_result_t *res, double **r, int *len)
	signed int xmmsc_result_get_playlist_change (xmmsc_result_t *res, unsigned int *change, unsigned int *id, unsigned int *argument)

cdef extern from "xmms/xmmsclient.h" :
	ctypedef struct xmmsc_connection_t

	xmmsc_connection_t *xmmsc_init ()
	signed int xmmsc_connect (xmmsc_connection_t *c, signed char *p)
	xmmsc_result_t *xmmsc_quit (xmmsc_connection_t *conn)

	xmmsc_result_t *xmmsc_playback_start (xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_playback_stop (xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_playback_pause (xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_playback_next (xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_playback_status (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playback_seek_ms (xmmsc_connection_t *c, unsigned int milliseconds)
	xmmsc_result_t *xmmsc_playback_seek_samples (xmmsc_connection_t *c, unsigned int samples)
	xmmsc_result_t *xmmsc_playback_playtime (xmmsc_connection_t *c)

	xmmsc_result_t *xmmsc_playback_current_id (xmmsc_connection_t *conn)
	xmmsc_result_t *xmmsc_playlist_shuffle(xmmsc_connection_t *)
	xmmsc_result_t *xmmsc_playlist_add (xmmsc_connection_t *, signed char *)
	xmmsc_result_t *xmmsc_playlist_remove (xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_clear (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_save (xmmsc_connection_t *c, signed char *)
	xmmsc_result_t *xmmsc_playlist_list (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_get_mediainfo (xmmsc_connection_t *, unsigned int)
	xmmsc_result_t *xmmsc_playlist_sort (xmmsc_connection_t *c, signed char *)
	xmmsc_result_t *xmmsc_playlist_entry_changed (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_changed (xmmsc_connection_t *c)
	xmmsc_result_t *xmmsc_playlist_set_next (xmmsc_connection_t *c, unsigned int type, signed int moment)

cdef extern from "xmms/xmmsclient-glib.h" :
	void xmmsc_setup_with_gmain (xmmsc_connection_t *connection, GMainContext *context)

#####################################################################

cdef foreach_hash (signed char *key, signed char *value, udata) :
# DAMN UGLY HACK! GET RID OF THIS! PLEASE!
	if strcmp (key, "id") == 0 :
		udata[key] = <unsigned int> value
	else :
		udata[key] = value
	
# THIS IS A REALLY REALLY UGLY HACK! global list will break totaly!
restartobj = [None]

cdef ResultNotifier (xmmsc_result_t *res, list) :
	result = list[0]
	result.noti (result)

cdef class XMMSResult :
	cdef xmmsc_result_t *res
	cdef object notifier
	cdef object user_data

	def noti (self, korven) :
		self._check ()
		if self.notifier :
			self.notifier (self)
	
	def _check (self) :
		if not self.res :
			raise ValueError

	def wait (self) :
		self._check ()
		xmmsc_result_wait (self.res)

	def int (self) :
		cdef signed int ret
		self._check ()
		if xmmsc_result_get_int (self.res, &ret) :
			return ret
		else :
			raise ValueError

	def uint (self) :
		cdef unsigned int ret
		self._check ()
		if xmmsc_result_get_uint (self.res, &ret) :
			return ret
		else :
			raise ValueError

	def string (self) :
		cdef signed char *ret

		self._check ()
		if xmmsc_result_get_string (self.res, &ret) :
			return ret
		else :
			raise ValueError


	def mediainfo (self) :
		cdef x_hash_t *hash
		self._check ()

		if xmmsc_result_get_mediainfo (self.res, &hash) :
			ret = {}
			x_hash_foreach (hash, foreach_hash, ret)
			return ret
		else :
			raise ValueError
			
	def intlist (self) :
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
			raise ValueError

	def uintlist (self) :
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
			raise ValueError


	def stringlist (self) :
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
			raise ValueError

	def setnotifier (self, function, userdata=None) :
		global restartobj
		self._check ()
		self.notifier = function
		self.user_data = userdata
		restartobj[0] = self
		xmmsc_result_notifier_set (self.res, ResultNotifier, restartobj)

	def restart (self) :
		global restartobj
		cdef XMMSResult r

		self._check ()
		r = XMMSResult ()
		r.res = xmmsc_result_restart (self.res)
		
		r.notifier = self.notifier
		r.user_data = self.user_data
		restartobj[0] = r

		xmmsc_result_unref (r.res)
		
		return r

	def iserror (self) :
		return xmmsc_result_iserror (self.res)

	def error (self) :
		return xmmsc_result_get_error (self.res)

	def __dealloc__ (self) :
		if self.res :
			xmmsc_result_unref (self.res)
	
cdef class XMMS:
	cdef xmmsc_connection_t *conn

	def __init__ (self) :
		self.conn = xmmsc_init()

	def connect (self, path) :

		if path :
			if path[0] == '/' :
				cpath = "unix:path="+path
			else :
				cpath = path

			r = xmmsc_connect (self.conn, cpath)
		else :
			r = xmmsc_connect (self.conn, NULL)

		if r == 0 :
			raise IOError

	def quit (self) :
		xmmsc_quit (self.conn)

	cdef _res (self, xmmsc_result_t *result) :
		cdef XMMSResult r 
		r = XMMSResult ()
		r.res = result
		return r

	def PlaybackPlaytime (self) :
		return self._res (xmmsc_playback_playtime (self.conn))

	def PlaybackStart (self) :
		return self._res (xmmsc_playback_start (self.conn))

	def PlaybackStop (self) :
		return self._res (xmmsc_playback_stop (self.conn))

	def PlaybackPause (self) :
		return self._res (xmmsc_playback_pause (self.conn))
	
	def PlaybackNext (self) :
		return self._res (xmmsc_playback_next (self.conn))



	def PlaybackCurrentId (self) :
		return self._res (xmmsc_playback_current_id (self.conn))

	def PlaylistShuffle (self) :
		return self._res (xmmsc_playlist_shuffle (self.conn))

	def PlaylistAdd (self, path) :
		if (path[0] == '/') :
			cpath = "file://"+path
		else :
			cpath = path

		return self._res (xmmsc_playlist_add (self.conn, cpath))

	def PlaylistRemove (self, id) :
		return self._res (xmmsc_playlist_remove (self.conn, id))

	def PlaylistSave (self, fname) :
		return self._res (xmmsc_playlist_save (self.conn, fname))

	def PlaylistList (self) :
		return self._res (xmmsc_playlist_list (self.conn))

	def PlaylistSetNext (self, type, moment) :
		return self._res (xmmsc_playlist_set_next (self.conn, type, moment))

	def PlaylistMediainfo (self, id) :
		return self._res (xmmsc_playlist_get_mediainfo (self.conn, id))

	def PlaylistSort (self, prop) :
		return self._res (xmmsc_playlist_sort (self.conn, property))

	def PlaylistEntryChanged (self) :
		return self._res (xmmsc_playlist_entry_changed (self.conn))

	def PlaylistChanged (self) :
		return self._res (xmmsc_playlist_changed (self.conn))

	def loop (self) :
		cdef GMainLoop *ml

		ml = g_main_loop_new (NULL, 0)
		xmmsc_setup_with_gmain (self.conn, NULL)

		g_main_loop_run (ml)
		
