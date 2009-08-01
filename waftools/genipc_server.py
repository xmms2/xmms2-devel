#!/usr/bin/env python
import sys
import genipc
from indenter import Indenter

#dictionary mapping of types in the xml to C type aliases
c_map = {
	'int': 'INT32',
	'string': 'STRING',
	'list': 'LIST',
	'dictionary': 'DICT',
	'collection': 'COLL',
	'binary': 'BIN',
}

def build(object_name, c_type):
	ipc = genipc.parse_xml('../src/ipc.xml')

	Indenter.printline('/* This code is automatically generated from foobar. Do not edit. */')
	Indenter.printline()

	for object in ipc.objects:
		if object.name == object_name:
			for method in object.methods:
				emit_method_define_code(object, method, c_type)

			Indenter.printline()
			Indenter.printline('static void')
			Indenter.printline('xmms_%s_register_ipc_commands (xmms_object_t *%s_object)' % (object.name, object.name))
			Indenter.enter('{')

			Indenter.printline('xmms_ipc_object_register (%i, %s_object);' % (object.id, object.name))
			Indenter.printline()

			for method in object.methods:
				emit_method_add_code(object, method)

			Indenter.printline()

			for broadcast in object.broadcasts:
				Indenter.printline('xmms_ipc_broadcast_register (%s_object, %i);' % (object.name, broadcast.id))

			Indenter.printline()

			for signal in object.signals:
				Indenter.printline('xmms_ipc_signal_register (%s_object, %i);' % (object.name, signal.id))

			Indenter.leave('}')

			Indenter.printline()
			Indenter.printline('static void')
			Indenter.printline('xmms_%s_unregister_ipc_commands (void)' % object.name)
			Indenter.enter('{')

			for broadcast in object.broadcasts:
				Indenter.printline('xmms_ipc_broadcast_unregister (%i);' % broadcast.id)

			Indenter.printline()

			for signal in object.signals:
				Indenter.printline('xmms_ipc_signal_unregister (%i);' % signal.id)

			Indenter.printline()
			Indenter.printline('xmms_ipc_object_unregister (%i);' % object.id)
			Indenter.leave('}')

def emit_method_define_code(object, method, c_type):
	full_method_name = 'xmms_%s_client_%s' % (object.name, method.name)

	if method.return_value:
		return_value_type = c_map[method.return_value.type[0]]
	else:
		return_value_type = 'NONE'

	argument_types = [c_map[a.type[0]] for a in method.arguments]

	while len(argument_types) < 6:
		argument_types.append('NONE')

	argument_types_s = ', ' + ', '.join(x for x in argument_types)

	Indenter.printline('XMMS_CMD_DEFINE6 (%s, %s, %s, %s%s);' % (method.name, full_method_name, c_type, return_value_type, argument_types_s))

def emit_method_add_code(object, method):
	Indenter.printline('xmms_object_cmd_add (%s_object, %i, XMMS_CMD_FUNC (%s));' % (object.name, method.id, method.name))
