#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"""
Qt4 support

If QT4_ROOT is given (absolute path), the configuration will look in it first

This module also demonstrates how to add tasks dynamically (when the build has started)
"""

import os, sys
import ccroot, cpp
import Action, Params, Object, Task, Utils
from Params import error, fatal
from Params import set_globals, globals

set_globals('MOC_H', ['.h', '.hpp', '.hxx', '.hh'])
set_globals('RCC_EXT', ['.qrc'])
set_globals('UI_EXT', ['.ui'])

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
			if not d[-4:] == '.moc': continue
			# paranoid check
			if d in mocfiles:
				error("paranoia owns")
				continue
			# process that base.moc only once
			mocfiles.append(d)

			# find the extension - this search is done only once
			ext = ''
			if Params.g_options.qt_header_ext:
				ext = Params.g_options.qt_header_ext
			else:
				base2 = d[:-4]
				path = node.m_parent.srcpath(parn.env)
				for i in globals('MOC_H'):
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
			task = parn.create_task('moc_hack', parn.env)
			task.set_inputs(h_node)
			task.set_outputs(m_node)
			moctasks.append(task)
		# look at the file inputs, it is set right above
		for d in tree.m_depends_on[variant][node]:
			name = d.m_name
			if name[-4:]=='.moc':
				task = parn.create_task('moc_hack', parn.env)
				task.set_inputs(tree.m_depends_on[variant][d])
				task.set_outputs(d)
				moctasks.append(task)
				break
		self.m_run_after = moctasks
		self.moc_done = 1
		return Task.Task.may_start(self)

def create_rcc_task(self, node):
	"hook for rcc files"
	# run rcctask with one of the highest priority
	# TODO add the dependency on the files listed in .qrc
	rcnode = node.change_ext('_rc.cpp')

	rcctask = self.create_task('rcc', self.env, 6)
	rcctask.m_inputs = [node]
	rcctask.m_outputs = [rcnode]

	cpptask = self.create_task('cpp', self.env)
	cpptask.m_inputs  = [rcnode]
	cpptask.m_outputs = [rcnode.change_ext('.o')]

def create_uic_task(self, node):
	"hook for uic tasks"
	uictask = self.create_task('uic4', self.env, 6)
	uictask.m_inputs    = [node]
	uictask.m_outputs   = [node.change_ext('.h')]

class qt4obj(cpp.cppobj):
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
		elif type == 'moc_hack': # add a task while the build has started
			task = Task.Task('moc', env, nice, normal=0)
			generator = Params.g_build.m_generator
			#generator.m_outstanding.append(task)
			generator.m_outstanding = [task] + generator.m_outstanding
			generator.m_total += 1
		else:
			task = Task.Task(type, env, nice)

		self.m_tasks.append(task)
		if type == 'cpp': self.p_compiletasks.append(task)
		return task

        def apply(self):
		cpp.cppobj.apply(self)
		lst = []
		for flag in self.to_list(self.env['CXXFLAGS']):
			if len(flag) < 2: continue
			if flag[0:2] == '-D' or flag[0:2] == '-I':
				lst.append(flag)
		self.env['MOC_FLAGS'] = lst

def setup(env):
	Action.simple_action('moc', '${QT_MOC} ${MOC_FLAGS} ${SRC} ${MOC_ST} ${TGT}', color='BLUE', vars=['QT_MOC', 'MOC_FLAGS'])
	Action.simple_action('rcc', '${QT_RCC} -name ${SRC[0].m_name} ${SRC} ${RCC_ST} -o ${TGT}', color='BLUE')
	Action.simple_action('uic4', '${QT_UIC} ${SRC} -o ${TGT}', color='BLUE')
	Object.register('qt4', qt4obj)

	try: env.hook('qt4', 'UI_EXT', create_uic_task)
	except: pass

	try: env.hook('qt4', 'RCC_EXT', create_rcc_task)
	except: pass

def detect_qt4(conf):
	env = conf.env
	opt = Params.g_options

	try: qtlibs = opt.qtlibs
	except: qtlibs=''

	try: qtincludes = opt.qtincludes
	except: qtincludes=''

	try: qtbin = opt.qtbin
	except: qtbin=''

	try: useframework = opt.use_qt4_osxframework
	except: useframework = True

	# if qtdir is given - helper for finding qtlibs, qtincludes and qtbin
	try: qtdir = opt.qtdir
	except: qtdir=''

	if not qtdir: qtdir = os.environ.get('QT4_ROOT', '')

	if not qtdir:
		try:
			lst = os.listdir('/usr/local/Trolltech/')
			lst.sort()
			lst.reverse()
			qtdir = '/usr/local/Trolltech/%s/' % lst[0]

		except OSError:
			pass

	if not qtdir:
		try:
			path = os.environ['PATH'].split(':')
			for qmk in ['qmake-qt4', 'qmake4', 'qmake']:
				qmake = conf.find_program(qmk, path)
				if qmake:
					version = os.popen(qmake+" -query QT_VERSION").read().strip().split('.')
					if version[0] == "4":
						qtincludes = os.popen(qmake+" -query QT_INSTALL_HEADERS").read().strip()
						qtdir = os.popen(qmake + " -query QT_INSTALL_PREFIX").read().strip()+"/"
						qtbin = os.popen(qmake + " -query QT_INSTALL_BINS").read().strip()+"/"
						break;
		except OSError:
			pass

	# check for the qt includes first
	if not qtincludes: qtincludes = qtdir + 'include/'
	env['QTINCLUDEPATH']=qtincludes

	lst = [qtincludes, '/usr/share/qt4/include/', '/opt/qt4/include']
        test = conf.create_header_enumerator()
        test.name = 'QtGui/QFont'
	test.path = lst
	test.mandatory = 1
        ret = test.run()


	# check for the qtbinaries
	if not qtbin: qtbin = qtdir + 'bin/'

	binpath = [qtbin, '/usr/share/qt4/bin/'] + os.environ['PATH'].split(':')
	def find_bin(lst, var):
		for f in lst:
			ret = conf.find_program(f, path_list=binpath)
			if ret:
				env[var]=ret
				break

	find_bin(['uic-qt3', 'uic3'], 'QT_UIC3')

	find_bin(['uic-qt4', 'uic'], 'QT_UIC')
	version = os.popen(env['QT_UIC'] + " -version 2>&1").read().strip()
	version = version.replace('Qt User Interface Compiler ','')
	version = version.replace('User Interface Compiler for Qt', '')
	if version.find(" 3.") != -1:
		conf.check_message('uic version', '(too old)', 0, option='(%s)'%version)
		sys.exit(1)
	conf.check_message('uic version', '', 1, option='(%s)'%version)

	find_bin(['moc-qt4', 'moc'], 'QT_MOC')
	find_bin(['rcc'], 'QT_RCC')

	env['UIC3_ST']= '%s -o %s'
	env['UIC_ST'] = '%s -o %s'
	env['MOC_ST'] = '-o'


	# check for the qt libraries
	if not qtlibs: qtlibs = qtdir + 'lib'

	vars = "Qt3Support QtCore QtGui QtNetwork QtOpenGL QtSql QtSvg QtTest QtXml".split()

	framework_ok = False
	if sys.platform == "darwin" and useframework:
		for i in vars:
			e = conf.create_framework_configurator()
			e.path = [qtlibs]
			e.name = i
			e.remove_dot_h = 1
			e.run()

			if not i == 'QtCore':
				# strip -F flag so it don't get reduant
				for r in env['CCFLAGS_' + i.upper()]:
					if r.startswith('-F'):
						env['CCFLAGS_' + i.upper()].remove(r)
						break
		
			incflag = '-I%s' % os.path.join(qtincludes, i)
			if not incflag in env["CCFLAGS_" + i.upper ()]:
				env['CCFLAGS_' + i.upper ()] += [incflag]
			if not incflag in env["CXXFLAGS_" + i.upper ()]:
				env['CXXFLAGS_' + i.upper ()] += [incflag]

		# now we add some static depends.
		if conf.is_defined("HAVE_QTOPENGL"):
			if not '-framework OpenGL' in env["LINKFLAGS_QTOPENGL"]:
				env["LINKFLAGS_QTOPENGL"] += ['-framework OpenGL']

		if conf.is_defined("HAVE_QTGUI"):
			if not '-framework AppKit' in env["LINKFLAGS_QTGUI"]:
				env["LINKFLAGS_QTGUI"] += ['-framework AppKit']
			if not '-framework ApplicationServices' in env["LINKFLAGS_QTGUI"]:
				env["LINKFLAGS_QTGUI"] += ['-framework ApplicationServices']

		framework_ok = True

	if not framework_ok: # framework_ok is false either when the platform isn't OSX, Qt4 shall not be used as framework, or Qt4 could not be found as framework
		vars_debug = map(lambda a: a+'_debug', vars)

		for i in vars_debug+vars:
			#conf.check_pkg(i, pkgpath=qtlibs)
			pkgconf = conf.create_pkgconfig_configurator()
			pkgconf.name = i
			pkgconf.path = qtlibs + ':/usr/lib/qt4/lib:/opt/qt4/lib'
			pkgconf.run()


		# the libpaths are set nicely, unfortunately they make really long command-lines
		# remove the qtcore ones from qtgui, etc
		def process_lib(vars_, coreval):
			for d in vars_:
				var = d.upper()
				if var == 'QTCORE': continue

				value = env['LIBPATH_'+var]
				if value:
					core = env[coreval]
					accu = []
					for lib in value:
						if lib in core: continue
						accu.append(lib)
					env['LIBPATH_'+var] = accu

		process_lib(vars, 'LIBPATH_QTCORE')
		process_lib(vars_debug, 'LIBPATH_QTCORE_DEBUG')

		# rpath if wanted
		if Params.g_options.want_rpath:
			def process_rpath(vars_, coreval):
				for d in vars_:
					var = d.upper()
					value = env['LIBPATH_'+var]
					if value:
						core = env[coreval]
						accu = []
						for lib in value:
							if var != 'QTCORE':
								if lib in core:
									continue
							accu.append('-Wl,--rpath='+lib)
						env['RPATH_'+var] = accu
			process_rpath(vars, 'LIBPATH_QTCORE')
			process_rpath(vars_debug, 'LIBPATH_QTCORE_DEBUG')

	env['QTLOCALE'] = str(env['PREFIX'])+'/share/locale'

def detect(conf):
	if sys.platform=='win32': fatal('Qt4.py will not work on win32 for now - ask the author')
	else: detect_qt4(conf)
	return 0

def set_options(opt):
	try: opt.add_option('--want-rpath', type='int', default=1, dest='want_rpath', help='set rpath to 1 or 0 [Default 1]')
	except: pass

	opt.add_option('--header-ext',
		type='string',
		default='',
		help='header extension for moc files',
		dest='qt_header_ext')

	for i in "qtdir qtincludes qtlibs qtbin".split():
		opt.add_option('--'+i, type='string', default='', dest=i)

	if sys.platform == "darwin":
		opt.add_option('--no-qt4-framework', action="store_false", help='do not use the framework version of Qt4 in OS X', dest='use_qt4_osxframework',default=True)
