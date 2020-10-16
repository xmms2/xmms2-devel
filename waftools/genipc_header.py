# encoding: utf-8

import sys
import genipc
from indenter import Indenter

HEADER_PROLOG = """
/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

/* Header generated from src/ipc.xml - DO NOT EDIT ! */

#ifndef __IPC_SIGNAL_XMMS_H__
#define __IPC_SIGNAL_XMMS_H__

#include <xmmsc/xmmsc_compiler.h>
""".strip()

HEADER_EPILOG = """
#include <xmmsc/xmmsc_idnumbers_extra.h>

#endif /* __IPC_SIGNAL_XMMS_H__ */
""".strip()

def _constant_name(name):
	return 'XMMS_%s' % name.upper()

def _c_type(name):
	return 'xmms_%s_t' % name

_escape = '\a\b\t\n\v\f\r"\\'
_escape_c = 'abtnvfr"\\'
def _c_string(s):
	ret = []
	for c in s:
		oc = ord(c)
		n = _escape.find(c)
		if n > -1:
			c = '\\%c' % _escape_c[n]
		elif oc < 32 or 0x7f <= oc <= 0xff:
			c = '\\x%02x' % oc
		elif oc > 0xff:
			raise ValueError("Unicode characters not supported.")
		ret.append(c)
	return '"%s"' % "".join(ret)

def _c_literal(val):
	""" Encode simple values in their C literal representation.
	Support int, float and str only.

	warning: Strings should contain only characters with ordinals <= 255.
	"""
	if isinstance(val, int):
		return "%d%s" % (val, (val >= 2**32 or val < -2**31) and "L" or "")
	elif isinstance(val, float):
		return ("%0.12f%s" % val).rstrip('0')
	elif isinstance(val, str):
		return _c_string(val)
	else:
		raise ValueError("Can't convert %r to C literal." % type(val).__name__)

def build_enum(enum, enums):
	if not enum:
		return
	Indenter.enter('typedef enum {')

	first = True
	for name, member in enum.items():
		if not first:
			print(',')
		first = False
		Indenter.printx(_constant_name(member.fullname()))
		value = getattr(member, 'value', None)
		alias = getattr(member, 'alias', None)
		if value is not None:
			val = " = %d" % value
		elif alias is not None:
			val, src, typ = alias
			if src:
				val = enums[src][val].fullname()
			elif typ == 'enum':
				val = enum[val].fullname()
			val = " = %s" % _constant_name(val)
		else:
			val = None
		if val is not None:
			sys.stdout.write(val)
	print('')

	Indenter.leave('} %s;' % _c_type(enum.name))
	print('')

def define_constant(c):
	Indenter.printline('#define %s %s' % (_constant_name(c.name), _c_literal(c.value)))
	print('')

def build():
	ipc = genipc.parse_xml('../src/ipc.xml')

	print(HEADER_PROLOG)
	print('')

	for n, c in ipc.constants.items():
		define_constant(c)
	print('')

	for n, e in ipc.enums.items():
		build_enum(e, ipc.enums)

	print('')
	print(HEADER_EPILOG)

if __name__ == '__main__':
	build()
