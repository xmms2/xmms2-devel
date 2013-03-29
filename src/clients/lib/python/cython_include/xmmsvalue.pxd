#needs to reimport required symbols from .h files.

cdef extern from "xmmsc/xmmsv.h":
	ctypedef struct xmmsv_t
	ctypedef struct xmmsv_dict_iter_t
	ctypedef struct xmmsv_list_iter_t

cdef class XmmsValue:
	cdef object sourcepref
	cdef xmmsv_t *val
	cdef int ispropdict

	cdef  set_value(self, xmmsv_t *value, int ispropdict=*)
	cpdef set_pyval(self, pyval)
	cpdef get_type(self)
	cpdef iserror(self)
	cpdef is_error(self)
	cpdef get_error(self)
	cpdef get_int(self)
	cpdef get_float(self)
	cpdef get_string(self)
	cpdef get_bin(self)
	cpdef get_coll(self)
	cpdef get_dict(self)
	cpdef get_dict_iter(self)
	cpdef get_propdict(self)
	cpdef get_list(self)
	cpdef get_list_iter(self)
	cpdef value(self)

cdef class XmmsListIter:
	cdef object sourcepref
	cdef xmmsv_t *val
	cdef xmmsv_list_iter_t *it

cdef class XmmsDictIter:
	cdef object sourcepref
	cdef xmmsv_t *val
	cdef xmmsv_dict_iter_t *it

cdef class CollectionRef:
	cdef xmmsv_t *coll

	cdef set_collection(self, xmmsv_t *coll)

cdef class Collection(CollectionRef):
	cdef object _attributes
	cdef object _operands
	cdef object _idlist

	cdef init_idlist(self)
	cdef init_attributes(self)
	cdef init_operands(self)

cdef class CollectionAttributes(CollectionRef):
	cpdef get_dict(self)
	cpdef iterkeys(self)
	cpdef keys(self)
	cpdef itervalues(self)
	cpdef iterxvalues(self)
	cpdef values(self)
	cpdef xvalues(self)
	cpdef iteritems(self)
	cpdef iterxitems(self)
	cpdef items(self)
	cpdef xitems(self)
	cpdef clear(self)

cdef enum AttributesIterType:
	ITER_KEYS = 1
	ITER_VALUES = 2
	ITER_ITEMS = 3
	ITER_XVALUES = 4
	ITER_XITEMS = 5
cdef class AttributesIterator:
	cdef xmmsv_dict_iter_t *diter
	cdef int itertype
	cdef CollectionAttributes attr

	cpdef reset(self)


cdef class CollectionOperands(CollectionRef):
	cdef object pylist
	cdef init_pylist(self)

	cpdef append(self, Collection op)
	cpdef extend(self, ops)
	cpdef remove(self, Collection op)
	cpdef clear(self)
	cpdef list(self)


cdef class CollectionIDList(CollectionRef):
	cpdef list(self)
	cpdef append(self, int v)
	cpdef extend(self, v)
	cpdef insert(self, int v, int i)
	cpdef remove(self, int i)
	cpdef clear(self)

cdef create_coll(xmmsv_t *coll)
cdef xmmsv_t *create_native_value(value) except NULL
