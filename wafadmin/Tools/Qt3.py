#! /usr/bin/env python
# encoding: utf-8

"""
Qt3 support

If QTDIR is given (absolute path), the configuration will look in it first

This module also demonstrates how to add tasks dynamically (when the build has started)
"""

import os, sys
import ccroot, cpp
import Action, Params, Object, Task, Utils
from Params import error, fatal
from Params import set_globals, globals

set_globals('QT3_MOC_H', ['.hpp', '.hxx', '.hh', '.h'])
set_globals('QT3_UI_EXT', ['.ui'])

uic_vardeps = ['QT3_UIC', 'QT3_UIC_FLAGS', 'QT3_UIC_ST']

class MTask(Task.Task):
	"A cpp task that may create a moc task dynamically"
	def __init__(self, action_name, env, parent, priority=10):
		Task.Task.__init__(self, action_name, env, priority)
		self.moc_done = 0
		self.parent = parent

	def may_start(self):
		if self.moc_done: return Task.Task.may_start(self)

		tree = Params.g_build
		parn = self.parent
		node = self.m_inputs[0]

		# scan the .cpp files and find if there is a moc file to run
		if tree.needs_rescan(node, parn.env):
			ccroot.g_c_scanner.do_scan(node, parn.env, hashparams = self.m_scanner_params)

		moctasks=[]
		mocfiles=[]
		variant = node.variant(parn.env)
		try:
			tmp_lst = tree.m_raw_deps[variant][node]
		except:
			tmp_lst = []
		for d in tmp_lst:
			base2, ext2 = os.path.splitext(d)
			if not ext2 == '.moc': continue
			# paranoid check
			if d in mocfiles:
				error("paranoia owns")
				continue
			# process that base.moc only once
			mocfiles.append(d)

			# find the extension - this search is done only once
			if Params.g_options.qt3_header_ext:
				ext = Params.g_options.qt3_header_ext
			else:
				path = node.m_parent.srcpath(parn.env)
				ext = None
				for i in globals('QT3_MOC_H'):
					try:
						os.stat(Utils.join_path(path,base2+i))
						ext = i
						break
					except:
						pass
				if not ext: fatal("no header found for %s which is a moc file" % str(d))

			# next time we will not search for the extension (look at the 'for' loop below)
			h_node = node.change_ext(ext)
			m_node = node.change_ext('.moc')
			tree.m_depends_on[variant][m_node] = h_node

			# create the task
			task = parn.create_task('moc3_hack', parn.env)
			task.set_inputs(h_node)
			task.set_outputs(m_node)
			moctasks.append(task)
		# look at the file inputs, it is set right above
		for d in tree.m_depends_on[variant][node]:
			deps = tree.m_depends_on[variant]

			name = d.m_name
			if name[-4:]=='.moc':
				task = parn.create_task('moc3_hack', parn.env)
				task.set_inputs(tree.m_depends_on[variant][d])
				task.set_outputs(d)
				moctasks.append(task)
				break
		self.m_run_after = moctasks
		self.moc_done = 1
		return Task.Task.may_start(self)

def create_uic_task(self, node):
	"hook for uic tasks"
	uictask = self.create_task('qt3_uic3', self.env, 6)
	uictask.m_inputs    = [node]
	uictask.m_outputs   = [node.change_ext('.h')]

class qt3obj(cpp.cppobj):
	def __init__(self, type='program'):
		cpp.cppobj.__init__(self, type)
		self.m_linktask = None
		self.m_latask = None

	def get_valid_types(self):
		return ['program', 'shlib', 'staticlib']

	def create_task(self, type, env=None, nice=10):
		"overrides Object.create_task to catch the creation of cpp tasks"

		if env is None: env=self.env
		if type == 'cpp':
			task = MTask(type, env, self, nice)
		elif type == 'cpp_ui':
			task = Task.Task('cpp', env, nice)
		elif type == 'moc3_hack': # add a task while the build has started
			task = Task.Task('qt3_moc', env, nice, normal=0)
			generator = Params.g_build.m_generator
			#generator.m_outstanding.append(task)
			generator.m_outstanding = [task] + generator.m_outstanding
			generator.m_total += 1
		else:
			task = Task.Task(type, env, nice)

		self.m_tasks.append(task)
		if type == 'cpp': self.p_compiletasks.append(task)
		return task

def setup(env):
	Action.simple_action('qt3_moc', '${QT3_MOC} ${QT3_MOC_FLAGS} ${SRC} ${QT3_MOC_ST} ${TGT}', color='BLUE')
	Action.simple_action('qt3_uic3', '${QT3_UIC} ${SRC} -o ${TGT}', color='BLUE')
	Object.register('qt3', qt3obj)

	try: env.hook('qt3', 'QT3_UI_EXT', create_uic_task)
	except: pass

def detect_qt3(conf):

	env = conf.env
	opt = Params.g_options

	try: qt3libs = opt.qt3libs
	except: qt3libs=''

	try: qt3includes = opt.qt3includes
	except: qt3includes=''

	try: qt3bin = opt.qt3bin
	except: qt3bin=''

	# if qt3dir is given - helper for finding qt3libs, qt3includes and qt3bin
	try: qt3dir = opt.qt3dir
	except: qt3dir=''

	if not qt3dir: qt3dir = os.environ.get('QTDIR', '')

	# Debian/Ubuntu support ('/usr/share/qt3/')
	# Gentoo support ('/usr/qt/3')
	if not qt3dir:
		candidates = ['/usr/share/qt3/', '/usr/qt/3/']

		for candidate in candidates:
			if os.path.exists(candidate):
				qt3dir = candidate
				break

	if qt3dir and qt3dir[len(qt3dir)-1] != '/':
		qt3dir += '/'

	if qt3dir:
		env['QT3_DIR'] = qt3dir

	if not qt3dir:
		if qt3libs and qt3includes and qt3bin:
			Params.pprint("YELLOW", "No valid qtdir found; using the specified qtlibs, qtincludes and qtbin params");
		else:
			fatal("Cannot find a valid qtdir, and not enough parameters are set; either specify a qtdir, or qtlibs, qtincludes and qtbin params")

	# check for the qtbinaries
	if not qt3bin: qt3bin = qt3dir + 'bin/'

	# If a qt3bin (or a qt3dir) param was given, test the version-neutral names first
	if qt3bin:
		moc_candidates = ['moc', 'moc-qt3']
		uic_candidates = ['uic', 'uic3', 'uic-qt3']
	else:
		moc_candidates = ['moc-qt3', 'moc']
		uic_candidates = ['uic-qt3', 'uic3', 'uic']

	binpath = [qt3bin] + os.environ['PATH'].split(':')
	def find_bin(lst, var):
		for f in lst:
			ret = conf.find_program(f, path_list=binpath)
			if ret:
				env[var]=ret
				return ret

	Params.pprint("GREEN", "Trying to find uic");
	if not find_bin(uic_candidates, 'QT3_UIC'):
		fatal("uic not found!")
	version = os.popen(env['QT3_UIC'] + " -version 2>&1").read().strip()
	version = version.replace('Qt User Interface Compiler ','')
	version = version.replace('User Interface Compiler for Qt', '')
	if version.find(" 3.") == -1:
		conf.check_message('uic version', '(not a 3.x uic)', 0, option='(%s)'%version)
		sys.exit(1)
	conf.check_message('uic version', '', 1, option='(%s)'%version)

	Params.pprint("GREEN", "Trying to find moc");
	if not find_bin(moc_candidates, 'QT3_MOC'):
		fatal("moc not found!")

	env['QT3_UIC_ST'] = '%s -o %s'
	env['QT3_MOC_ST'] = '-o'

	if not qt3includes: qt3includes = qt3dir + 'include/'
	if not qt3libs: qt3libs = qt3dir + 'lib/'

	# check for the qt-mt package
	Params.pprint("GREEN", "Trying to use the qt-mt pkg-config package");
	pkgconf = conf.create_pkgconfig_configurator()
	pkgconf.name = 'qt-mt'
	pkgconf.uselib = 'QT3'
	pkgconf.path = qt3libs
	if not pkgconf.run():
		Params.pprint("YELLOW", "qt-mt package not found - trying to enumerate paths & flags manually");

		# check for the qt includes first
		lst = [qt3includes, '/usr/qt/3/include', '/usr/include/qt3', '/opt/qt3/include', '/usr/local/include', '/usr/include']
		headertest = conf.create_header_enumerator()
		headertest.name = 'qt.h'
		headertest.path = lst
		headertest.mandatory = 1
		ret = headertest.run()
		env['CPPPATH_QT3'] = ret

		# now check for the qt libs
		lst = [qt3libs, '/usr/qt/3/lib', '/usr/lib/qt3', '/opt/qt3/lib', '/usr/local/lib', '/usr/lib']
		libtest = conf.create_library_enumerator()
		libtest.name = 'qt-mt'
		libtest.path = lst
		libtest.mandatory = 1
		ret = libtest.run()
		env['LIBPATH_QT3'] = ret
		env['LIB_QT3'] = 'qt-mt'

	env['QT3_FOUND'] = 1

	# rpath settings
	# TODO: Check if this works on darwin
	try:
		if Params.g_options.want_rpath_qt3:
			env['RPATH_QT3']=['-Wl,--rpath='+qt3libs]
	except AttributeError:
		pass

def detect(conf):
	if sys.platform=='win32': fatal('Qt3.py will not work on win32 for now - ask the author')
	else: detect_qt3(conf)
	return 0

def set_options(opt):
	try: opt.add_option('--want-rpath-qt3', type='int', default=1, dest='want_rpath_qt3', help='set rpath to 1 or 0 [Default 1]')
	except: pass

	opt.add_option('--header-ext-qt3',
		type='string',
		default='',
		help='header extension for moc files',
		dest='qt3_header_ext')

	qtparams = {}
	qtparams['qt3dir'] = 'manual Qt3 QTDIR specification (autodetected)'
	qtparams['qt3bin'] = 'path to the Qt3 binaries (moc, uic) (autodetected)'
	qtparams['qt3includes'] = 'path to the Qt3 includes (autodetected)'
	qtparams['qt3libs'] = 'path to the Qt3 libraries (autodetected)'
	for name, desc in qtparams.iteritems():
		opt.add_option('--'+name, type='string', default='', help=desc, dest=name)


