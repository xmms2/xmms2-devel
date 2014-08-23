"""
Python bindings for xmmsv manipulations.
"""

from xmmsutils cimport *
from cpython.bytes cimport PyBytes_FromStringAndSize
cimport cython
from cxmmsvalue cimport *

VALUE_TYPE_NONE   = XMMSV_TYPE_NONE
VALUE_TYPE_ERROR  = XMMSV_TYPE_ERROR
VALUE_TYPE_INT64  = XMMSV_TYPE_INT64
VALUE_TYPE_FLOAT  = XMMSV_TYPE_FLOAT
VALUE_TYPE_STRING = XMMSV_TYPE_STRING
VALUE_TYPE_COLL   = XMMSV_TYPE_COLL
VALUE_TYPE_BIN    = XMMSV_TYPE_BIN
VALUE_TYPE_LIST   = XMMSV_TYPE_LIST
VALUE_TYPE_DICT   = XMMSV_TYPE_DICT

COLLECTION_TYPE_REFERENCE    = XMMS_COLLECTION_TYPE_REFERENCE
COLLECTION_TYPE_UNIVERSE     = XMMS_COLLECTION_TYPE_UNIVERSE
COLLECTION_TYPE_UNION        = XMMS_COLLECTION_TYPE_UNION
COLLECTION_TYPE_INTERSECTION = XMMS_COLLECTION_TYPE_INTERSECTION
COLLECTION_TYPE_COMPLEMENT   = XMMS_COLLECTION_TYPE_COMPLEMENT
COLLECTION_TYPE_HAS          = XMMS_COLLECTION_TYPE_HAS
COLLECTION_TYPE_MATCH        = XMMS_COLLECTION_TYPE_MATCH
COLLECTION_TYPE_TOKEN        = XMMS_COLLECTION_TYPE_TOKEN
COLLECTION_TYPE_EQUALS       = XMMS_COLLECTION_TYPE_EQUALS
COLLECTION_TYPE_NOTEQUAL     = XMMS_COLLECTION_TYPE_NOTEQUAL
COLLECTION_TYPE_SMALLER      = XMMS_COLLECTION_TYPE_SMALLER
COLLECTION_TYPE_SMALLEREQ    = XMMS_COLLECTION_TYPE_SMALLEREQ
COLLECTION_TYPE_GREATER      = XMMS_COLLECTION_TYPE_GREATER
COLLECTION_TYPE_GREATEREQ    = XMMS_COLLECTION_TYPE_GREATEREQ
COLLECTION_TYPE_ORDER        = XMMS_COLLECTION_TYPE_ORDER
COLLECTION_TYPE_LIMIT        = XMMS_COLLECTION_TYPE_LIMIT
COLLECTION_TYPE_MEDIASET     = XMMS_COLLECTION_TYPE_MEDIASET
COLLECTION_TYPE_IDLIST       = XMMS_COLLECTION_TYPE_IDLIST


from propdict import PropDict # xmmsclient.propdict


cdef xmmsv_t *create_native_value(value) except NULL:
	cdef xmmsv_t *ret = NULL
	cdef xmmsv_t *v
	cdef XmmsValue xv

	if value is None:
		ret = xmmsv_new_none()
	elif isinstance(value, XmmsValue):
		xv = value
		ret = xmmsv_copy(xv.val) # Prevent side effects.
	elif isinstance(value, int):
		ret = xmmsv_new_int(value)
	elif isinstance(value, (unicode, str)):
		s = from_unicode(value)
		ret = xmmsv_new_string(s)
	elif hasattr(value, "keys"):
		ret = xmmsv_new_dict()
		for key in value.keys():
			k = from_unicode(key)
			v = create_native_value(value[key])
			xmmsv_dict_set(ret, k, v)
			xmmsv_unref(v)
	else:
		try:
			it = iter(value)
		except TypeError:
			raise TypeError("Type '%s' has no corresponding native value type"
					% value.__class__.__name__)
		else:
			ret = xmmsv_new_list()
			for item in value:
				v = create_native_value(item)
				xmmsv_list_append(ret, v)
				xmmsv_unref(v) # the list took its own reference
	return ret

class XmmsError(Exception): pass

cdef class XmmsValue:
	#cdef object sourcepref
	#cdef xmmsv_t *val
	#cdef int ispropdict

	def __cinit__(self):
		# Prevent stupid segfault from user missuse.
		self.val = xmmsv_new_none()
		self.ispropdict = 0

	def __init__(self, sourcepref = None, pyval = None):
		cdef xmmsv_t *v
		self.sourcepref = sourcepref

		if pyval is not None: # None value already created.
			XmmsValue.set_pyval(self, pyval) # Use cdef version of the function

	def __dealloc__(self):
		if self.val != NULL:
			xmmsv_unref(self.val)
			self.val = NULL

	cdef set_value(self, xmmsv_t *value, int ispropdict = -1):
		if self.val != NULL:
			xmmsv_unref(self.val)
		xmmsv_ref(value)
		self.val = value
		if ispropdict >= 0:
			self.ispropdict = ispropdict

	cpdef set_pyval(self, pyval):
		cdef xmmsv_t *v
		v = create_native_value(pyval)
		self.set_value(v)
		xmmsv_unref(v)

	cpdef get_type(self):
		"""
		Return the type of the value.
		The return value is one of the VALUE_TYPE_* constants.
		"""
		return xmmsv_get_type(self.val)

	cpdef iserror(self):
		"""
		@deprecated
		@return: Whether the value represents an error or not.
		@rtype: Boolean
		"""
		return self.is_error()

	cpdef is_error(self):
		"""
		@return: Whether the value represents an error or not.
		@rtype: Boolean
		"""
		return xmmsv_is_type(self.val, XMMSV_TYPE_ERROR)

	cpdef get_error(self):
		"""
		@return: Error string from the result.
		@rtype: String
		"""
		cdef char *ret = NULL
		if not xmmsv_get_error(self.val, <const_char **>&ret):
			raise ValueError("Failed to retrieve value")
		return XmmsError(to_unicode(ret))

	cpdef get_int(self):
		"""
		Get data from the result structure as an int.
		@rtype: int
		"""
		cdef int64_t ret = 0
		if not xmmsv_get_int64(self.val, &ret):
			raise ValueError("Failed to retrieve value")
		return ret

	cpdef get_float(self):
		"""
		Get data from the result structure as an float.
		@rtype: float
		"""
		cdef float ret = 0
		if not xmmsv_get_float(self.val, &ret):
			raise ValueError("Failed to retrieve value")
		return ret

	cpdef get_string(self):
		"""
		Get data from the result structure as a string.
		@rtype: string
		"""
		cdef char *ret = NULL
		if not xmmsv_get_string(self.val, <const_char **>&ret):
			raise ValueError("Failed to retrieve value")
		return to_unicode(ret)

	cpdef get_bin(self):
		"""
		Get data from the result structure as binary data.
		@rtype: string
		"""
		cdef char *ret = NULL
		cdef unsigned int rlen = 0
		if not xmmsv_get_bin(self.val, <const_uchar **>&ret, &rlen):
			raise ValueError("Failed to retrieve value")
		return PyBytes_FromStringAndSize(<char *>ret, rlen)

	cpdef get_coll(self):
		"""
		Get data from the result structure as a Collection.
		@rtype: Collection
		"""
		if not xmmsv_is_type(self.val, XMMSV_TYPE_COLL):
			raise ValueError("The value is not a collection")
		return create_coll(self.val)

	cpdef get_dict(self):
		"""
		@return: A dictionary containing media info.
		"""
		return dict([(k, v.value()) for k,v in self.get_dict_iter()])

	cpdef get_dict_iter(self):
		"""
		@return: An iterator on dict items ((key, value) pairs).
		"""
		return XmmsDictIter(self)

	cpdef get_propdict(self):
		"""
		@return: A source dict.
		"""
		ret = PropDict(self.sourcepref)
		for key, values in self.get_dict_iter():
			for source, value in values.get_dict_iter():
				ret[(source, key)] = value.value()
		return ret

	cpdef get_list(self):
		"""
		@return: A list of dicts from the result structure.
		"""
		return [v.value() for v in self.get_list_iter()]

	cpdef get_list_iter(self):
		"""
		@return: An iterator on a list.
		"""
		return XmmsListIter(self)

	cpdef value(self):
		"""
		Return value of appropriate data type contained in this result.
		This can be used instead of most get_* functions in this class.
		"""
		cdef xmmsv_type_t vtype
		vtype = self.get_type()

		if vtype == XMMSV_TYPE_NONE:
			return None
		elif vtype == XMMSV_TYPE_ERROR:
			return self.get_error()
		elif vtype == XMMSV_TYPE_INT64:
			return self.get_int()
		elif vtype == XMMSV_TYPE_FLOAT:
			return self.get_float()
		elif vtype == XMMSV_TYPE_STRING:
			return self.get_string()
		elif vtype == XMMSV_TYPE_COLL:
			return self.get_coll()
		elif vtype == XMMSV_TYPE_BIN:
			return self.get_bin()
		elif vtype == XMMSV_TYPE_LIST:
			return self.get_list()
		elif vtype == XMMSV_TYPE_DICT:
			if self.ispropdict:
				return self.get_propdict()
			return self.get_dict()
		else:
			raise TypeError("Unknown value type from the server: %d" % vtype)

	cpdef copy(self, cls=None):
		if cls is None:
			cls = XmmsValue
		elif not issubclass(cls, XmmsValue):
			raise TypeError("Custom class must be a subclass of XmmsValue")
		return cls(sourcepref = self.sourcepref, pyval = self)

	def __iter__(self):
		vtype = self.get_type()

		if vtype == XMMSV_TYPE_LIST:
			return self.get_list_iter()
		elif vtype == XMMSV_TYPE_DICT:
			return self.get_dict_iter()
		elif vtype == XMMSV_TYPE_STRING:
			return iter(self.get_string())
		elif vtype == XMMSV_TYPE_BIN:
			return iter(self.get_bin())
		raise TypeError("Value type not iterable")



cdef class XmmsListIter:
	#cdef object sourcepref
	#cdef xmmsv_t *val
	#cdef xmmsv_list_iter_t *it

	def __cinit__(self):
		self.val = NULL
		self.it = NULL

	def __dealloc__(self):
		if self.it != NULL:
			xmmsv_list_iter_explicit_destroy(self.it)
		if self.val != NULL:
			xmmsv_unref(self.val)

	def __init__(self, XmmsValue value):
		if value.get_type() != XMMSV_TYPE_LIST:
			raise TypeError("The value is not a list.")
		self.val = xmmsv_ref(value.val)
		if not xmmsv_get_list_iter(self.val, &self.it):
			raise RuntimeError("Failed to initialize the iterator.")
		self.sourcepref = value.sourcepref

	def __iter__(self):
		return self

	def __next__(self):
		cdef xmmsv_t *val = NULL
		cdef XmmsValue v

		if not xmmsv_list_iter_valid(self.it):
			raise StopIteration()

		if not xmmsv_list_iter_entry(self.it, &val):
			raise RuntimeError("Failed to retrieve list entry.")
		v = XmmsValue(self.sourcepref)
		v.set_value(val)

		xmmsv_list_iter_next(self.it)
		return v


cdef class XmmsDictIter:
	#cdef object sourcepref
	#cdef xmmsv_t *val
	#cdef xmmsv_dict_iter_t *it

	def __cinit__(self):
		self.val = NULL
		self.it = NULL

	def __dealloc__(self):
		if self.it != NULL:
			xmmsv_dict_iter_explicit_destroy(self.it)
		if self.val != NULL:
			xmmsv_unref(self.val)

	def __init__(self, XmmsValue value):
		if value.get_type() != XMMSV_TYPE_DICT:
			raise TypeError("The value is not a dict.")
		self.val = xmmsv_ref(value.val)
		if not xmmsv_get_dict_iter(self.val, &self.it):
			raise RuntimeError("Failed to initialize the iterator.")
		self.sourcepref = value.sourcepref

	def __iter__(self):
		return self

	def __next__(self):
		cdef char *key = NULL
		cdef xmmsv_t *val = NULL
		cdef XmmsValue v

		if not xmmsv_dict_iter_valid(self.it):
			raise StopIteration()

		if not xmmsv_dict_iter_pair(self.it, <const_char **>&key, &val):
			raise RuntimeError("Failed to retrieve dict item")
		v = XmmsValue(self.sourcepref)
		v.set_value(val)

		xmmsv_dict_iter_next(self.it)
		return (to_unicode(key), v)


cdef class CollectionRef:
	def __cinit__(self):
		self.coll = NULL

	def __dealloc__(self):
		if self.coll != NULL:
			xmmsv_unref(<xmmsv_t *>self.coll)
		self.coll = NULL

	cdef set_collection(self, xmmsv_t *coll):
		if self.coll != NULL:
			xmmsv_unref(<xmmsv_t *>self.coll)
		xmmsv_ref(<xmmsv_t *>coll)
		self.coll = coll


cdef _idlist_types = (
		XMMS_COLLECTION_TYPE_IDLIST,
		)
cdef class Collection(CollectionRef):
	#cdef object _attributes
	#cdef object _operands
	#cdef object _idlist

	cdef init_idlist(self):
		if self._idlist is None and self.coll != NULL and xmmsv_coll_get_type(self.coll) in _idlist_types:
			self._idlist = CollectionIDList(self)

	cdef init_attributes(self):
		if self._attributes is None and self.coll != NULL:
			self._attributes = CollectionAttributes(self)

	cdef init_operands(self):
		if self._operands is None and self.coll != NULL:
			self._operands = CollectionOperands(self)

	property attributes:
		def __get__(self):
			self.init_attributes()
			return self._attributes
		def __set__(self, attrs):
			self.init_attributes()
			if self._attributes is attrs:
				pass
			else:
				self._attributes.clear()
				self._attributes.update(attrs)
	property operands:
		def __get__(self):
			self.init_operands()
			return self._operands
		def __set__(self, ops):
			self.init_operands()
			if self._operands is ops:
				pass
			else:
				self._operands.clear()
				self._operands.extend(ops)
	property ids:
		def __get__(self):
			self.init_idlist()
			return self._idlist
		def __set__(self, ids):
			self.init_idlist()
			if self._idlist is None:
				raise TypeError("Can't set idlist for this type of collection")
			elif self._idlist is ids:
				pass
			else:
				self._idlist.clear()
				self._idlist.extend(ids)

	cpdef copy(self):
		cdef xmmsv_t *newcoll = xmmsv_copy(<xmmsv_t *>self.coll)
		coll = create_coll(newcoll)
		xmmsv_unref(newcoll)
		return coll

	def __repr__(self):
		atr = []
		operands = []
		if self.ids is not None:
			atr.append("ids=%r" % self.ids)
		for o in self.operands:
			operands.append(repr(o))
		if operands:
			atr.append("operands=[%s]" % ", ".join(operands))
		for k,v in self.attributes.iteritems():
			atr.append("%s=%s" % (k, repr(v)))
		return "%s(%s)" % (self.__class__.__name__, ", ".join(atr))

	def __or__(self, other): # |
		if not (isinstance(self, Collection) and isinstance(other, Collection)):
			raise TypeError("Unsupported operand type(s) for |: %r and %r"
				% (self.__class__.__name__, other.__class__.__name__))
		u = Union()
		for op in (self, other):
			if isinstance(op, Union):
				u.operands.extend(op.operands)
			else:
				u.operands.append(op)
		return u

	def __and__(self, other): # &
		if not (isinstance(self, Collection) and isinstance(other, Collection)):
			raise TypeError("Unsupported operand type(s) for &: %r and %r"
				% (self.__class__.__name__, other.__class__.__name__))
		i = Intersection()
		for op in (self, other):
			if isinstance(op, Intersection):
				i.operands.extend(op.operands)
			else:
				i.operands.append(op)
		return i

	def __invert__(self): #~
		if isinstance(self, Complement):
			return self.operands[0]
		return Complement(self)


cdef class AttributesIterator:
	#cdef xmmsv_dict_iter_t *diter
	#cdef int itertype
	#cdef CollectionAttributes attr

	def __cinit__(self):
		self.diter = NULL
		self.itertype = 0

	def __dealloc__(self):
		if self.diter != NULL:
			xmmsv_dict_iter_explicit_destroy(self.diter)

	def __init__(self, CollectionAttributes attributes, AttributesIterType itertype):
		cdef xmmsv_dict_iter_t *it = NULL
		if attributes is None:
			raise TypeError("__init__() argument must not be None")
		if not xmmsv_get_dict_iter(xmmsv_coll_attributes_get(attributes.coll), &it):
			raise RuntimeError("Failed to get fict iterator")
		xmmsv_dict_iter_first(it)
		self.diter = it
		self.itertype = itertype

	def __iter__(self):
		return self

	def __next__(self):
		cdef char *key = NULL
		cdef xmmsv_t *value = NULL
		if self.diter != NULL and xmmsv_dict_iter_valid(self.diter):
			xmmsv_dict_iter_pair(self.diter, <const_char **>&key, &value)
			k = to_unicode(key)
			if self.itertype == ITER_KEYS:
				ret = k
			else:
				v = XmmsValue()
				v.set_value(value)
				if self.itertype == ITER_VALUES:
					ret = v.values
				elif self.itertype == ITER_ITEMS:
					ret = (k, v.value())
				elif self.itertype == ITER_XVALUES:
					ret = v
				elif self.itertype == ITER_XITEMS:
					ret = (k, v)
				else:
					# Should never be reached
					raise RuntimeError("Unknown iter type")
			xmmsv_dict_iter_next(self.diter)
			return ret
		raise StopIteration()

	cpdef reset(self):
		xmmsv_dict_iter_first(self.diter)


cdef class CollectionAttributes(CollectionRef):
	def __init__(self, Collection c):
		if c.coll == NULL:
			raise RuntimeError("Uninitialized collection")
		self.set_collection(c.coll)

	cpdef get_dict(self):
		"""Get a copy of attributes dict"""
		return dict(self.iteritems())

	cpdef iterkeys(self):
		return AttributesIterator(self, ITER_KEYS)

	cpdef keys(self):
		return list(self.iterkeys())

	cpdef itervalues(self):
		return AttributesIterator(self, ITER_VALUES)

	cpdef iterxvalues(self):
		return AttributesIterator(self, ITER_XVALUES)

	cpdef values(self):
		return list(self.itervalues())

	cpdef xvalues(self):
		return list(self.iterxvalues())

	cpdef iteritems(self):
		return AttributesIterator(self, ITER_ITEMS)

	cpdef iterxitems(self):
		return AttributesIterator(self, ITER_XITEMS)

	cpdef items(self):
		return list(self.iteritems())

	cpdef xitems(self):
		return list(self.iterxitems())

	def __contains__(self, name):
		cdef xmmsv_t *value = NULL
		return xmmsv_coll_attribute_get_value(self.coll, name, &value)

	def __iter__(self):
		return self.iterkeys()

	def __repr__(self):
		return repr(self.get_dict())

	def __str__(self):
		return str(self.get_dict())

	def __getitem__(self, name):
		return self.xget(name).value()

	def __setitem__(self, name, value):
		cdef xmmsv_t *v
		n = from_unicode(name)
		v = create_native_value(value)
		xmmsv_coll_attribute_set_value(self.coll, <char *>n, v)
		xmmsv_unref(v)

	def __delitem__(self, name):
		n = from_unicode(name)
		if not xmmsv_coll_attribute_remove(self.coll, <char *>n):
			raise KeyError("The attribute '%s' doesn't exist" % name)

	def get(self, name, default = None):
		try:
			return self.xget(name).value()
		except KeyError:
			return default

	def xget(self, name):
		cdef xmmsv_t *value = NULL
		cdef XmmsValue xv
		nam = from_unicode(name)
		if not xmmsv_coll_attribute_get_value(self.coll, <char *>nam, &value):
			raise KeyError("The attribute '%s' doesn't exist" % name)
		xv = XmmsValue()
		xv.set_value(value)
		return xv

	cpdef clear(self):
		for k in self.keys():
			xmmsv_coll_attribute_remove(self.coll, k)

	def update(self, *a, **kargs):
		for d in a:
			if hasattr(d, 'keys') and hasattr(d.keys, '__call__'):
				for k in d.keys():
					self[k] = d[k]
			else:
				for k, v in d:
					self[k] = v
		for k in kargs:
			self[k] = kargs[k]

	def copy(self):
		return self.get_dict()


cdef class CollectionOperands(CollectionRef):
	#cdef object pylist

	def __init__(self, Collection c):
		if c.coll == NULL:
			raise RuntimeError("Uninitialized collection")
		self.set_collection(c.coll)
		self.init_pylist()

	cdef init_pylist(self):
		cdef XmmsValue v
		if self.pylist is None:
			v = XmmsValue()
			v.set_value(xmmsv_coll_operands_get(self.coll))
			self.pylist = v.value()

	def __repr__(self):
		return repr(self.pylist)
	def __str__(self):
		return str(self.pylist)
	def __len__(self):
		return len(self.pylist)
	def __getitem__(self, i):
		return self.pylist[i]
	def __delitem__(self, i):
		cdef Collection op
		try:
			op = self.pylist.pop(i)
			xmmsv_coll_remove_operand(self.coll, op.coll)
		except IndexError:
			raise IndexError("Index out of range")
	def __iter__(self):
		return iter(self.pylist)

	cpdef append(self, Collection op):
		"""Append an operand"""
		with cython.nonecheck(True):
			xmmsv_coll_add_operand(self.coll, op.coll)
			self.pylist.append(op)

	cpdef extend(self, ops):
		for o in ops:
			self.append(o)

	cpdef remove(self, Collection op):
		"""Remove an operand"""
		with cython.nonecheck(True):
			xmmsv_coll_remove_operand(self.coll, op.coll)
			self.pylist.remove(op)

	cpdef clear(self):
		cdef Collection op
		for op in self.pylist:
			xmmsv_coll_remove_operand(self.coll, op.coll)
		self.pylist = []

	cpdef list(self):
		return self.pylist[:]



cdef class CollectionIDList(CollectionRef):
	def __init__(self, Collection c):
		if c.coll == NULL:
			raise RuntimeError("Uninitialized collection")
		self.set_collection(c.coll)

	def __len__(self):
		return xmmsv_coll_idlist_get_size(self.coll)

	def __iter__(self):
		"""Iterate over ids"""
		cdef int x = -1
		cdef int l
		cdef int i = 0
		l = xmmsv_coll_idlist_get_size(self.coll)
		while i < l:
			if not xmmsv_coll_idlist_get_index(self.coll, i, &x):
				raise RuntimeError("Failed to retrieve id at index %d" % i)
			yield x
			i += 1

	def list(self):
		"""Returns a _COPY_ of the idlist as an ordinary list"""
		return list(self)

	def __repr__(self):
		return repr(self.list())

	cpdef append(self, int v):
		"""Appends an id to the idlist"""
		if not xmmsv_coll_idlist_append(self.coll, v):
			raise RuntimeError("Failed to append an id")

	cpdef extend(self, v):
		cdef int a
		for a in v:
			self.append(a)

	def __iadd__(self, v):
		self.extend(v)
		return self

	cpdef insert(self, int v, int i):
		"""Inserts an id at specified position"""
		if i < 0:
			i = len(self) + i
		if not xmmsv_coll_idlist_insert(self.coll, v, i):
			raise IndexError("Index out of range")

	cpdef remove(self, int i):
		"""Removes entry at specified position"""
		if i < 0:
			i = len(self) + i
		if not xmmsv_coll_idlist_remove(self.coll, i):
			raise IndexError("Index out of range")

	cpdef pop(self, int i = -1):
		"""Remove an id at specified position and return it"""
		x = self[i]
		self.remove(i)
		return x

	def __delitem__(self, int i):
		if i < 0:
			i = len(self) + i
		self.remove(i)

	def __getitem__(self, int i):
		cdef int x = 0
		if i < 0:
			i = len(self) + i
		if not xmmsv_coll_idlist_get_index(self.coll, i, &x):
			raise IndexError("Index out of range")
		return x

	def __setitem__(self, int i, int v):
		if i < 0:
			i = len(self) + i
		if not xmmsv_coll_idlist_set_index(self.coll, i, v):
			raise IndexError("Index out of range")

	cpdef clear(self):
		if not xmmsv_coll_idlist_clear(self.coll):
			raise RuntimeError("Failed to clear ids")


# XXX Might not be needed.
class CollectionWrapper(Collection):
	pass

class BaseCollection(CollectionWrapper):
	def __init__(Collection self, int _colltype, **kargs):
		self.coll = xmmsv_new_coll(<xmmsv_coll_type_t> _colltype)
		if self.coll == NULL:
			raise RuntimeError("Bad collection")

		operands = kargs.pop("operands", None)
		if operands:
			self.operands = operands

		ids = kargs.pop("ids", None)
		if ids:
			try:
				self.ids = ids
			except TypeError: # Just ignore idlist when illegal
				pass

		self.attributes.update(kargs)

class Reference(BaseCollection):
	def __init__(Collection self, ref = None, ns = "Collections", **kargs):
		kargs.update(
				namespace = kargs.get('namespace', ns),
				reference = kargs.get('reference', ref)
				)
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_REFERENCE, **kargs)

class Universe(Reference):
	def __init__(self):
		Reference.__init__(self, reference = "All Media")

	def __repr__(self):
		return "%s()" % self.__class__.__name__

class FilterCollection(BaseCollection):
	def __init__(Collection self, _operation, parent = None, **kargs):
		if 'operands' in kargs:
			operands = kargs
		elif parent is None:
			operands = [Universe()]
		else:
			operands = [parent]

		BaseCollection.__init__(self, _operation,
				operands = operands,
				**kargs)

class Equals(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_EQUALS, parent, **kargs)

class NotEqual(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_NOTEQUAL, parent, **kargs)

class Smaller(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_SMALLER, parent, **kargs)

class SmallerEqual(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_SMALLEREQ, parent, **kargs)

class Greater(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_GREATER, parent, **kargs)

class GreaterEqual(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_GREATEREQ, parent, **kargs)

class Match(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_MATCH, parent, **kargs)

class Token(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_TOKEN, parent, **kargs)

class Has(FilterCollection):
	def __init__(Collection self, parent = None, **kargs):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_HAS, parent, **kargs)

class Order(BaseCollection):
	def __init__(Collection self, operand = None, field = None, direction = 0, **kargs):
		if isinstance(direction, int):
			kargs['direction'] = 'ASC' if direction >= 0 else 'DESC'
		else:
			direction = str(direction).upper()
			if direction not in ('ASC', 'DESC'):
				raise TypeError("'direction' must be an integer or one of 'ASC', 'DESC'")
			kargs['direction'] = direction

		kargs['operands'] = kargs.get('operands', [operand or Universe()])

		if field == 'id':
			kargs["type"] = "id"
		elif field:
			kargs["field"] = field
			kargs["type"] = "value"
		else:
			kargs["type"] = kargs.get('type', "random")

		if kargs["type"] == "random":
			del kargs['direction']

		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_ORDER, **kargs)

class Mediaset(BaseCollection):
	def __init__(Collection self, operand = None, **kargs):
		kargs['operands'] = kargs.get('operands', [operand or Universe()])
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_MEDIASET, **kargs)

class Limit(BaseCollection):
	def __init__(Collection self, operand = None, start = 0, length = 0, **kargs):
		kargs.update(
				operands = kargs.get('operands', [operand or Universe()]),
				start = int(start),
				length = int(length)
				)
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_LIMIT, **kargs)

class IDList(BaseCollection):
	def __init__(Collection self, ids = None, **kargs):
		kargs.update(
				type = kargs.get('type', 'list'),
				ids = ids
				)
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_IDLIST, **kargs)

class Queue(IDList):
	def __init__(Collection self, ids = None, **kargs):
		kargs.update(
				type = kargs.get('type', 'queue')
				)
		IDList.__init__(self, ids, **kargs)

class PShuffle(IDList):
	def __init__(Collection self, ids = None, parent = None, **kargs):
		kargs.update(
				type = kargs.get('type', 'pshuffle'),
				operands = kargs.get('operands', [parent or Universe()])
				)
		IDList.__init__(self, ids, **kargs)

class Union(BaseCollection):
	def __init__(Collection self, *a, **kargs):
		kargs['operands'] = kargs.get('operands', list(a))
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_UNION, **kargs)

class Intersection(BaseCollection):
	def __init__(Collection self, *a, **kargs):
		kargs['operands'] = kargs.get('operands', list(a))
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_INTERSECTION, **kargs)

class Complement(BaseCollection):
	def __init__(Collection self, operand = None, **kargs):
		kargs['operands'] = kargs.get('operands', [operand or Universe()])
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_COMPLEMENT, **kargs)

cdef _collclass = {
		XMMS_COLLECTION_TYPE_REFERENCE: Reference,
		XMMS_COLLECTION_TYPE_UNIVERSE: Universe,
		XMMS_COLLECTION_TYPE_UNION: Union,
		XMMS_COLLECTION_TYPE_INTERSECTION: Intersection,
		XMMS_COLLECTION_TYPE_COMPLEMENT: Complement,
		XMMS_COLLECTION_TYPE_HAS: Has,
		XMMS_COLLECTION_TYPE_EQUALS: Equals,
		XMMS_COLLECTION_TYPE_NOTEQUAL: NotEqual,
		XMMS_COLLECTION_TYPE_MATCH: Match,
		XMMS_COLLECTION_TYPE_TOKEN: Token,
		XMMS_COLLECTION_TYPE_SMALLER: Smaller,
		XMMS_COLLECTION_TYPE_SMALLEREQ: SmallerEqual,
		XMMS_COLLECTION_TYPE_GREATER: Greater,
		XMMS_COLLECTION_TYPE_GREATEREQ: GreaterEqual,
		XMMS_COLLECTION_TYPE_ORDER: Order,
		XMMS_COLLECTION_TYPE_MEDIASET: Mediaset,
		XMMS_COLLECTION_TYPE_LIMIT: Limit,
		XMMS_COLLECTION_TYPE_IDLIST: IDList,
		}
cdef create_coll(xmmsv_t *coll):
	cdef xmmsv_coll_type_t colltype
	cdef Collection c
	cdef const_char *idlist_type
	cdef object idtype_uni

	colltype = xmmsv_coll_get_type(coll)
	c = CollectionWrapper()
	if colltype in _collclass:
		c.__class__ = _collclass[colltype]
	else:
		raise RuntimeError("Unknown collection type")

	c.set_collection(coll)

	if colltype == XMMS_COLLECTION_TYPE_REFERENCE:
		if c.attributes.get("reference", "") == "All Media":
			c.__class__ = Universe
	elif colltype == XMMS_COLLECTION_TYPE_IDLIST:
		if xmmsv_coll_attribute_get_string(coll, "type", &idlist_type):
			idtype_uni = to_unicode(idlist_type)
			if idtype_uni == "queue":
				c.__class__ = Queue
			elif idtype_uni == "pshuffle":
				c.__class__ = PShuffle

	return c


def coll_parse(pattern):
	cdef xmmsv_t *coll = NULL

	_pattern = from_unicode(pattern)
	if not xmmsv_coll_parse(<char *>_pattern, &coll):
		raise ValueError("Unable to parse the pattern")
	return create_coll(coll)

cdef get_default_source_pref():
	cdef int i = 0
	sourcepref = []
	while xmmsv_default_source_pref[i] != NULL:
		sourcepref.append(to_unicode(xmmsv_default_source_pref[i]))
		i += 1
	return sourcepref
