#!/usr/bin/env python
import sys
import genipc
from indenter import Indenter

#dictionary mapping of types in the xml to C type aliases
c_type_map = {
	'string': 'const char *',
	'int': 'gint32',
	'collection':'xmmsv_t *',
	'binary':'GString *',
	'list':'xmmsv_t *',
	'dictionary':'xmmsv_t *',
	'unknown' : 'xmmsv_t *',
}

c_getter_map = {
	'int': 'xmmsv_get_int',
	'string': 'xmmsv_get_string',
	'list': None,
	'dictionary': None,
	'collection': None,
	'binary': 'xmms_bin_to_gstring',
	'unknown' : None,
}

c_creator_map = {
	'int': 'xmmsv_new_int',
	'string': 'xmms_convert_and_kill_string',
	'list': None,
	'dictionary': None,
	'collection': None,
	'binary': None,
	'unknown' : None,
}

c_nullable_type_map = {
	'int': False,
	'string': "gchar *",
	'list': "xmmsv_t *",
	'dictionary': "xmmsv_t *",
	'collection': "xmmsv_t *",
	'binary': "GString *",
	'unknown' : "xmmsv_t *",
}

def build(object_name, c_type):
	ipc = genipc.parse_xml('../src/ipc.xml')

	Indenter.printline('/* This code is automatically generated from foobar. Do not edit. */')
	Indenter.printline()
	Indenter.printline("#include <xmmsc/xmmsv.h>")

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

def method_name_to_cname(n):
	return "__int_xmms_cmd_%s" % n

def emit_method_define_code(object, method, c_type):
	full_method_name = 'xmms_%s_client_%s' % (object.name, method.name)

	# output a prototype
	#static __XMMS_CMD_DO_RETTYPE_##_rettype() realfunc (argtype0 __XMMS_CMD_DO_ARGTYPE_##argtype1 __XMMS_CMD_DO_ARGTYPE_##argtype2 __XMMS_CMD_DO_ARGTYPE_##argtype3 __XMMS_CMD_DO_ARGTYPE_##argtype4 __XMMS_CMD_DO_ARGTYPE_##argtype5 __XMMS_CMD_DO_ARGTYPE_##argtype6, xmms_error_t *); \


	Indenter.printline("static void")
	Indenter.printline("%s (xmms_object_t *object, xmms_object_cmd_arg_t *arg)" % method_name_to_cname (method.name))
	Indenter.enter("{")

	if method.arguments:
		Indenter.printline("xmmsv_t *t;")

	Indenter.enter("if (xmmsv_list_get_size (arg->args) != %d) {" % len(method.arguments))
	Indenter.printline('XMMS_DBG ("Wrong number of arguments to %s (%%d)", xmmsv_list_get_size (arg->args));' % method.name)
	Indenter.printline('xmms_error_set (&arg->error, XMMS_ERROR_INVAL, "Wrong number of arguments to %s");' % method.name)
	Indenter.printline('return;')
	Indenter.leave("}")

	for i, a in enumerate(method.arguments):
		Indenter.printline("%s argval%d;" % (c_type_map[a.type[0]], i))

	Indenter.printline()

	for i, a in enumerate(method.arguments):
		Indenter.enter("if (!xmmsv_list_get (arg->args, %d, &t)) {" % i)
		Indenter.printline('XMMS_DBG ("Missing arg %d in %s");' % (i, method.name))
		Indenter.printline('xmms_error_set (&arg->error, XMMS_ERROR_INVAL, "Missing arg %d in %s");' % (i, method.name))
		Indenter.printline('return;')
		Indenter.leave("}")

		if c_getter_map[a.type[0]] is None:
			Indenter.printline("argval%d = t;" % i)
		else:
			Indenter.enter("if (!%s (t, &argval%d)) {" % (c_getter_map[a.type[0]], i))
			Indenter.printline('XMMS_DBG ("Error parsing arg %d in %s");' % (i, method.name))
			Indenter.printline('xmms_error_set (&arg->error, XMMS_ERROR_INVAL, "Error parsing arg %d in %s");' % (i, method.name))
			Indenter.printline("return;")
			Indenter.leave("}")


	Indenter.printline()

	args = "".join([("argval%d, " % i) for i in  range(len(method.arguments))])
	funccall = "%s ((%s) object, %s&arg->error)" % (full_method_name, c_type, args)
	if method.return_value:
		if c_creator_map[method.return_value.type[0]] is None:
			Indenter.printline("arg->retval = %s;" % (funccall))
		else:
			if c_nullable_type_map[method.return_value.type[0]]:
				Indenter.printline("%s retval = %s;" % (c_nullable_type_map[method.return_value.type[0]], funccall))
				Indenter.enter("if (retval != NULL) {")
				Indenter.printline("arg->retval = %s (retval);" % c_creator_map[method.return_value.type[0]])
				Indenter.leave("}")
			else:
				Indenter.printline("arg->retval = %s (%s);" % (c_creator_map[method.return_value.type[0]], funccall))
	else:
		Indenter.printline("%s;" % funccall)
		Indenter.printline("arg->retval = xmmsv_new_none ();")


	Indenter.leave("}")

	Indenter.printline()
	Indenter.printline()


def emit_method_add_code(object, method):
	Indenter.printline('xmms_object_cmd_add (%s_object, %i, %s);' % (object.name, method.id, method_name_to_cname (method.name)))
