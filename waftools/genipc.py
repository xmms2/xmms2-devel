#!/usr/bin/env python
import sys
import string
import xml.dom.minidom

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
			self.type.append(node.nodeName)

			choices = [e for e in node.childNodes
			           if e.nodeType == e.ELEMENT_NODE]
			node = (choices or [None])[0]

class IpcFoo:
	def __init__(self, xml_element):
		self.version = 1 # FIXME
		self.objects = []

		object_elements = xml_element.getElementsByTagName('object')
		object_id = 1 # ID 0 is reserved for signal voodoo

		for object_element in object_elements:
			object = IpcObject(object_element)

			object.id = object_id
			object_id += 1

			self.objects.append(object)

class IpcObject(NamedElement):
	def __init__(self, xml_element):
		NamedElement.__init__(self, xml_element)

		self.id = 0
		self.methods = []
		self.broadcasts = []
		self.signals = []

		method_elements = xml_element.getElementsByTagName('method')
		method_id = 32 # IDs 0..31 are reserved for voodoo use

		for method_element in method_elements:
			method = IpcMethod(method_element)

			method.id = method_id
			method_id += 1

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

class IpcReturnValue(DocumentedElement, TypedElement):
	def __init__(self, xml_element):
		DocumentedElement.__init__(self, xml_element)
		TypedElement.__init__(self, xml_element)

class IpcSignalOrBroadcast(NamedElement, DocumentedElement):
	def __init__(self, xml_element):
		NamedElement.__init__(self, xml_element)
		DocumentedElement.__init__(self, xml_element)

		self.id = 0
		self.return_value = None

		id_element = xml_element.getElementsByTagName('id')[0]
		self.id = int(id_element.firstChild.data.strip())

		return_value_elements = xml_element.getElementsByTagName('return_value')
		self.return_value = IpcReturnValue(return_value_elements[0])

def parse_xml(file):
    #load the xml file
    doc = xml.dom.minidom.parse(file)

    ipc_element = doc.getElementsByTagName('ipc')[0]

    return IpcFoo(ipc_element)
