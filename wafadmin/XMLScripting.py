#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Use xml files as waf input, the files are transformed into wscript modules"

import imp
import Utils
from Params import fatal

try:
	from xml.sax import make_parser
	from xml.sax.handler import ContentHandler
except:
	fatal('wscript_xml requires the Python xml modules (sax)!')

def compile(file_path):
	parser = make_parser()
	curHandler = XMLHandler()
	parser.setContentHandler(curHandler)
	fi = open(file_path)
	parser.parse(fi)
	fi.close()

	res = "".join(curHandler.doc)
	module = imp.new_module('wscript')

	#print res
	#fatal('stop now')

	exec res in module.__dict__

	Utils.g_loaded_modules[file_path[:-4]] = module
	Utils.g_module = module

class XMLHandler(ContentHandler):
	def __init__(self):
		self.doc = []
		self.buf = []
		self.obj = 0
		self.tmp_lst = []
	def startElement(self, name, attrs):
		if self.obj and name != 'obj':
			if name == 'item':
				self.tmp_lst.append(attrs.get('value'))
				return

			if 'type' in attrs.keys() and attrs.get('type') == 'list':
				self.tmp_lst = []
				return

			self.doc += '\tobj.%s="%s"\n' % (name, attrs.get('value'))
			return

		if name == 'obj':
			self.doc += '\tobj = bld.create_obj("%s")\n' % attrs.get('class')
			self.doc += '\tobj.m_type = "%s"\n' % attrs.get('type')
			self.obj += 1
			return

		if name == 'dir':
			self.doc += '\tbld.pushdir("%s")\n' % attrs.get('name')
			return

		if name == 'group':
			self.doc += '\tbld.add_group("%s")\n' % attrs.get('name')
			return

		if name == 'document':
			self.buf = []
			return
		if name == 'options':
			self.doc += 'def set_options(opt):\n'
			return
		if name == 'config':
			self.doc += 'def configure(conf):\n'
			return
		if name == 'shutdown':
			self.doc += 'def shutdown():\n'
			return
		if name == 'build':
			self.doc += 'def build(bld):\n'
			return
		if name == 'tool_option':
			self.doc += '\topt.tool_options("%s")\n' % attrs.get('value')
			return

		if name == 'option':
			self.doc += '\topt.add_option('
			return
		if name == 'optparam':
			if 'name' in attrs.keys():
				self.doc += "%s='%s', " % (attrs.get('name'), attrs.get('value'))
			else:
				self.doc += "'%s', " % attrs.get('value')

		self.buf = []

	def endElement(self, name):
		buf = "".join(self.buf)

		if self.obj and name != 'obj':
			if self.tmp_lst and name != 'item':
				self.doc += "\tobj.%s = '" % name
				self.doc += " ".join(self.tmp_lst) + "'\n"
				self.tmp_lst = []
			return
		if name == 'obj':
			self.obj -= 1
			return

		if name == 'dir':
			self.doc += '\tbld.popdir()\n'
			return

		if name == 'version':
			self.doc += 'VERSION = "%s"\n' % buf
			return
		if name == 'appname':
			self.doc += 'APPNAME = "%s"\n' % buf
			return
		if name == 'srcdir':
			self.doc += 'srcdir  = "%s"\n' % buf
			return
		if name == 'blddir':
			self.doc += 'blddir  = "%s"\n' % buf
			return
		if name == 'build':
			self.doc += '\treturn\n'
			return
		if name == 'config':
			self.doc += '\treturn\n'
			return
		if name == 'shutdown':
			self.doc += '\treturn\n'
			return
		if name == 'options':
			self.doc += '\treturn\n'
			return
		if name == 'option':
			self.doc += ')\n'
			return
		if name == 'config-code':
			for line in buf.split('\n'):
				self.doc += '\t%s\n' % line
			self.doc += '\n'
			return
		if name == 'shutdown-code':
			for line in buf.split('\n'):
				self.doc += '\t%s\n' % line
			self.doc += '\n'
			return
		return

	def characters(self, cars):
		self.buf.append(cars)

