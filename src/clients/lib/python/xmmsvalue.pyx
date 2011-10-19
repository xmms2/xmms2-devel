"""
Python bindings for xmmsv manipulations.
"""

from xmmsutils cimport *
from cpython.bytes cimport PyBytes_FromStringAndSize
cimport cython
from cxmmsvalue cimport *

VALUE_TYPE_NONE   = XMMSV_TYPE_NONE
VALUE_TYPE_ERROR  = XMMSV_TYPE_ERROR
VALUE_TYPE_INT32  = XMMSV_TYPE_INT32
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
	cdef xmmsv_t *ret=NULL
	cdef xmmsv_t *v

	if value is None:
		ret = xmmsv_new_none()
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


cdef class XmmsValue:
	#cdef object sourcepref
	#cdef xmmsv_t *val
	#cdef int ispropdict

	def __cinit__(self):
		# Prevent stupid segfault from user missuse.
		self.val = xmmsv_new_none()
		self.ispropdict = 0

	def __init__(self, sourcepref=None, pyval=None):
		cdef xmmsv_t *v
		self.sourcepref = sourcepref

		if pyval is not None: # None value already created.
			XmmsValue.set_pyval(self, pyval) # Use cdef version of the function

	def __dealloc__(self):
		if self.val != NULL:
			xmmsv_unref(self.val)
			self.val = NULL

	cdef set_value(self, xmmsv_t *value, int ispropdict=-1):
		if self.val != NULL:
			xmmsv_unref(self.val)
		xmmsv_ref(value)
		self.val = value
		if ispropdict >= 0:
			self.ispropdict = ispropdict

	cpdef set_pyval(self, pyval):
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
		return xmmsv_is_error(self.val)

	cpdef get_error(self):
		"""
		@return: Error string from the result.
		@rtype: String
		"""
		cdef char *ret = NULL
		if not xmmsv_get_error(self.val, <const_char **>&ret):
			raise ValueError("Failed to retrieve value")
		return to_unicode(ret)

	cpdef get_int(self):
		"""
		Get data from the result structure as an int.
		@rtype: int
		"""
		cdef int ret = 0
		if not xmmsv_get_int(self.val, &ret):
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
		cdef xmmsv_coll_t *coll = NULL
		if not xmmsv_get_coll(self.val, &coll):
			raise ValueError("Failed to retrieve value")
		return create_coll(coll)

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
		elif vtype == XMMSV_TYPE_INT32:
			return self.get_int()
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
			xmmsv_coll_unref(self.coll)
		self.coll = NULL

	cdef set_collection(self, xmmsv_coll_t *coll):
		if self.coll != NULL:
			xmmsv_coll_unref(self.coll)
		xmmsv_coll_ref(coll)
		self.coll = coll


cdef _idlist_types = [XMMS_COLLECTION_TYPE_IDLIST]
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

	# Define read-only properties so that thoses appear in dir().
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

	def __repr__(self):
		atr = []
		if self.ids is not None:
			atr.append(repr(self.ids))
		for k,v in self.attributes.iteritems():
			atr.append("%s=%s" % (k, repr(v)))
		return "%s(%s)" % (self.__class__.__name__, ", ".join(atr))

	def __or__(self, other): # |
		if not (isinstance(self, Collection) and isinstance(self, Collection)):
			raise NotImplemented()
		# TODO: merge union (e.g. a|b|c -> Union(a, b, c)
		return Union(self, other)

	def __and__(self, other): # &
		if not (isinstance(self, Collection) and isinstance(self, Collection)):
			raise NotImplemented()
		# TODO: merge intersection (e.g. a&b&c -> Intersection(a, b, c)
		return Intersection(self, other)

	def __invert__(self): #~
		if not isinstance(self, Collection):
			return NotImplemented()
		# TODO: Unpack redundant complement (e.g. ~~a is a)
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
		cdef char *value = NULL
		if self.diter != NULL and xmmsv_dict_iter_valid(self.diter):
			xmmsv_dict_iter_pair_string(self.diter, <const_char **>&key, <const_char **>&value)
			if self.itertype == ITER_VALUES:
				ret = to_unicode(value)
			elif self.itertype == ITER_ITEMS:
				ret = (key, to_unicode(value))
			else: # ITER_KEYS is the default
				ret = key
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
		return [k for k in self.iterkeys()]

	cpdef itervalues(self):
		return AttributesIterator(self, ITER_VALUES)

	cpdef values(self):
		return [v for v in self.itervalues()]

	cpdef iteritems(self):
		return AttributesIterator(self, ITER_ITEMS)

	cpdef items(self):
		return [i for i in self.iteritems()]

	def __iter__(self):
		return self.iterkeys()

	def __repr__(self):
		return repr(self.get_dict())

	def __str__(self):
		return str(self.get_dict())

	def __getitem__(self, name):
		cdef char *value = NULL
		nam = from_unicode(name)
		if not xmmsv_coll_attribute_get(self.coll, <char *>nam, &value):
			raise KeyError("The attribute '%s' doesn't exist" % name)
		return to_unicode(value)

	def __setitem__(self, name, value):
		n = from_unicode(name)
		v = from_unicode(value)
		xmmsv_coll_attribute_set(self.coll, <char *>n, <char *>v)

	def __delitem__(self, name):
		n = from_unicode(name)
		if not xmmsv_coll_attribute_remove(self.coll, <char *>n):
			raise KeyError("The attribute '%s' doesn't exist" % name)

	def get(self, name, default=None):
		try:
			return self[name]
		except KeyError:
			return default

	cpdef clear(self):
		for k in self.keys():
			xmmsv_coll_attribute_remove(self.coll, k)

	def update(self, d, **kargs):
		if hasattr(d, 'keys') and hasattr(d.keys, '__call__'):
			for k in d.keys():
				self[k] = d[k]
		else:
			for k, v in d:
				self[k] = v
		for k in kargs:
			self[k] = kargs[k]

	def __contains__(self, name):
		cdef char *value = NULL
		return xmmsv_coll_attribute_get(self.coll, name, &value)

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

	cpdef list(self):
		"""Returns a _COPY_ of the idlist as an ordinary list"""
		cdef int x
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

	cpdef append(self, int v):
		"""Appends an id to the idlist"""
		xmmsv_coll_idlist_append(self.coll, v)

	cpdef extend(self, v):
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
		xmmsv_coll_idlist_clear(self.coll)


# XXX Might not be needed.
class CollectionWrapper(Collection):
	pass

class BaseCollection(CollectionWrapper):
	def __init__(Collection self, int _colltype, **kargs):
		self.coll = xmmsv_coll_new(<xmmsv_coll_type_t> _colltype)
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
		for k in kargs:
			self.attributes[k] = kargs[k]

class Reference(BaseCollection):
	def __init__(Collection self, ref, ns="Collections"):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_REFERENCE,
				namespace=ns, reference=ref)

class Universe(Reference):
	def __init__(self):
		Reference.__init__(self, "All Media")

class FilterCollection(BaseCollection):
	def __init__(Collection self, operation, parent=None, **kv):
		if parent is None:
			parent = Universe()
		BaseCollection.__init__(self, operation, operands=[parent], **kv)

class Equals(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_EQUALS, parent, **kv)

class NotEqual(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_NOTEQUAL, parent, **kv)

class Smaller(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_SMALLER, parent, **kv)

class SmallerEqual(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_SMALLEREQ, parent, **kv)

class Greater(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_GREATER, parent, **kv)

class GreaterEqual(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_GREATEREQ, parent, **kv)

class Match(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_MATCH, parent, **kv)

class Token(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_TOKEN, parent, **kv)

class Has(FilterCollection):
	def __init__(Collection self, parent=None, **kv):
		FilterCollection.__init__(self, XMMS_COLLECTION_TYPE_HAS, parent, **kv)

class Order(BaseCollection):
	def __init__(Collection self, operand, field=None, ascending=True):
		kv = dict(operands=[operand], direction="ASC" if ascending else "DESC")
		if field:
			kv["field"] = field
			kv["type"] = "value"
		else:
			kv["type"] = "random"
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_ORDER, **kv)

class Mediaset(BaseCollection):
	def __init__(Collection self, operand):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_MEDIASET,
				operands=[operand])

class Limit(BaseCollection):
	def __init__(Collection self, operand, start=0, length=0):
		kv = dict(operands=[operand], start=str(start), length=str(length))
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_LIMIT, **kv)

class IDList(BaseCollection):
	def __init__(Collection self, ids = None):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_IDLIST, type="list", ids=ids)

class Queue(BaseCollection):
	def __init__(Collection self, ids = None):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_IDLIST, type="queue", ids=ids)

class PShuffle(BaseCollection):
	def __init__(Collection self, parent = None, ids = None):
		if parent is None:
			parent = Universe()
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_IDLIST, type="pshuffle", operands=[parent], ids=ids)

class Union(BaseCollection):
	def __init__(Collection self, *a):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_UNION, operands=a)

class Intersection(BaseCollection):
	def __init__(Collection self, *a):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_INTERSECTION, operands=a)

class Complement(BaseCollection):
	def __init__(Collection self, op):
		BaseCollection.__init__(self, XMMS_COLLECTION_TYPE_COMPLEMENT, operands=[op])


cdef create_coll(xmmsv_coll_t *coll):
	cdef xmmsv_coll_type_t colltype
	cdef Collection c
	cdef char *idlist_type
	cdef object idtype_uni

	colltype = xmmsv_coll_get_type(coll)
	c = CollectionWrapper()
	if colltype == XMMS_COLLECTION_TYPE_REFERENCE:
		c.__class__ = Reference
	elif colltype == XMMS_COLLECTION_TYPE_UNIVERSE: # coll_parse() needs this
		c.__class__ = Universe
	elif colltype == XMMS_COLLECTION_TYPE_UNION:
		c.__class__ = Union
	elif colltype == XMMS_COLLECTION_TYPE_INTERSECTION:
		c.__class__ = Intersection
	elif colltype == XMMS_COLLECTION_TYPE_COMPLEMENT:
		c.__class__ = Complement
	elif colltype == XMMS_COLLECTION_TYPE_HAS:
		c.__class__ = Has
	elif colltype == XMMS_COLLECTION_TYPE_EQUALS:
		c.__class__ = Equals
	elif colltype == XMMS_COLLECTION_TYPE_NOTEQUAL:
		c.__class__ = NotEqual
	elif colltype == XMMS_COLLECTION_TYPE_MATCH:
		c.__class__ = Match
	elif colltype == XMMS_COLLECTION_TYPE_TOKEN:
		c.__class__ = Token
	elif colltype == XMMS_COLLECTION_TYPE_SMALLER:
		c.__class__ = Smaller
	elif colltype == XMMS_COLLECTION_TYPE_SMALLEREQ:
		c.__class__ = SmallerEqual
	elif colltype == XMMS_COLLECTION_TYPE_GREATER:
		c.__class__ = Greater
	elif colltype == XMMS_COLLECTION_TYPE_GREATEREQ:
		c.__class__ = GreaterEqual
	elif colltype == XMMS_COLLECTION_TYPE_ORDER:
		c.__class__ = Order
	elif colltype == XMMS_COLLECTION_TYPE_MEDIASET:
		c.__class__ = Mediaset
	elif colltype == XMMS_COLLECTION_TYPE_LIMIT:
		c.__class__ = Limit
	elif colltype == XMMS_COLLECTION_TYPE_IDLIST:
		if xmmsv_coll_attribute_get(coll, "type", &idlist_type):
			idtype_uni = to_unicode(idlist_type)
		else:
			idtype_uni = "list"
		if idtype_uni == "queue":
			c.__class__ = Queue
		elif idtype_uni == "pshuffle":
			c.__class__ = PShuffle
		else:
			c.__class__ = IDList
	else:
		raise RuntimeError("Unkown collection type")

	c.set_collection(coll)

	if colltype == XMMS_COLLECTION_TYPE_REFERENCE:
		if c.attributes.get("reference", "") == "All Media":
			c.__class__ = Universe

	return c


def coll_parse(pattern):
	cdef xmmsv_coll_t *coll = NULL

	_pattern = from_unicode(pattern)
	if not xmmsv_coll_parse(<char *>_pattern, &coll):
		raise ValueError("Unable to parse the pattern")
	return create_coll(coll)

XMMSValue = XmmsValue
