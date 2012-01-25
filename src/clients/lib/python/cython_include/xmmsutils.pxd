from cpython.unicode cimport PyUnicode_DecodeUTF8, PyUnicode_AsUTF8String
from libc.string cimport const_char, strlen

cdef inline to_unicode(const_char *s):
	return PyUnicode_DecodeUTF8(s, strlen(s), "replace")

cdef inline from_unicode(object o):
	if isinstance(o, unicode):
		return PyUnicode_AsUTF8String(o)
	else:
		return o
