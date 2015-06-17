#!/usr/bin/env python
import sys
import string
import xml.dom.minidom
from collections import OrderedDict

def _attribute_value(xml_element, attribute, default=None):
	try:
		return xml_element.attributes[attribute].value
	except KeyError:
		return default

class Enum(OrderedDict):
	def __init__(self, name, namespace_hint=None):
		OrderedDict.__init__(self)

		self.name = name
		if namespace_hint:
			self.namespace_hint = namespace_hint
		else:
			self.namespace_hint = self.name.upper()

	def add_member(self, name, value=None, alias=None):
		name = name.upper()
		if name in self:
			raise KeyError("%s.%s already defined" % (self.name, name))
		m = EnumMember(name, self, value, alias)
		self[name] = m
		return m

	def __setitem__(self, k, member):
		if not isinstance(member, EnumMember):
			raise ValueError("Expected EnumMember, got %s" % type(member).__name__)
		return OrderedDict.__setitem__(self, k, member)

class EnumMember:
	def __init__(self, name, enum, value=None, alias=None):
		self.name = name
		self.enum = enum
		if alias is not None and value is not None:
			raise ValueError("value and alias are mutually exclusive")
		if value is not None:
			self.value = int(value)
		elif alias:
			self.alias = alias

	def fullname(self, sep='_', fmt=str):
		return '%s%s%s' % (self.enum.namespace_hint, sep, self.name)

	def dottedname(self):
		return '%s.%s' % (self.enum.name, self.name)

	def __repr__(self):
		value = getattr(self, 'value', None)
		rval = ''
		if value is None:
			alias = getattr(self, 'alias', None)
			if alias is not None:
				val, src, t = alias
				rval = '%s%s%s%s' % (src or '', src and '.' or '', val, t == 'constant' and ' (constant)')
		else:
			rval = '%d' % value

		return "<%s %s%s%s>" % (type(self).__name__, self.dottedname(), rval and '=', rval)

class Constant:
	def __init__(self, name, value):
		self.name = name
		self.value = value

class NamedElement:
	def __init__(self, xml_element):
		name_element = xml_element.getElementsByTagName('name')[0]
		self.name = name_element.firstChild.data.strip()

class DocumentedElement:
	def __init__(self, xml_element):
		doc_element = xml_element.getElementsByTagName('documentation')[0]
		self.documentation = doc_element.firstChild.data.strip()

class TypedElement:
	def __init__(self, xml_element):
		self.type = []

		type_element = xml_element.getElementsByTagName('type')[0]

		choices = [e for e in type_element.childNodes
		           if e.nodeType == e.ELEMENT_NODE]
		node = choices[0]

		while node:
			nodeinfo = node.nodeName
			if nodeinfo == 'enum-value':
				nodeinfo = (nodeinfo, node.attributes['name'].value)
			self.type.append(nodeinfo)

			choices = [e for e in node.childNodes
			           if e.nodeType == e.ELEMENT_NODE]
			node = (choices or [None])[0]

class IpcFoo:
	def __init__(self, xml_element):
		self.version = int(xml_element.attributes.get('version').value)
		self.objects = []

		object_elements = xml_element.getElementsByTagName('object')
		object_id = 1 # ID 0 is reserved for signal voodoo

		self.constants = OrderedDict()

		ipc_version = Constant('IPC_PROTOCOL_VERSION', self.version)
		self.constants[ipc_version.name] = ipc_version

		for c in xml_element.getElementsByTagName('constant'):
			if c.parentNode is not xml_element:
				continue
			const = IpcConstant(c)
			self.constants[const.name] = const

		self.enums = OrderedDict()

		# Generated enums
		obj_enum = self.objects_enum = Enum('ipc_object', 'IPC_OBJECT')
		sig_enum = self.signals_enum = Enum('ipc_signal', 'IPC_SIGNAL')
		self.enums[obj_enum.name] = obj_enum
		self.enums[sig_enum.name] = sig_enum

		obj_enum.add_member('SIGNAL') # special object for signals and broadcasts

		for e in xml_element.getElementsByTagName('enum'):
			if e.parentNode is not xml_element:
				continue
			enum = IpcEnum(e)
			self.enums[enum.name] = enum

		for object_element in object_elements:
			object = IpcObject(object_element)
			self.objects.append(object)

			m = obj_enum.add_member(object.name.upper())
			object.id = m

			methods_enum = Enum('ipc_command_%s' % object.name)
			self.enums[methods_enum.name] = methods_enum
			alias_first = ('IPC_COMMAND_FIRST', None, 'constant')
			for meth in object.methods:
				m = methods_enum.add_member(meth.name.upper(), alias=alias_first)
				meth.id = m
				alias_first = None

		obj_enum.add_member('END')

		for bcsig, obj, type in self.iter_broadcasts_and_signals():
			m = sig_enum.add_member('%s_%s' % (obj.name.upper(), bcsig.name.upper()))
			bcsig.id = m
		sig_enum.add_member('END')


	def iter_broadcasts_and_signals(self):
		for obj in self.objects:
			for bc in obj.broadcasts:
				yield (bc, obj, 'broadcast')
			for s in obj.signals:
				yield (s, obj, 'signal')

class IpcConstant(Constant):
	_value_parse = {
			'string': str,
			'integer': int,
			'float': float,
			}
	def __init__(self, xml_element):
		name = NamedElement(xml_element).name

		value_element = xml_element.getElementsByTagName('value')[0]
		value_type = _attribute_value(value_element, 'type', 'string')
		value = value_element.firstChild.data.strip()
		value = self._value_parse[value_type](value)

		Constant.__init__(self, name, value)

class IpcEnum(Enum):
	def __init__(self, xml_element):
		name = NamedElement(xml_element).name
		ns_hint = xml_element.getElementsByTagName('namespace-hint')
		if ns_hint:
			ns_hint = ns_hint[0].firstChild.data.strip().upper()
		else: 
			ns_hint = None
		Enum.__init__(self, name, ns_hint)

		for m in xml_element.getElementsByTagName('member'):
			member = IpcEnumMember(m, self)
			name = member.name
			if name in self:
				raise KeyError("Enum member %s.%s defined twice" % (self.name, name))
			self[name] = member

class IpcEnumMember(EnumMember):
	def __init__(self, xml_element, enum):
		name = xml_element.firstChild.data.strip().upper()
		value = _attribute_value(xml_element, 'value', None)
		ref_type = _attribute_value(xml_element, 'ref-type', 'enum')
		ref_source = _attribute_value(xml_element, 'ref-source', None)
		ref_value = _attribute_value(xml_element, 'ref-value', None)
		if ref_value:
			alias = (ref_value, ref_source or None, ref_type or 'enum')
		else:
			alias = None
		EnumMember.__init__(self, name, enum, value, alias)

class IpcObject(NamedElement):
	def __init__(self, xml_element):
		NamedElement.__init__(self, xml_element)

		self.id = 0
		self.methods = []
		self.broadcasts = []
		self.signals = []

		method_elements = xml_element.getElementsByTagName('method')
		#method_id = 32 # IDs 0..31 are reserved for voodoo use

		for method_element in method_elements:
			method = IpcMethod(method_element)

			#method.id = method_id
			#method_id += 1

			self.methods.append(method)

		signal_elements = xml_element.getElementsByTagName('signal')

		for signal_element in signal_elements:
			self.signals.append(IpcSignalOrBroadcast(signal_element))

		broadcast_elements = xml_element.getElementsByTagName('broadcast')

		for broadcast_element in broadcast_elements:
			self.broadcasts.append(IpcSignalOrBroadcast(broadcast_element))

class IpcMethod(NamedElement, DocumentedElement):
	def __init__(self, xml_element):
		NamedElement.__init__(self, xml_element)
		DocumentedElement.__init__(self, xml_element)

		self.id = 0
		self.arguments = []
		self.return_value = None

		# Store attributes for this method as boolean object attributes
		for attr in ('noreply', 'need_cookie', 'need_client'):
			val = xml_element.attributes.get(attr, False)
			if val:
				val = (val.value == "true")
			setattr(self, attr, val)

		argument_elements = xml_element.getElementsByTagName('argument')

		for argument_element in argument_elements:
			self.arguments.append(IpcMethodArgument(argument_element))

		return_value_elements = xml_element.getElementsByTagName('return_value')
		if return_value_elements:
			self.return_value = IpcReturnValue(return_value_elements[0])

class IpcMethodArgument(NamedElement, DocumentedElement, TypedElement):
	def __init__(self, xml_element):
		NamedElement.__init__(self, xml_element)
		DocumentedElement.__init__(self, xml_element)
		TypedElement.__init__(self, xml_element)
		default_elements = xml_element.getElementsByTagName('default-hint')
		if default_elements:
			self.default_hint = default_elements[0].firstChild.data.strip()

class IpcReturnValue(DocumentedElement, TypedElement):
	def __init__(self, xml_element):
		DocumentedElement.__init__(self, xml_element)
		TypedElement.__init__(self, xml_element)

class IpcSignalOrBroadcast(NamedElement, DocumentedElement):
	def __init__(self, xml_element):
		NamedElement.__init__(self, xml_element)
		DocumentedElement.__init__(self, xml_element)

		self.id = None
		self.return_value = None

		#id_element = xml_element.getElementsByTagName('id')[0]
		#self.id = int(id_element.firstChild.data.strip())

		return_value_elements = xml_element.getElementsByTagName('return_value')
		self.return_value = IpcReturnValue(return_value_elements[0])

def parse_xml(file):
    #load the xml file
    doc = xml.dom.minidom.parse(file)

    ipc_element = doc.getElementsByTagName('ipc')[0]

    return IpcFoo(ipc_element)
