from cpython.unicode cimport PyUnicode_DecodeUTF8, PyUnicode_AsUTF8String
from cpython.bytes cimport PyBytes_AsString

cdef inline to_unicode(char *s):
	try:
		ns = PyUnicode_DecodeUTF8(s, len(s), NULL)
	except:
		ns = s
	return ns

cdef inline from_unicode(object o):
	if isinstance(o, unicode):
		return PyUnicode_AsUTF8String(o)
	else:
		return o

cdef inline char *to_charp(object o) except NULL:
	return PyBytes_AsString(o)


