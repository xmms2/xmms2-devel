#! /usr/bin/env python
# encoding: utf-8
# Peter Soetens, 2006

"docbook processing (may be broken)"

import os, string
import Action, Object, Params, Runner, Utils
from Params import debug, fatal

# first, we define an action to build something
fop_vardeps = ['FOP']
def fop_build(task):
	bdir = task.m_inputs[0].bld_dir()
	src = task.m_inputs[0].bldpath()
	tgt = src[:-3]+'.pdf'
	cmd = '%s %s %s' % (task.m_env['FOP'], src, tgt)
	return Runner.exec_command(cmd)

xslt_vardeps = ['XSLTPROC', 'XSLTPROC_ST']

# Create .fo or .html from xml file
def xslt_build(task):
	bdir = task.m_inputs[0].bld_dir()
	src = task.m_inputs[0].bldpath()
	srcdir = os.path.dirname(task.m_inputs[0].bldpath())
	tgt = task.m_outputs[0].m_name
	cmd = task.m_env['XSLTPROC_ST'] % (task.m_env['XSLTPROC'], os.path.join(srcdir,task.m_env['XSLT_SHEET']), src, os.path.join(bdir, tgt))
	return Runner.exec_command(cmd)

# Create various file formats from a docbook or sgml file.
db2_vardeps = ['DB2','DB2HTML', 'DB2PDF', 'DB2TXT', 'DB2PS']
def db2_build(task):
	bdir = task.m_inputs[0].bld_dir()
	src = task.m_inputs[0].bldpath()
	cmd = task.m_compiler % (bdir, src)
	return Runner.exec_command(cmd)

xslt_vardeps = ['XSLTPROC']

## Given a 'docbook' object and a node to build,
# create the tasks to build the node's target.
def docb_file(obj, node):
	debug("Seen docbook file type")

	# this function is used several times
	fi = obj.find

	base, ext = os.path.splitext(node.m_name)

	# Input format is XML
	if ext == '.xml':
		if not obj.env['XSLTPROC']:
			fatal("Can not process %s: no xml processor detected." % node.m_name)
	if ext == '.xml' and obj.get_type() == 'pdf':
		debug("building pdf")
		xslttask = obj.create_task('xslt', obj.env, 4)

		xslttask.m_inputs  = [fi(node.m_name)]
		xslttask.m_outputs = [fi(base+'.fo')]
		if not obj.stylesheet:
			fatal('No stylesheet specified for creating pdf.')

		xslttask.m_env['XSLT_SHEET'] = obj.stylesheet

		# now we also add the task that creates the pdf file
		foptask = obj.create_task('fop', obj.env)
		foptask.m_inputs  = xslttask.m_outputs
		foptask.m_outputs = [fi(base+'.pdf')]

	if ext == '.xml' and obj.get_type() == 'html':
		debug("building html")
		xslttask = obj.create_task('xslt', obj.env, 4)

		xslttask.m_inputs  = [fi(node.m_name)]
		xslttask.m_outputs = [fi(base+'.html')]
		if not obj.stylesheet:
			fatal('No stylesheet specified for creating html.')
		xslttask.m_env['XSLT_SHEET'] = obj.stylesheet

	# Input format is docbook.
	if ext == '.sgml' or ext == '.docbook':
		if not obj.env["DB2%s" % string.upper(obj.get_type()) ]:
			fatal("Can not process %s: no suitable docbook processor detected." %  node.m_name )
	if ext == '.sgml' or ext == '.docbook':
		debug("building %s" % obj.get_type())

		xslttask = obj.create_task('db2', obj.env)

		xslttask.m_inputs  = [fi(node.m_name)]
		xslttask.m_outputs = [fi(base+'.'+obj.get_type())]
		xslttask.m_compiler = obj.env[ "DB2%s" % string.upper(obj.get_type() ) ]

	if ext == '.xml':
		if obj.get_type() == 'txt' or obj.get_type() == 'ps':
			fatal("docbook: while processing '%s':\n"
			      "txt and ps output are currently not supported when input format is XML." % node.m_name )

# docbook objects
class docbookobj(Object.genobj):
	def __init__(self, type='html'):
		Object.genobj.__init__(self, type)
		self.stylesheet = None

	def get_valid_types(self):
		return ['html', 'pdf', 'txt', 'ps']

	def get_type(self):
		return self.m_type

	def apply(self):
		debug("apply called for docbookobj")

		if not self.m_type in self.get_valid_types():
			fatal('Trying to convert docbook file to unknown type')

		# for each source argument, create a task
		lst = self.source.split()
		for filename in lst:
			node = self.path.find_source(filename)
			if not node:
				fatal("source not found: "+filename+" in "+str(self.path))

			# create a task to process the source file.
			docb_file(self, node)

	def install(self):
		if not (Params.g_commands['install'] or Params.g_commands['uninstall']):
			return

		current = Params.g_build.m_curdirnode
		lst = []
		docpath = Utils.join_path('share',Utils.g_module.APPNAME, 'doc')

		# Install all generated docs
		for task in self.m_tasks:
			base, ext = os.path.splitext(task.m_outputs[0].m_name)
			if ext[1:] not in self.get_valid_types():
				continue
			self.install_results('PREFIX', docpath, task )

def setup(env):
	Action.Action('fop', vars=fop_vardeps, func=fop_build, color='BLUE')
	Action.Action('xslt', vars=xslt_vardeps, func=xslt_build, color='BLUE')
	Action.Action('db2', vars=db2_vardeps, func=db2_build, color='BLUE')
	Object.register('docbook', docbookobj)

## Detect the installed programs: fop, xsltproc, xalan, docbook2xyz
# Favour xsltproc over xalan.
def detect(conf):
	# Detect programs for converting xml -> html/pdf
	fop = conf.find_program('fop', var='FOP')
	if fop:
		conf.env['FOP'] = fop
	xsltproc = conf.find_program('xsltproc', var='XSLTPROC')
	if xsltproc:
		conf.env['XSLTPROC_ST'] = '%s --xinclude %s %s > %s'
		conf.env['XSLTPROC'] = xsltproc

	xalan = conf.find_program('xalan', var='XALAN')
	if not xsltproc and xalan:
		conf.env['XSLTPROC_ST'] = '%s -xsl %s -in %s -out %s'
		conf.env['XSLTPROC'] = xalan

	# OpenJade conversion tools for converting sgml -> xyz
	jw = conf.find_program('jw', var='JW')
	if jw:
		conf.env['DB2HTML'] = "jw -u -f docbook -b html -o %s %s"
		conf.env['DB2PDF']  = "jw -f docbook -b pdf -o %s %s"
		conf.env['DB2PS']   = "jw -f docbook -b ps -o %s %s"
		conf.env['DB2TXT']  = "jw -f docbook -b txt -o %s %s"

	return 1

