"""
Python bindings for XMMS2.
"""

# CImports
from cpython.mem cimport PyMem_Malloc, PyMem_Free
from cpython.bytes cimport PyBytes_FromStringAndSize
cimport cython
from xmmsutils cimport from_unicode, to_unicode
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

COLLECTION_NS_COLLECTIONS = to_unicode(<char *>XMMS_COLLECTION_NS_COLLECTIONS)
COLLECTION_NS_PLAYLISTS = to_unicode(<char *>XMMS_COLLECTION_NS_PLAYLISTS)
COLLECTION_NS_ALL = to_unicode(<char *>XMMS_COLLECTION_NS_ALL)

ACTIVE_PLAYLIST = to_unicode(<char *>XMMS_ACTIVE_PLAYLIST)

#####################################################################

def deprecated(f):
	def deprecated_decorator(*a, **kw):
		from os import getenv
		if getenv('XMMS_PYTHON_SHOW_DEPRECATED'):
			from sys import stderr
			print >>stderr, "DEPRECATED: %s()" % f.__name__
		return f(*a, **kw)
	deprecated_decorator.__doc__ = f.__doc__
	deprecated_decorator.__name__ = f.__name__
	return deprecated_decorator

# Trick to not expose select in the module, but keep it accessible from builtins
# methods.
cdef object select
cdef _install_select():
	global select
	from select import select as _sel
	select = _sel
_install_select()

cdef char *check_namespace(object ns, bint can_be_all) except NULL:
	cdef char *n
	if ns == COLLECTION_NS_COLLECTIONS:
		n = <char *>XMMS_COLLECTION_NS_COLLECTIONS
	elif ns == COLLECTION_NS_PLAYLISTS:
		n = <char *>XMMS_COLLECTION_NS_PLAYLISTS
	elif can_be_all and ns == COLLECTION_NS_ALL:
		n = <char *>XMMS_COLLECTION_NS_ALL
	else:
		raise ValueError("Bad namespace")
	return n

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

	def __init__(self, sources = None):
		self.set(sources)

	def get(self):
		return self.sources

	def set(self, sources):
		if sources is None:
			self.sources = []
		else:
			self.sources = [enforce_unicode(s) for s in sources]

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
			raise TypeError("Type '%s' is not callable" % cb.__class__.__name__)
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
			traceback.print_exception(*sys.exc_info())
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

	cpdef is_error(self):
		"""
		@return: Whether the result represents an error or not.
		@rtype: Boolean
		"""
		return self.xmmsvalue().is_error()

	@deprecated
	def iserror(self):
		"""
		@deprecated
		Use is_error() instead.
		"""
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

	property xvalue:
		def __get__(self):
			return self.xmmsvalue()

	@deprecated
	def _value(self):
		"""
		@deprecated
		Use xmmsvalue() or the xvalue property.
		"""
		return self.xmmsvalue()

	cpdef value(self):
		return self.xmmsvalue().value()


cdef class XmmsVisResult(XmmsResult):
	#cdef XmmsValue _val
	#cdef VisResultCommand command
	#cdef xmmsc_connection_t *conn

	def __cinit__(self):
		self.command = VIS_RESULT_CMD_NONE
		self.conn = NULL

	def __dealloc__(self):
		if self.conn != NULL:
			xmmsc_unref(self.conn)
			self.conn = NULL

	cdef set_command(self, VisResultCommand cmd, xmmsc_connection_t *conn):
		self.command = cmd
		if self.conn != NULL:
			xmmsc_unref(self.conn)
			self.conn = NULL
		if conn != NULL:
			self.conn = xmmsc_ref(conn)

	cdef retrieve_error(self):
		cdef xmmsv_t *errval
		xval = XmmsResult.xmmsvalue(self)
		if xval.is_error():
			self._val = xval
		else:
			self._val = XmmsValue()
			errval = xmmsv_new_error("Failed to initialize visualization")
			self._val.set_value(errval)
			xmmsv_unref(errval)

	cdef _init_xmmsvalue(self):
		if self._val is None:
			if self.res == NULL:
				raise RuntimeError("Uninitialized result")
			hid = xmmsc_visualization_init_handle(self.res)
			if hid == -1:
				self.retrieve_error()
			else:
				self._val = XmmsValue(pyval = hid)
		return self._val

	cdef _start_xmmsvalue(self):
		if self._val is None:
			if self.res == NULL:
				raise RuntimeError("Uninitialized result")
			xval = XmmsResult.xmmsvalue(self)
			if xval.is_error():
				self._val = xval
			elif self.conn == NULL:
				raise RuntimeError("Internal connection reference not set")
			else:
				xmmsc_visualization_start_handle(self.conn, self.res)
				self._val = XmmsValue(pyval = None)
		return self._val

	cpdef xmmsvalue(self):
		if self.command == VIS_RESULT_CMD_INIT:
			return self._init_xmmsvalue()
		elif self.command == VIS_RESULT_CMD_START:
			return self._start_xmmsvalue()
		else:
			# XXX Raise an exception ?
			return XmmsResult.xmmsvalue(self)

cdef class XmmsVisChunk:
	#cdef short *data
	#cdef int sample_count

	def __cinit__(self):
		self.data = NULL
		self.sample_count = 0

	def __dealloc__(self):
		if self.data != NULL:
			PyMem_Free(self.data)

	cdef set_data(self, short *data, int sample_count):
		if self.data != NULL:
			PyMem_Free(self.data)
			self.sample_count = 0
		self.data = <short *>PyMem_Malloc(sizeof (short) * sample_count)
		if self.data == NULL:
			raise RuntimeError("Failed to initialize chunk data")
		for i in range(sample_count):
			self.data[i] = data[i]
		self.sample_count = sample_count

	def __len__(self):
		return self.sample_count

	cpdef get_buffer(self):
		"""
		Get the chunk buffer
		@rtype: L{bytes}
		@return chunk data as a string
		"""
		if self.data == NULL:
			raise RuntimeError("chunk data not initialized")
		return PyBytes_FromStringAndSize(<char *>self.data, sizeof (short) * self.sample_count)

	cpdef get_data(self):
		"""
		Get chunk data as a list.
		@rtype: L{list}
		@return A list of int
		"""
		if self.data == NULL:
			raise RuntimeError("chunk data not initialized")
		return [<int>self.data[i] for i in range(self.sample_count)]

class VisualizationError(Exception):
	pass


def _noop(*a, **kw):
	pass
def _setcb(f, fb=_noop):
	return hasattr(f, '__call__') and f or fb

class method_arg:
	TYPES = {
		'integer': XMMSV_TYPE_INT64,
		'i': XMMSV_TYPE_INT64,
		'float': XMMSV_TYPE_FLOAT,
		'f': XMMSV_TYPE_FLOAT,
		'string': XMMSV_TYPE_STRING,
		's': XMMSV_TYPE_STRING,
		'coll': XMMSV_TYPE_COLL,
		'c': XMMSV_TYPE_COLL,
		'list': XMMSV_TYPE_LIST,
		'l': XMMSV_TYPE_LIST,
		'dict': XMMSV_TYPE_DICT,
		'd': XMMSV_TYPE_DICT,
		}
	def __init__(self, name, type = '', doc = '', **kw):
		for key, value in kw.items():
			setattr(self, key, value)
		self.name = name
		self.type = type.lower()
		if type not in self.TYPES:
			raise TypeError("Unsupported argument type %s" % type)
		self._x_type = self.TYPES.get(type, XMMSV_TYPE_NONE)
		self.doc = doc

class method_varg:
	def __init__(self, doc = '', **kw):
		self.doc = doc
		for key, value in kw.items():
			setattr(self, key, value)

class service_method:
	def __init__(self, positional = None, named = None, name = None, doc = None):
		self.positional = positional or ()
		self.named = named or ()
		self.name = name
		self.doc = doc

	def __call__(self, f):
		f._xmms_service_entity = dict(
				entity = 'method',
				positional = self.positional,
				named = self.named,
				name = self.name or f.__name__,
				doc = self.doc or f.__doc__,
				)
		return f

cdef class service_broadcast:
	def __init__(self, name = None, doc = None):
		self.name = name
		self.doc = doc

	cdef bind(self, XmmsServiceNamespace namespace, name = None):
		cdef service_broadcast obj
		obj = service_broadcast(self.name or name, self.doc)
		obj.namespace = namespace
		return obj

	cpdef emit(self, value = None):
		if not self.namespace:
			raise RuntimeError("No namespace bound to broadcast")
		path = self.namespace.path + (self.name,)
		return self.namespace.xmms.sc_broadcast_emit(path, value)

	def __call__(self, value = None):
		return self.emit(value)

class service_constant:
	def __init__(self, value):
		self.value = value

cdef class XmmsServiceNamespace:
	namespace_path = ()

	property path:
		def __get__(self):
			if self.parent:
				path = self.parent.path
			else:
				path = ()
			return path + self.namespace_path

	def __cinit__(self, XmmsCore xmms, *a, **ka):
		cdef service_broadcast bc_obj
		self.parent = ka.get('parent', None)
		self.xmms = xmms
		self._xmms_service_constants = set()
		self._bound_methods = dict()

		for attr in dir(self):
			if attr[0] == '_':
				continue
			obj = getattr(self, attr)

			if isinstance(obj, service_constant):
				self._xmms_service_constants.add(attr)
				setattr(self, attr, obj.value)
				continue

			if isinstance(obj, service_broadcast):
				bc_obj = obj
				setattr(self, attr, bc_obj.bind(self, attr))
				continue

			try:
				if issubclass(obj, XmmsServiceNamespace):
					inst = obj(self.xmms, parent = self)
					inst.namespace_path = (attr,)

					setattr(self, attr, inst)
					inst._xmms_service_entity = dict(
							entity = 'namespace',
							name = attr,
							doc = inst.__doc__
							)
			except TypeError:
				pass

	cpdef _walk(self, namespace = None, constant = None, method = None, broadcast = None, path = (), other = _noop, depth = -1, deep = False):
		callbacks = dict(
				namespace = _setcb(namespace, other),
				constant = _setcb(constant, other),
				method = _setcb(method, other),
				broadcast = _setcb(broadcast, other),
				)

		for attr in dir(self):
			if attr[0] == '_':
				continue
			obj = getattr(self, attr)
			if attr in self._xmms_service_constants:
				entity = dict(
						entity = 'constant',
						name = attr,
						)
			elif isinstance(obj, service_broadcast):
				entity = dict(
						entity = 'broadcast',
						name = obj.name or attr,
						doc = obj.doc
						)
			else:
				entity = getattr(obj, '_xmms_service_entity', None)

			if not isinstance(entity, dict):
				continue

			entity_type = entity.get('entity', '')

			if entity_type not in callbacks:
				continue

			if not deep:
				callbacks[entity_type](path, obj, entity)
			if entity_type == 'namespace' and depth:
				obj._walk(path = path + (entity['name'] or attr,), depth = depth - 1, **callbacks)
			if deep:
				callbacks[entity_type](path, obj, entity)

	cpdef register_namespace(self, path, instance, infos):
		cdef xmmsc_sc_namespace_t *ns
		name = from_unicode(infos.get('name', ""))
		if not name: #root namespace already exists
			return
		doc = from_unicode(infos.get('doc', ""))
		ns = _namespace_get(self.xmms.conn, path, True)
		xmmsc_sc_namespace_new(ns, <char *>name, <char *>doc)

	cpdef register_constant(self, path, value, infos):
		cdef xmmsc_sc_namespace_t *ns
		cdef xmmsv_t *val
		name = from_unicode(infos.get('name', ""))
		if not name:
			return
		ns = _namespace_get(self.xmms.conn, path, True)
		val = create_native_value(value)
		xmmsc_sc_namespace_add_constant(ns, <char *>name, val)
		xmmsv_unref(val)

	cpdef register_method(self, path, meth, infos):
		cdef xmmsc_sc_namespace_t *ns
		cdef xmmsv_t *positional
		cdef xmmsv_t *named
		cdef xmmsv_t *x_arg
		cdef bint va_pos
		cdef bint va_named
		cdef xmmsv_t *a_default
		cdef xmmsv_type_t a_type

		positional = xmmsv_new_list()
		named = xmmsv_new_list()

		name = from_unicode(infos.get('name') or "")
		if not name:
			return
		doc = from_unicode(infos.get('doc') or "")
		for arg in infos.get('positional', ()):
			if isinstance(arg, method_varg):
				va_pos = True
				break
			if hasattr(arg, 'default'):
				a_default = create_native_value(arg.default)
			else:
				a_default = NULL
			a_type = arg._x_type
			a_name = from_unicode(arg.name)
			a_doc = from_unicode(arg.doc)
			x_arg = xmmsv_sc_argument_new(<char *>a_name, <char *>a_doc, a_type, a_default)
			xmmsv_list_append(positional, x_arg)
			xmmsv_unref(x_arg)

		for arg in infos.get('named', ()):
			if isinstance(arg, method_varg):
				va_named = True
				break
			if hasattr(arg, 'default'):
				a_default = create_native_value(arg.default)
			else:
				a_default = NULL
			a_type = arg._x_type
			a_name = from_unicode(arg.name)
			a_doc = from_unicode(arg.doc)
			x_arg = xmmsv_sc_argument_new(<char *>a_name, <char *>a_doc, a_type, a_default)
			xmmsv_list_append(named, x_arg)
			xmmsv_unref(x_arg)

		ns = _namespace_get(self.xmms.conn, path, True)

		# We need this to prevent bound function wrapper to be recycled on
		# return, leading to weird behaviour
		self._bound_methods[name] = meth

		xmmsc_sc_namespace_add_method(ns, service_method_proxy, <char *>name, <char *>doc, positional, named, va_pos, va_named, <void *>meth)
		xmmsv_unref(positional)
		xmmsv_unref(named)

	cpdef register_broadcast(self, path, bc, infos):
		cdef xmmsc_sc_namespace_t *ns
		name = from_unicode(infos.get('name') or "")
		if not name:
			return
		doc = from_unicode(infos.get('doc') or "")
		ns = _namespace_get(self.xmms.conn, path, True)
		xmmsc_sc_namespace_add_broadcast(ns, <char *>name, <char *>doc)

	cpdef register(self):
		if self.registered:
			return

		self.xmms.sc_init()

		_path = self.path
		self.register_namespace(_path[:-1], self, dict(
			name = _path and _path[-1] or "",
			doc = self.__doc__ or ""
			))

		self._walk( path = _path,
			namespace = self.register_namespace,
			constant = self.register_constant,
			method = self.register_method,
			broadcast = self.register_broadcast)

		self.xmms.c2c_ready()

		self.registered = True


cdef xmmsc_sc_namespace_t *_namespace_get(xmmsc_connection_t *c, path, bint create):
	cdef xmmsc_sc_namespace_t *ns
	cdef xmmsc_sc_namespace_t *child_ns

	ns = xmmsc_sc_namespace_root(c)

	for p in path:
		p = from_unicode(p)
		doc = from_unicode("")
		child_ns = xmmsc_sc_namespace_get(ns, <char *>p)
		if child_ns == NULL:
			if not create:
				break
			child_ns = xmmsc_sc_namespace_new(ns, <char *>p, <char *>doc)
		ns = child_ns

	return ns

cdef xmmsv_t *service_method_proxy(xmmsv_t *pargs, xmmsv_t *nargs, void *udata):
	cdef object method
	cdef XmmsValue args
	cdef XmmsValue kargs
	cdef xmmsv_t *res

	method = <object> udata
	args = XmmsValue()
	args.set_value(pargs)
	kargs = XmmsValue()
	kargs.set_value(nargs)
	try:
		ret = method(*args.get_list(), **kargs.get_dict())
		res = create_native_value(ret)
	except:
		import traceback, sys
		exc = sys.exc_info()
		traceback.print_exception(*exc)
		s = from_unicode("%s"%exc[1])
		res = xmmsv_new_error(<char *>s)
	return res


cdef class client_broadcast:
	def __cinit__(self, _XmmsServiceClient parent, name, docstring):
		if not parent._async:
			raise RuntimeError("Broadcast is available only on asynchronous connections")
		self._xmms = parent._xmms
		self._clientid = parent._clientid
		self._path = parent._path + (name,)
		self.name = name
		self.docstring = docstring

	def subscribe(self, cb = None):
		return self._xmms.sc_broadcast_subscribe(self._clientid, self._path, cb = cb)

	def __call__(self, cb = None):
		return self.subscribe(cb = cb)

	def __repr__(self):
		return "<broadcast '%s' on client #%d>" % (".".join(self._path), self._clientid)


cdef class client_method:
	def __cinit__(self, _XmmsServiceClient parent, name, docstring, inspect, cb = None):
		self._parent = parent
		self._callback = cb
		self._path = parent._path + (name,)
		self._clientid = parent._clientid
		self.name = name
		self.docstring = docstring
		self.inspect = inspect

	def with_callback(self, cb):
		if not self._parent._async:
			raise NotImplementedError()
		return client_method(self._parent, self.name, self.docstring, self.inspect, cb)

	def call(self, args, kargs, cb = None):
		res = self._parent._xmms.sc_call(self._clientid, self._path, args, kargs, cb = self._parent._async and cb or None)

		if self._parent._async:
			return res

		res.wait()
		xvalue = res.xvalue
		if xvalue.is_error():
			raise xvalue.get_error()
		cxvalue = XmmsValueC2C(pyval = xvalue)
		payload = cxvalue.payload
		if payload is not None:
			if payload.is_error():
				raise payload.get_error()
			payload = payload.value()
		return payload

	def __call__(self, *args, **kargs):
		return self.call(args, kargs, cb = self._callback)

	def __repr__(self):
		return "<method '%s' on client #%d>" % (".".join(self._path), self._clientid)


cdef class _XmmsServiceClient:
	def __cinit__(self, xmms, int clientid, *a, **ka):
		cdef XmmsProxy _proxy

		if isinstance(xmms, XmmsCore):
			self._xmms = xmms
			self._async = ka.get('_async', True)
		elif isinstance(xmms, XmmsProxy):
			_proxy = xmms
			self._xmms = _proxy._get_xmms()
			self._async = False
		else:
			raise TypeError("Not a valid Xmms client instance")

		self._path = tuple(ka.get('path', ()))
		self._clientid = clientid

	def __repr__(self):
		return "<namespace '%s' on client #%d>" % (".".join(self._path), self._clientid)

class XmmsServiceClient(_XmmsServiceClient):
	def _update_api(self, xvalue, recursive = False, cb = None):
		cdef XmmsValueC2C cxvalue
		cxvalue = XmmsValueC2C(pyval = xvalue)

		if cxvalue.sender != self._clientid:
			# ignore bad sender
			return

		payload = cxvalue.payload
		if payload and payload.get_type() == XMMSV_TYPE_DICT:
			payload = payload.get_dict()
		else:
			return

		self.__doc__ = payload.get('docstring', '')

		try:
			for c, v in payload.get('constants', {}).items():
				if hasattr(self.__class__, c):
					continue
				setattr(self, c, v)
		except (AttributeError, TypeError):
			pass

		if self._async:
			# Expose broadcasts only for asynchronous connections.
			for bc in payload.get('broadcasts', []):
				try:
					name = bc['name']
					doc = bc.get('docstring')
				except (TypeError, AttributeError, KeyError):
					continue
				if hasattr(self.__class__, name):
					continue
				setattr(self, name, client_broadcast(self, name, doc))

		for m in payload.get('methods', []):
			try:
				name = m['name']
				doc = m.get('docstring')
			except (TypeError, AttributeError, KeyError):
				continue
			if hasattr(self.__class__, name):
				continue
			setattr(self, name, client_method(self, name, doc, m))

		for ns in payload.get('namespaces', []):
			if hasattr(self.__class__, ns):
				continue
			sc = XmmsServiceClient(self._xmms, self._clientid,
					path = self._path + (ns,),
					_async = self._async)
			setattr(self, ns, sc)
			if recursive:
				sc(recursive, cb)

	def _update_api_callback(self, cb = None, recursive = False):
		def _update_api(xvalue):
			self._update_api(xvalue, recursive, cb)
			if cb:
				cb(self, xvalue)
		return _update_api

	def __call__(self, recursive = False, cb = None):
		cdef XmmsResult res
		cb = self._update_api_callback(cb, recursive)
		res = self._xmms.sc_introspect_namespace(self._clientid, self._path, cb = self._async and cb or None)

		if self._async:
			return res

		res.wait()
		xvalue = res.xvalue
		if xvalue.is_error():
			raise xvalue.get_error()
		cb(xvalue)


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
		return o
	else:
		try:
			return unicode(o, "UTF-8")
		except:
			return o

cdef object check_playlist(object pls, bint None_is_active):
	if pls is None and not None_is_active:
		raise TypeError("expected str, %s found" % pls.__class__.__name__)
	return from_unicode(pls or ACTIVE_PLAYLIST)

cdef class XmmsProxy:
	def __init__(self, xmms):
		self._xmms = xmms

	cdef XmmsCore _get_xmms(self):
		return self._xmms

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
		if not clientname:
			clientname = "UnnamedPythonClient"
		self.clientname = clientname
		default_sp = get_default_source_pref()
		default_sp.insert(0, enforce_unicode("client/"+clientname))
		self.source_preference = XmmsSourcePreference(default_sp)
		self.result_tracker = XmmsResultTracker() # Keep track of all results that set a notifier.
		self.new_connection()

	def __init__(self, clientname = None):
		pass

	def __dealloc__(self):
		if self.conn != NULL:
			xmmsc_unref(self.conn)
			self.conn = NULL

	def __del__(self):
		self.disconnect()

	cdef new_connection(self):
		cn = from_unicode(self.clientname)
		self.conn = xmmsc_init(<char *>cn)
		if self.conn == NULL:
			raise ValueError("Failed to initialize xmmsclient library! Probably due to broken name.")

	cpdef get_source_preference(self):
		return self.source_preference.get()
	cpdef set_source_preference(self, sources):
		self.source_preference.set(sources)

	cpdef _needout_cb(self, int i):
		if self.needout_fun:
			self.needout_fun(i)
	cpdef _disconnect_cb(self):
		if self.disconnect_fun:
			self.disconnect_fun(self)

	cpdef disconnect(self):
		# Disconnect all results.
		self.result_tracker.disconnect_all(True)
		if self.conn:
			xmmsc_unref(self.conn)
			self.conn = NULL
		self.new_connection()

	cpdef ioin(self):
		"""
		Read data from the daemon, when available.
		Note: This is a low level function that should only be used in
		certain circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_in_handle(self.conn)

	cpdef ioout(self):
		"""
		Write data out to the daemon, when available. Note: This is a
		low level function that should only be used in certain
		circumstances. e.g. a custom event loop
		"""
		return xmmsc_io_out_handle(self.conn)

	cpdef want_ioout(self):
		"""
        Check if there's data to send to the server.
		"""
		return xmmsc_io_want_out(self.conn)

	cpdef set_need_out_fun(self, fun):
		"""
        Set a function to be called when there's data to be
        sent to the server.
		"""
		self.needout_fun = fun

	cpdef get_fd(self):
		"""
		Get the underlying file descriptor used for communication with
		the XMMS2 daemon. You can use this in a client to ensure that
		the IPC link is still active and safe to use.(e.g by calling
		select() or poll())
		@rtype: int
		@return: IPC file descriptor
		"""
		return xmmsc_io_fd_get(self.conn)

	cpdef connect(self, path = None, disconnect_func = None):
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
			p = from_unicode(path)
			ret = xmmsc_connect(self.conn, <char *>p)
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

	cdef XmmsResult _create_result(self, cb, xmmsc_result_t *res, Cls):
		cdef XmmsResult ret
		if res == NULL:
			raise RuntimeError("xmmsc_result_t couldn't be allocated")
		ret = Cls()
		ret.set_sourcepref(self.source_preference)
		ret.set_result(res)
		if cb is not None:
			ret.set_callback(self.result_tracker, cb) # property that setup all.
		return ret

	cdef XmmsResult create_result(self, cb, xmmsc_result_t *res):
		return self._create_result(cb, res, XmmsResult)

	cdef XmmsResult create_vis_result(self, cb, xmmsc_result_t *res, VisResultCommand cmd):
		cdef XmmsVisResult vres = self._create_result(cb, res, XmmsVisResult)
		vres.set_command(cmd, self.conn)
		return vres


cdef class XmmsApi(XmmsCore):
	property client_id:
		def __get__(self):
			return self.c2c_get_own_id()

	cpdef int c2c_get_own_id(self):
		return xmmsc_c2c_get_own_id(self.conn)

	cpdef XmmsResult quit(self, cb = None):
		"""
		Tell the XMMS2 daemon to quit.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_quit(self.conn))

	cpdef XmmsResult plugin_list(self, typ, cb = None):
		"""
		Get a list of loaded plugins from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_main_list_plugins(self.conn, typ))

	cpdef XmmsResult playback_start(self, cb = None):
		"""
		Instruct the XMMS2 daemon to start playing the currently
		selected file from the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_start(self.conn))

	cpdef XmmsResult playback_stop(self, cb = None):
		"""
		Instruct the XMMS2 daemon to stop playing the file
		currently being played.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_stop(self.conn))

	cpdef XmmsResult playback_tickle(self, cb = None):
		"""
		Instruct the XMMS2 daemon to move on to the next playlist item.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_tickle(self.conn))

	cpdef XmmsResult playback_pause(self, cb = None):
		"""
		Instruct the XMMS2 daemon to pause playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playback_pause(self.conn))

	cpdef XmmsResult playback_current_id(self, cb = None):
		"""
		@rtype: L{XmmsResult}(UInt)
		@return: The medialib id of the item currently selected.
		"""
		return self.create_result(cb, xmmsc_playback_current_id(self.conn))

	cpdef XmmsResult playback_seek_ms(self, int ms, xmms_playback_seek_mode_t whence = PLAYBACK_SEEK_SET, cb = None):
		"""
		Seek to a time position in the current file or stream in playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		if whence == PLAYBACK_SEEK_SET or whence == PLAYBACK_SEEK_CUR:
			return self.create_result(cb, xmmsc_playback_seek_ms(self.conn, ms, whence))
		else:
			raise ValueError("Bad whence parameter")

	cpdef XmmsResult playback_seek_samples(self, int samples, xmms_playback_seek_mode_t whence = PLAYBACK_SEEK_SET, cb = None):
		"""
		Seek to a number of samples in the current file or stream in playback.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		if whence == PLAYBACK_SEEK_SET or whence == PLAYBACK_SEEK_CUR:
			return self.create_result(cb, xmmsc_playback_seek_samples(self.conn, samples, whence))
		else:
			raise ValueError("Bad whence parameter")

	cpdef XmmsResult playback_status(self, cb = None):
		"""
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
		Set a method to handle the playback status broadcast from the
		XMMS2 daemon.
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_status(self.conn))

	cpdef XmmsResult broadcast_playback_current_id(self, cb = None):
		"""
		Set a method to handle the playback id broadcast from the
		XMMS2 daemon.
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_current_id(self.conn))

	cpdef XmmsResult playback_playtime(self, cb = None):
		"""
		Return playtime on current file/stream. This is essentially a
		more direct version of L{signal_playback_playtime}
		@rtype: L{XmmsResult}(UInt)
		@return: The result of the operation.(playtime in milliseconds)
		"""
		return self.create_result(cb, xmmsc_playback_playtime(self.conn))

	cpdef XmmsResult signal_playback_playtime(self, cb = None):
		"""
		Set a method to handle the playback playtime signal from the
		XMMS2 daemon.
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_signal_playback_playtime(self.conn))

	cpdef XmmsResult playback_volume_set(self, channel, int volume, cb = None):
		"""
		Set the playback volume for specified channel
		@rtype: L{XmmsResult}(UInt)
		"""
		c = from_unicode(channel)
		return self.create_result(cb, xmmsc_playback_volume_set(self.conn, c, volume))

	cpdef XmmsResult playback_volume_get(self, cb = None):
		"""
		Get the playback for all channels
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_playback_volume_get(self.conn))

	cpdef XmmsResult broadcast_playback_volume_changed(self, cb = None):
		"""
		Set a broadcast callback for volume updates
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playback_volume_changed(self.conn))

	cpdef XmmsResult broadcast_playlist_loaded(self, cb = None):
		"""
		Set a broadcast callback for loaded playlist event
		@rtype: L{XmmsResult}(UInt)
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_loaded(self.conn))

	cpdef XmmsResult playlist_load(self, playlist, cb = None):
		"""
		Load the playlist as current playlist
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, False)
		return self.create_result(cb, xmmsc_playlist_load(self.conn, <char *>p))

	cpdef XmmsResult playlist_list(self, cb = None):
		"""
		Lists the playlists
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_playlist_list(self.conn))

	cpdef XmmsResult playlist_remove(self, playlist, cb = None):
		"""
		Remove the playlist from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, False)
		return self.create_result(cb, xmmsc_playlist_remove(self.conn, <char *>p))

	cpdef XmmsResult playlist_shuffle(self, playlist = None, cb = None):
		"""
		Instruct the XMMS2 daemon to shuffle the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_shuffle(self.conn, <char *>p))

	cpdef XmmsResult playlist_rinsert(self, int pos, url, playlist = None, cb = None, encoded = False):
		"""
		Insert a directory in the playlist.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		c = from_unicode(url)
		p = check_playlist(playlist, True)

		if encoded:
			res = xmmsc_playlist_rinsert_encoded(self.conn, <char *>p, pos, <char *>c)
		else:
			res = xmmsc_playlist_rinsert(self.conn, <char *>p, pos, <char *>c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_rinsert_encoded(self, int pos, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_rinsert(pos, url, ..., encoded = True) instead
		"""
		return self.playlist_rinsert(pos, url, playlist, cb = cb, encoded = True)

	cpdef XmmsResult playlist_insert_url(self, int pos, url, playlist = None, cb = None, encoded = False):
		"""
		Insert a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires an int 'pos' and a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		c = from_unicode(url)
		p = check_playlist(playlist, True)

		if encoded:
			res = xmmsc_playlist_insert_encoded(self.conn, <char *>p, pos, <char *>c)
		else:
			res = xmmsc_playlist_insert_url(self.conn, <char *>p, pos, <char *>c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_insert_encoded(self, int pos, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_insert_url(pos, url, ..., encoded = True) instead
		"""
		return self.playlist_insert_url(pos, url, playlist, cb = cb, encoded = True)


	cpdef XmmsResult playlist_insert_id(self, int pos, int id, playlist = None, cb = None):
		"""
		Insert a medialib to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_insert_id(self.conn, <char *>p, pos, id))


	cpdef XmmsResult playlist_insert_collection(self, int pos, Collection coll, order = None, playlist = None, cb = None):
		"""
		Insert the content of a collection to the playlist.
		Requires an int 'pos' and an int 'id' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *order_val
		cdef xmmsc_result_t *res

		if order is None:
			order = []
		p = check_playlist(playlist, True)
		order_val = create_native_value(order)
		res = xmmsc_playlist_insert_collection(self.conn, <char *>p, pos, coll.coll, order_val)
		xmmsv_unref(order_val)
		return self.create_result(cb, res)

	cpdef XmmsResult playlist_radd(self, url, playlist = None, cb = None, encoded = False):
		"""
		Add a directory to the playlist.
		Requires a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		c = from_unicode(url)
		p = check_playlist(playlist, True)
		if encoded:
			res = xmmsc_playlist_radd_encoded(self.conn, <char *>p, <char *>c)
		else:
			res = xmmsc_playlist_radd(self.conn, <char *>p, <char *>c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_radd_encoded(self, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_radd(url, ..., encoded = True) instead
		"""
		return self.playlist_radd(url, playlist, cb = cb, encoded = True)

	cpdef XmmsResult playlist_add_url(self, url, playlist = None, cb = None, encoded = False):
		"""
		Add a path or URL to a playable media item to the playlist.
		Playable media items may be files or streams.
		Requires a string 'url' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		c = from_unicode(url)
		p = check_playlist(playlist, True)
		if encoded:
			res = xmmsc_playlist_add_encoded(self.conn, <char *>p, <char *>c)
		else:
			res = xmmsc_playlist_add_url(self.conn, <char *>p, <char *>c)
		return self.create_result(cb, res)

	@deprecated
	def playlist_add_encoded(self, url, playlist = None, cb = None):
		"""
		@deprecated
		Use playlist_add_url(url, ..., encoded = True) instead
		"""
		return self.playlist_add_url(url, playlist, cb = cb, encoded = True)

	cpdef XmmsResult playlist_add_id(self, int id, playlist = None, cb = None):
		"""
		Add a medialib id to the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_add_id(self.conn, <char *>p, id))

	cpdef XmmsResult playlist_add_collection(self, Collection coll, order = None, playlist = None, cb = None):
		"""
		Add the content of a collection to the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *order_val
		cdef xmmsc_result_t *res

		p = check_playlist(playlist, True)
		if order is None:
			order = []
		order_val = create_native_value(order)
		res = xmmsc_playlist_add_collection(self.conn, <char *>p, coll.coll, order_val)
		xmmsv_unref(order_val)
		return self.create_result(cb, res)

	cpdef XmmsResult playlist_remove_entry(self, int id, playlist = None, cb = None):
		"""
		Remove a certain media item from the playlist.
		Requires a number 'id' as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_remove_entry(self.conn, <char *>p, id))

	cpdef XmmsResult playlist_clear(self, playlist = None, cb = None):
		"""
		Clear the playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_clear(self.conn, <char *>p))

	cpdef XmmsResult playlist_list_entries(self, playlist = None, cb = None):
		"""
		Get the current playlist. This function returns a list of IDs
		of the files/streams currently in the playlist. Use
		L{medialib_get_info} to retrieve more specific information.
		@rtype: L{XmmsResult}(UIntList)
		@return: The current playlist.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_list_entries(self.conn, <char *>p))

	cpdef XmmsResult playlist_sort(self, props, playlist = None, cb = None):
		"""
		Sorts the playlist according to the properties specified.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *props_val
		cdef xmmsc_result_t *res

		p = check_playlist(playlist, True)
		props_val = create_native_value(props)
		res = xmmsc_playlist_sort(self.conn, <char *>p, props_val)
		xmmsv_unref(props_val)
		return self.create_result(cb, res)

	cpdef XmmsResult playlist_set_next_rel(self, int position, cb = None):
		"""
		Sets the position in the playlist. Same as L{playlist_set_next}
		but sets the next position relative to the current position.
		You can do set_next_rel(-1) to move backwards for example.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_playlist_set_next_rel(self.conn, position))

	cpdef XmmsResult playlist_set_next(self, int position, cb = None):
		"""
		Sets the position to move to, next, in the playlist. Calling
		L{playback_tickle} will perform the jump to that position.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_playlist_set_next(self.conn, position))

	cpdef XmmsResult playlist_move(self, int cur_pos, int new_pos, playlist = None, cb = None):
		"""
		Moves a playlist entry to a new position.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_move_entry(self.conn, <char *>p, cur_pos, new_pos))

	cpdef XmmsResult playlist_create(self, playlist, cb = None):
		"""
		Create a new playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = check_playlist(playlist, False)
		return self.create_result(cb, xmmsc_playlist_create(self.conn, <char *>p))

	cpdef XmmsResult playlist_current_pos(self, playlist = None, cb = None):
		"""
		Returns the current position in the playlist. This value will
		always be equal to, or larger than 0. The first entry in the
		list is 0.
		@rtype: L{XmmsResult}
		"""
		p = check_playlist(playlist, True)
		return self.create_result(cb, xmmsc_playlist_current_pos(self.conn, <char *>p))

	cpdef XmmsResult playlist_current_active(self, cb = None):
		"""
		Returns the name of the current active playlist
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_playlist_current_active(self.conn))

	cpdef XmmsResult broadcast_playlist_current_pos(self, cb = None):
		"""
		Set a method to handle the playlist current position updates
		from the XMMS2 daemon. This is triggered whenever the daemon
		jumps from one playlist position to another. (not when moving
		a playlist item from one position to another)
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_current_pos(self.conn))

	cpdef XmmsResult broadcast_playlist_changed(self, cb = None):
		"""
		Set a method to handle the playlist changed broadcast from the
		XMMS2 daemon. Updated data is sent whenever the daemon's
		playlist changes.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_playlist_changed(self.conn))

	cpdef XmmsResult broadcast_config_value_changed(self, cb = None):
		"""
		Set a method to handle the config value changed broadcast
		from the XMMS2 daemon.(i.e. some configuration value has
		been modified) Updated data is sent whenever a config
		value is modified.
		@rtype: L{XmmsResult} (the modified config key and its value)
		"""
		return self.create_result(cb, xmmsc_broadcast_config_value_changed(self.conn))

	cpdef XmmsResult config_set_value(self, key, val, cb = None):
		"""
		Set a configuration value on the daemon, given a key.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		k = from_unicode(key)
		v = from_unicode(val)
		return self.create_result(cb, xmmsc_config_set_value(self.conn, <char *>k, <char *>v))

	cpdef XmmsResult config_get_value(self, key, cb = None):
		"""
		Get the configuration value of a given key, from the daemon.
		@rtype: L{XmmsResult}(String)
		@return: The result of the operation.
		"""
		k = from_unicode(key)
		return self.create_result(cb, xmmsc_config_get_value(self.conn, <char *>k))

	cpdef XmmsResult config_list_values(self, cb = None):
		"""
		Get list of configuration keys on the daemon. Use
		L{config_get_value} to retrieve the values corresponding to the
		configuration keys.
		@rtype: L{XmmsResult}(StringList)
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_config_list_values(self.conn))

	cpdef XmmsResult config_register_value(self, valuename, defaultvalue, cb = None):
		"""
		Register a new configvalue.
		This should be called in the initcode as XMMS2 won't allow
		set/get on values that haven't been registered.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		v = from_unicode(valuename)
		dv = from_unicode(defaultvalue)
		return self.create_result(cb, xmmsc_config_register_value(self.conn, <char *>v, <char *>dv))

	cpdef XmmsResult medialib_add_entry(self, path, cb = None, encoded = False):
		"""
		Add an entry to the MediaLib.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res
		p = from_unicode(path)
		if encoded:
			res = xmmsc_medialib_add_entry_encoded(self.conn, <char *>p)
		else:
			res = xmmsc_medialib_add_entry(self.conn, <char *>p)
		return self.create_result(cb, res)

	@deprecated
	def medialib_add_entry_encoded(self, path, cb = None):
		"""
		@deprecated
		Use medialib_add_entry(file, ..., encoded = True) instead
		"""
		return self.medialib_add_entry(path, cb = cb, encoded = True)

	cpdef XmmsResult medialib_remove_entry(self, int id, cb = None):
		"""
		Remove an entry from the medialib.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_remove_entry(self.conn, id))

	cpdef XmmsResult medialib_move_entry(self, int id,  url, cb = None, encoded = False):
		"""
		Set a new url for an entry in the medialib.
		@rtype: L{XmmsResult}
		@return The result of the operation.
		"""
		if encoded:
			try:
				from urllib import unquote_plus
			except ImportError: #Py3k
				from urllib.parse import unquote_plus
			url = unquote_plus(url)
		u = from_unicode(url)
		return self.create_result(cb, xmmsc_medialib_move_entry(self.conn, id, <char *>u))

	cpdef XmmsResult medialib_get_info(self, int id, cb = None):
		"""
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
		Force the medialib to check that metadata stored is up to
		date.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_medialib_rehash(self.conn, id))

	cpdef XmmsResult medialib_get_id(self, url, cb = None, encoded = False):
		"""
		Search for an entry (URL) in the medialib and return its ID
		number.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res
		u = from_unicode(url)
		if encoded:
			res = xmmsc_medialib_get_id_encoded(self.conn, <char *>u)
		else:
			res = xmmsc_medialib_get_id(self.conn, <char *>u)
		return self.create_result(cb, res)

	cpdef XmmsResult medialib_import_path(self, path, cb = None, encoded = False):
		"""
		Import metadata from all files recursively from the directory
		passed as argument.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res
		p = from_unicode(path)
		if encoded:
			res = xmmsc_medialib_import_path_encoded(self.conn, <char *>p)
		else:
			res = xmmsc_medialib_import_path(self.conn, <char *>p)
		return self.create_result(cb, res)

	@deprecated
	def medialib_path_import(self, path, cb = None, encoded = False):
		"""
		@deprecated
		Use medialib_import_path(path, ...) instead
		"""
		return self.medialib_import_path(path, cb = cb, encoded = encoded)

	@deprecated
	def medialib_path_import_encoded(self, path, cb = None):
		"""
		@deprecated
		Use medialib_import_path(path, ..., encoded = True) instead
		"""
		return self.medialib_import_path(path, cb = cb, encoded = True)

	cpdef XmmsResult medialib_property_set(self, int id, key, value, source = None, cb = None):
		"""
		Associate a value with a medialib entry. Source is optional.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res
		k = from_unicode(key)
		if isinstance(value, int):
			if source:
				s = from_unicode(source)
				res = xmmsc_medialib_entry_property_set_int_with_source(self.conn, id, <char *>s, <char *>k, value)
			else:
				res = xmmsc_medialib_entry_property_set_int(self.conn, id, <char *>k, value)
		else:
			v = from_unicode(value)
			if source:
				s = from_unicode(source)
				res = xmmsc_medialib_entry_property_set_str_with_source(self.conn, id, <char *>s, <char *>k, <char *>v)
			else:
				res = xmmsc_medialib_entry_property_set_str(self.conn, id, <char *>k, <char *>v)
		return self.create_result(cb, res)

	cpdef XmmsResult medialib_property_remove(self, int id, key, source = None, cb = None):
		"""
		Remove a value from a medialib entry. Source is optional.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res
		k = from_unicode(key)
		if source:
			s = from_unicode(source)
			res = xmmsc_medialib_entry_property_remove_with_source(self.conn, id, <char *>s, <char *>k)
		else:
			res = xmmsc_medialib_entry_property_remove(self.conn, id, <char *>k)
		return self.create_result(cb, res)

	cpdef XmmsResult broadcast_medialib_entry_added(self, cb = None):
		"""
		Set a method to handle the medialib entry added broadcast
		from the XMMS2 daemon. (i.e. a new entry has been added)
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_added(self.conn))

	@deprecated
	def broadcast_medialib_entry_changed(self, cb = None):
		"""
		@deprecated
		Use broadcast_medialib_entry_updated(self, cb = None) instead
		"""
		return self.broadcast_medialib_entry_updated(cb)

	cpdef XmmsResult broadcast_medialib_entry_updated(self, cb = None):
		"""
		Set a method to handle the medialib entry updated broadcast
		from the XMMS2 daemon.
		Updated data is sent when the metadata for a song is updated
		in the medialib.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_updated(self.conn))

	cpdef XmmsResult broadcast_medialib_entry_removed(self, cb = None):
		"""
		Set a method to handle the medialib entry removed broadcast
		from the XMMS2 daemon. (i.e. an entry has been removed)
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_medialib_entry_removed(self.conn))

	cpdef XmmsResult broadcast_collection_changed(self, cb = None):
		"""
		Set a method to handle the collection changed broadcast
		from the XMMS2 daemon.
		@rtype: L{XmmsResult}
		"""
		return self.create_result(cb, xmmsc_broadcast_collection_changed(self.conn))

	cpdef XmmsResult signal_mediainfo_reader_unindexed(self, cb = None):
		"""
		Tell daemon to send you the number of unindexed files in the mlib
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_signal_mediainfo_reader_unindexed(self.conn))

	cpdef XmmsResult broadcast_mediainfo_reader_status(self, cb = None):
		"""
		Tell daemon to send you the status of the mediainfo reader
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_broadcast_mediainfo_reader_status(self.conn))

	cpdef XmmsResult xform_media_browse(self, url, cb = None, encoded = False):
		"""
		Browse files from xform plugins.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res
		u = from_unicode(url)
		if encoded:
			res = xmmsc_xform_media_browse_encoded(self.conn, <char *>u)
		else:
			res = xmmsc_xform_media_browse(self.conn, <char *>u)
		return self.create_result(cb, res)

	@deprecated
	def xform_media_browse_encoded(self, url, cb = None):
		"""
		@deprecated
		Use xform_media_browse(url, ..., encoded = True) instead
		"""
		return self.xform_media_browse(url, cb = cb, encoded = True)

	cpdef XmmsResult coll_get(self, name, ns = "Collections", cb = None):
		"""
		Retrieve a Collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		n = check_namespace(ns, False)
		nam = from_unicode(name)
		return self.create_result(cb, xmmsc_coll_get(self.conn, nam, n))

	cpdef XmmsResult coll_list(self, ns = "Collections", cb = None):
		"""
		List collections
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		n = check_namespace(ns, False)
		return self.create_result(cb, xmmsc_coll_list(self.conn, n))

	cpdef XmmsResult coll_save(self, Collection coll, name, ns = "Collections", cb = None):
		"""
		Save a collection on server.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		n = check_namespace(ns, False)
		nam = from_unicode(name)
		return self.create_result(cb, xmmsc_coll_save(self.conn, coll.coll, <char *>nam, n))

	cpdef XmmsResult coll_remove(self, name, ns = "Collections", cb = None):
		"""
		Remove a collection on server.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		n = check_namespace(ns, False)
		nam = from_unicode(name)
		return self.create_result(cb, xmmsc_coll_remove(self.conn, <char *>nam, n))

	cpdef XmmsResult coll_rename(self, oldname, newname, ns = "Collections", cb = None):
		"""
		Rename a collection.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *n
		n = check_namespace(ns, False)

		oldnam = from_unicode(oldname)
		newnam = from_unicode(newname)
		return self.create_result(cb, xmmsc_coll_rename(self.conn, <char *>oldnam, <char *>newnam, n))

	cpdef XmmsResult coll_idlist_from_playlist_file(self, path, cb = None):
		"""
		Create an idlist from a playlist.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		p = from_unicode(path)
		return self.create_result(cb, xmmsc_coll_idlist_from_playlist_file(self.conn, <char *>p))

	cpdef XmmsResult coll_query(self, Collection coll, fetch, cb = None):
		"""
		Retrive a list of ids of the media matching the collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *fetch_val
		fetch_val = create_native_value(fetch)
		res = self.create_result(cb, xmmsc_coll_query(self.conn, coll.coll, fetch_val))
		return res

	cpdef XmmsResult coll_query_ids(self, Collection coll, start = 0, leng = 0, order = None, cb = None):
		"""
		Retrive a list of ids of the media matching the collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *order_val
		cdef xmmsc_result_t *res

		if order is None:
			order = []
		order_val = create_native_value(order)
		res = xmmsc_coll_query_ids(self.conn, coll.coll, order_val, start, leng)
		xmmsv_unref(order_val)
		return self.create_result(cb, res)

	cpdef XmmsResult coll_query_infos(self, Collection coll, fields, start = 0, leng = 0, order = None, groupby = None, cb = None):
		"""
		Retrive a list of mediainfo of the media matching the collection
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *order_val
		cdef xmmsv_t *fields_val
		cdef xmmsv_t *groupby_val
		cdef xmmsc_result_t *res

		if order is None:
			order = []
		if groupby is None:
			groupby = []
		order_val = create_native_value(order)
		fields_val = create_native_value(fields)
		groupby_val = create_native_value(groupby)
		res = xmmsc_coll_query_infos(self.conn, coll.coll, order_val, start, leng, fields_val, groupby_val)
		xmmsv_unref(order_val)
		xmmsv_unref(fields_val)
		xmmsv_unref(groupby_val)
		return self.create_result(cb, res)

	cpdef XmmsResult c2c_ready(self, cb = None):
		"""
		Notify the server that client services are ready for query.
		This method is called XmmsServiceNamespace.register() and don't need to
		be called explicitly.

		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		if not cb:
			cb = _noop

		res = xmmsc_c2c_ready(self.conn)
		return self.create_result(cb, res)

	cpdef XmmsResult c2c_get_connected_clients(self, cb = None):
		"""
		Get a list of clients connected to the xmms2 server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		res = xmmsc_c2c_get_connected_clients (self.conn)
		return self.create_result(cb, res)

	cpdef XmmsResult c2c_get_ready_clients(self, cb = None):
		"""
		Get a list of clients connected to the xmms2 server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		res = xmmsc_c2c_get_ready_clients (self.conn)
		return self.create_result(cb, res)

	cpdef XmmsResult broadcast_c2c_ready(self, cb = None):
		"""
		Broadcast reveiced whenever a client's service api is ready
		@rtype: L{XmmsResult}
		@return: the result of the operation.
		"""
		cdef xmmsc_result_t *res

		res = xmmsc_broadcast_c2c_ready(self.conn)
		return self.create_result(cb, res)


	cpdef XmmsResult broadcast_c2c_client_connected(self, cb = None):
		"""
		Broadcast received whenever a new client connects to the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		res = xmmsc_broadcast_c2c_client_connected(self.conn)
		return self.create_result(cb, res)

	cpdef XmmsResult broadcast_c2c_client_disconnected(self, cb = None):
		"""
		Broadcast received whenever a client disconnects from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsc_result_t *res

		res = xmmsc_broadcast_c2c_client_disconnected(self.conn)
		return self.create_result(cb, res)

	cpdef bint sc_init(self):
		"""
		Initialize client-to-client features.
		Client-to-client features won't work with the synchronous client wrapper
		@rtype: L{bool}
		@return: whether client-to-client is initialized.
		"""
		return xmmsc_sc_init(self.conn) != NULL

	cpdef bint sc_broadcast_emit(self, broadcast, value = None):
		"""
		Emit a broadcast message to subscribed clients
		@rtype: L{bool}
		@return: whether notifications were successfully queued
		"""
		cdef xmmsv_t *val
		cdef xmmsv_t *bc_path
		cdef bint ret

		bc_path = create_native_value(broadcast)
		val = create_native_value(value)
		ret = xmmsc_sc_broadcast_emit(self.conn, bc_path, val)
		xmmsv_unref(bc_path)
		xmmsv_unref(val)
		return ret

	cpdef XmmsResult sc_broadcast_subscribe(self, int dest, broadcast, cb = None):
		"""
		Subscribe to a broadcast from another client
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *bc_path
		cdef xmmsc_result_t *res

		bc_path = create_native_value(broadcast)
		res = xmmsc_sc_broadcast_subscribe(self.conn, dest, bc_path)
		xmmsv_unref(bc_path)
		return self.create_result(cb, res)

	cpdef XmmsResult sc_call(self, int dest, method, args = (), kargs = dict(), cb = None):
		"""
		Call a remote method
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *m_path
		cdef xmmsv_t *m_pos
		cdef xmmsv_t *m_named
		cdef xmmsc_result_t *res

		m_path = create_native_value(method)
		m_pos = create_native_value(args)
		m_named = create_native_value(kargs)
		res = xmmsc_sc_call(self.conn, dest, m_path, m_pos, m_named)
		xmmsv_unref(m_path)
		xmmsv_unref(m_pos)
		xmmsv_unref(m_named)
		return self.create_result(cb, res)

	cpdef XmmsResult sc_introspect_namespace(self, int dest, path = (), cb = None):
		"""
		Get informations about a namespace on a remote client
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *i_path
		cdef xmmsc_result_t *res

		i_path = create_native_value(path)
		res = xmmsc_sc_introspect_namespace(self.conn, dest, i_path)
		xmmsv_unref(i_path)
		return self.create_result(cb, res)

	cpdef XmmsResult sc_introspect_method(self, int dest, path, cb = None):
		"""
		Get informations about a method on a remote client
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *i_path
		cdef xmmsc_result_t *res

		i_path = create_native_value(path)
		res = xmmsc_sc_introspect_method(self.conn, dest, i_path)
		xmmsv_unref(i_path)
		return self.create_result(cb, res)

	cpdef XmmsResult sc_introspect_broadcast(self, int dest, path, cb = None):
		"""
		Get informations about a broadcast on a remote client
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *i_path
		cdef xmmsc_result_t *res

		i_path = create_native_value(path)
		res = xmmsc_sc_introspect_broadcast(self.conn, dest, i_path)
		xmmsv_unref(i_path)
		return self.create_result(cb, res)

	cpdef XmmsResult sc_introspect_constant(self, int dest, path, cb = None):
		"""
		Get informations about a constant on a remote client
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *i_path
		cdef xmmsc_result_t *res
		ns = path[:-1]
		key = from_unicode(path[-1])
		i_path = create_native_value(ns)
		res = xmmsc_sc_introspect_constant(self.conn, dest, i_path, <char *>key)
		xmmsv_unref(i_path)
		return self.create_result(cb, res)

	cpdef XmmsResult sc_introspect_docstring(self, int dest, path, cb = None):
		"""
		Get the docstring for a given path on a remote client
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef xmmsv_t *i_path
		cdef xmmsc_result_t *res

		i_path = create_native_value(path)
		res = xmmsc_sc_introspect_docstring(self.conn, dest, i_path)
		xmmsv_unref(i_path)
		return self.create_result(cb, res)

	cpdef XmmsResult bindata_add(self, data, cb = None):
		"""
		Add a datafile to the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		cdef char *t
		t = <char *>data
		return self.create_result(cb, xmmsc_bindata_add(self.conn,<unsigned char *>t,len(data)))

	cpdef XmmsResult bindata_retrieve(self, hash, cb = None):
		"""
		Retrieve a datafile from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		h = from_unicode(hash)
		return self.create_result(cb, xmmsc_bindata_retrieve(self.conn, <char *>h))

	cpdef XmmsResult bindata_remove(self, hash, cb = None):
		"""
		Remove a datafile from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		h = from_unicode(hash)
		return self.create_result(cb, xmmsc_bindata_remove(self.conn, <char *>h))

	cpdef XmmsResult bindata_list(self, cb = None):
		"""
		List all bindata hashes stored on the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_bindata_list(self.conn))

	cpdef XmmsResult stats(self, cb = None):
		"""
		Get statistics information from the server
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_main_stats(self.conn))

	cpdef XmmsResult visualization_version(self, cb = None):
		"""
		Get the version of the visualization plugin installed on the server.
		@rtype: L{XmmsResult}
		@return: The result of the operation.
		"""
		return self.create_result(cb, xmmsc_visualization_version(self.conn))

	cpdef XmmsResult visualization_init(self, cb = None):
		"""
		Get a new visualization handle.
		@rtype: L{XmmsResult}
		@return: The result of the operation
		"""
		return self.create_vis_result(cb, xmmsc_visualization_init(self.conn), VIS_RESULT_CMD_INIT)

	cpdef XmmsResult visualization_start(self, int handle, cb = None):
		"""
		Starts the visualization.
		@rtype: L{XmmsResult}
		@return: The result of the operation
		"""
		return self.create_vis_result(cb, xmmsc_visualization_start(self.conn, handle), VIS_RESULT_CMD_START)

	cpdef bint visualization_started(self, int handle):
		"""
		Whether the visualization is started or not.
		@rtype: L{bool}
		@return: True if the visualization is started, False otherwise.
		"""
		return xmmsc_visualization_started(self.conn, handle)

	cpdef bint visualization_errored(self, int handle):
		"""
		Whether the visualization got an error.
		@rtype: L{bool}
		@return: True if the visualization got an error, False otherwise.
		"""
		return xmmsc_visualization_errored(self.conn, handle)

	cpdef XmmsResult visualization_property_set(self, int handle, key, value, cb = None):
		"""
		Set a visualization's property.
		@rtype: L{bool}
		@return: The result of the operation
		"""
		k = from_unicode(key)
		v = from_unicode(value)
		return self.create_result(cb, xmmsc_visualization_property_set(self.conn, handle, <char *>k, <char *>v))

	cpdef XmmsResult visualization_properties_set(self, int handle, props = {}, cb = None):
		"""
		Set visualization's properties.
		@rtype: L{bool}
		@return: The result of the operation
		"""
		cdef xmmsv_t *_props
		cdef xmmsc_result_t *res

		_props = create_native_value(props)
		res = xmmsc_visualization_properties_set(self.conn, handle, _props)
		xmmsv_unref(_props)
		return self.create_result(cb, res)

	cpdef XmmsVisChunk visualization_chunk_get(self, int handle, int drawtime = 0, bint blocking = False):
		"""
		Fetches the next available data chunk
		@rtype: L{XmmsVisChunk}
		@return: Visualization chunk.
		"""
		cdef short *buf
		cdef int size
		cdef XmmsVisChunk chunk

		buf = <short *>PyMem_Malloc(2 * XMMSC_VISUALIZATION_WINDOW_SIZE * sizeof (short))
		size = xmmsc_visualization_chunk_get (self.conn, handle, buf, drawtime, blocking)
		if size < 0:
			PyMem_Free(buf)
			raise VisualizationError("Unrecoverable error in visualization")
		chunk = XmmsVisChunk()
		chunk.set_data(buf, size)
		PyMem_Free(buf)
		return chunk

	cpdef visualization_shutdown(self, int handle):
		"""
		Shutdown and destroy a visualization. After this, handle is no longer valid.
		"""
		xmmsc_visualization_shutdown(self.conn, handle)



class XmmsDisconnectException(Exception):
	pass

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
		Exits from the L{loop} call
		"""
		self.do_loop = False
		self.loop_tickle()

	def loop_tickle(self):
		from os import write
		w = self._loop_get_wakeup()
		if w is not None:
			write(w, "1".encode('ascii'))

	def loop_iter(self, infd = None, outfd = None, errfd = None, timeout = -1):
		"""
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
				(i, o, e) = self.loop_iter(infd = [r])
				if r in i:
					read(r, 1) # Purge wakeup stream (each wakeup signal should not write more than one byte)
			except XmmsDisconnectException:
				self.do_loop = False

		self._loop_set_wakeup(None)

Xmms = XmmsApi
