#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)
# Ralf Habacker, 2006 (rh)

import os, sys
import optparse
import Utils, Action, Params, checks, Configure

def setup(env):
	pass

def detect(conf):
	cc = None
	if conf.env['CC']:
		cc = conf.env['CC']
	elif 'CC' in os.environ:
		cc = os.environ['CC']
	if not cc: cc = conf.find_program('gcc', var='CC')
	if not cc: cc = conf.find_program('cc', var='CC')
	if not cc:
		return 0;

	conf.check_tool('checks')
	# load the cc builders
	conf.check_tool('cc')

	# gcc requires ar for static libs
	if not conf.check_tool('ar'):
		Utils.error('gcc needs ar - not found')
		return 0

	v = conf.env

        cpp = conf.find_program('cpp', var='CPP')
        if not cpp: cpp = cc

	v['CC']  = cc
	v['CPP'] = cpp

	v['CPPFLAGS']             = []
	v['CCDEFINES']            = []
	v['_CCINCFLAGS']          = []
	v['_CCDEFFLAGS']          = []

	v['CC_SRC_F']             = ''
	v['CC_TGT_F']             = '-c -o '
	v['CPPPATH_ST']           = '-I%s' # template for adding include pathes

	# linker
	v['LINK_CC']              = v['CC']
	v['LIB']                  = []
	v['CCLNK_SRC_F']          = ''
	v['CCLNK_TGT_F']          = '-o '

	v['LIB_ST']               = '-l%s'	# template for adding libs
	v['LIBPATH_ST']           = '-L%s' # template for adding libpathes
	v['STATICLIB_ST']         = '-l%s'
	v['STATICLIBPATH_ST']     = '-L%s'
	v['_LIBDIRFLAGS']         = ''
	v['_LIBFLAGS']            = ''
	v['CCDEFINES_ST']         = '-D%s'

	# linker debug levels
	v['LINKFLAGS']            = []
	v['LINKFLAGS_OPTIMIZED']  = ['-s']
	v['LINKFLAGS_RELEASE']    = ['-s']
	v['LINKFLAGS_DEBUG']      = ['-g']
	v['LINKFLAGS_ULTRADEBUG'] = ['-g3']

	v['SHLIB_MARKER']        = '-Wl,-Bdynamic'
	v['STATICLIB_MARKER']    = '-Wl,-Bstatic'

	if sys.platform == "win32":
		# shared library
		v['shlib_CCFLAGS']       = ['']
		v['shlib_LINKFLAGS']     = ['-shared']
		v['shlib_obj_ext']       = ['.os']
		v['shlib_PREFIX']        = 'lib'
		v['shlib_SUFFIX']        = '.dll'
		v['shlib_IMPLIB_SUFFIX'] = ['.dll.a']

		# static library
		v['staticlib_LINKFLAGS'] = ['']
		v['staticlib_obj_ext']   = ['.o']
		v['staticlib_PREFIX']    = 'lib'
		v['staticlib_SUFFIX']    = '.a'

		# program
		v['program_obj_ext']     = ['.o']
		v['program_SUFFIX']      = '.exe'

		# plugins, loadable modules.
		v['plugin_CCFLAGS']      = v['shlib_CCFLAGS']
		v['plugin_LINKFLAGS']    = v['shlib_LINKFLAGS']
		v['plugin_obj_ext']      = v['shlib_obj_ext']
		v['plugin_PREFIX']       = v['shlib_PREFIX']
		v['plugin_SUFFIX']       = v['shlib_SUFFIX']
	elif sys.platform == 'cygwin':
		# shared library
		v['shlib_CCFLAGS']    = ['']
		v['shlib_LINKFLAGS']   = ['-shared']
		v['shlib_obj_ext']     = ['.os']
		v['shlib_PREFIX']      = 'lib'
		v['shlib_SUFFIX']      = '.dll'
		v['shlib_IMPLIB_SUFFIX'] = ['.a']

		# static library
		v['staticlib_LINKFLAGS'] = ['']
		v['staticlib_obj_ext'] = ['.o']
		v['staticlib_PREFIX']  = 'lib'
		v['staticlib_SUFFIX']  = '.a'

		# program
		v['program_obj_ext']   = ['.o']
		v['program_SUFFIX']    = '.exe'

	elif sys.platform == "darwin":
		v['shlib_CCFLAGS']       = ['-fPIC']
		v['shlib_LINKFLAGS']     = ['-dynamiclib']
		v['shlib_obj_ext']       = ['.os']
		v['shlib_PREFIX']        = 'lib'
		v['shlib_SUFFIX']        = '.dylib'

		# static lib
		v['staticlib_LINKFLAGS'] = ['']
		v['staticlib_obj_ext']   = ['.o']
		v['staticlib_PREFIX']    = 'lib'
		v['staticlib_SUFFIX']    = '.a'

		# program
		v['program_obj_ext']     = ['.o']
		v['program_SUFFIX']      = ''

		# bundles
		v['plugin_LINKFLAGS']    = ['-bundle', '-undefined dynamic_lookup']
		v['plugin_obj_ext']      = ['.os']
		v['plugin_CCFLAGS']      = ['-fPIC']
		v['plugin_PREFIX']       = ''
		v['plugin_SUFFIX']       = '.bundle'

		v['SHLIB_MARKER']        = ''
		v['STATICLIB_MARKER']    = ''

	elif sys.platform == 'aix5':
		# shared library
		v['shlib_CCFLAGS']     = ['-fPIC', '-DPIC']
		v['shlib_LINKFLAGS']   = ['-shared','-Wl,-brtl,-bexpfull']
		v['shlib_obj_ext']     = ['_sh.o']
		v['shlib_PREFIX']      = 'lib'
		v['shlib_SUFFIX']      = '.so'

		# plugins, loadable modules.
		v['plugin_CCFLAGS']    = v['shlib_CCFLAGS']
		v['plugin_LINKFLAGS']  = v['shlib_LINKFLAGS']
		v['plugin_obj_ext']    = v['shlib_obj_ext']
		v['plugin_PREFIX']     = v['shlib_PREFIX']
		v['plugin_SUFFIX']     = v['shlib_SUFFIX']

		# static lib
		v['staticlib_obj_ext'] = ['.o']
		v['staticlib_PREFIX']  = 'lib'
		v['staticlib_SUFFIX']  = '.a'

		# program
		v['program_LINKFLAGS'] = ['-Wl,-brtl']
		v['program_obj_ext']   = ['.o']
		v['program_SUFFIX']    = ''

		v['SHLIB_MARKER']      = ''
	else:
		# shared library
		v['shlib_CCFLAGS']       = ['-fPIC', '-DPIC']
		v['shlib_LINKFLAGS']     = ['-shared']
		v['shlib_obj_ext']       = ['.os']
		v['shlib_PREFIX']        = 'lib'
		v['shlib_SUFFIX']        = '.so'

		# plugins. We handle them exactly as shlibs
		# everywhere except on osx, where we do bundles
		v['plugin_CCFLAGS']      = v['shlib_CCFLAGS']
		v['plugin_LINKFLAGS']    = v['shlib_LINKFLAGS']
		v['plugin_obj_ext']      = v['shlib_obj_ext']
		v['plugin_PREFIX']       = v['shlib_PREFIX']
		v['plugin_SUFFIX']       = v['shlib_SUFFIX']

		# static lib
		v['staticlib_LINKFLAGS'] = ['-Wl,-Bstatic']
		v['staticlib_obj_ext']   = ['.o']
		v['staticlib_PREFIX']    = 'lib'
		v['staticlib_SUFFIX']    = '.a'

		# program
		v['program_obj_ext']     = ['.o']
		v['program_SUFFIX']      = ''
	#test if the compiler could build a prog
	test = Configure.check_data()
	test.code = 'int main() {return 0;}\n'
	test.env = v
	test.execute = 1
	test.force_compiler="cc"
	ret = conf.run_check(test)
	conf.check_message('compiler could create', 'pragramms', not (ret is False))
	if not ret:
		return 0
	ret = 0
	#test if the compiler could build a shlib
	lib_obj = Configure.check_data()
	lib_obj.code = "int k = 3;\n"
	lib_obj.env = v
	lib_obj.build_type = "shlib"
	lib_obj.force_compiler = "cc"
	ret = conf.run_check(lib_obj)
	conf.check_message('compiler could create', 'shared libs', not (ret is False))
	if not ret:
		return 0
	ret = 0
	#test if the compiler could build a staiclib
	lib_obj = Configure.check_data()
	lib_obj.code = "int k = 3;\n"
	lib_obj.env = v
	lib_obj.build_type = "staticlib"
	lib_obj.force_compiler = "cc"
	ret = conf.run_check(lib_obj)
	conf.check_message('compiler could create', 'static libs', not (ret is False))
	if not ret:
		return 0

	# compiler debug levels
	if conf.check_flags('-Wall'):
		v['CCFLAGS'] = ['-Wall']
	if conf.check_flags('-O2'):
		v['CCFLAGS_OPTIMIZED'] = ['-O2']
		v['CCFLAGS_RELEASE'] = ['-O2']
	if conf.check_flags('-g -DDEBUG'):
		v['CCFLAGS_DEBUG'] = ['-g', '-DDEBUG']
	if conf.check_flags('-g3 -O0 -DDEBUG'):
		v['CCFLAGS_ULTRADEBUG'] = ['-g3', '-O0', '-DDEBUG']

	# see the option below
	try:
		v.append_value('CCFLAGS', v['CCFLAGS_'+Params.g_options.debug_level.upper()])
	except AttributeError:
		pass

	ron = os.environ
	def addflags(orig, dest=None):
		if not dest: dest=orig
		try: conf.env[dest] = ron[orig]
		except KeyError: pass
	addflags('CFLAGS', 'CCFLAGS')
	addflags('CPPFLAGS')
	addflags('LINKFLAGS')

	if not v['DESTDIR']: v['DESTDIR']=''

	v['program_INST_VAR'] = 'PREFIX'
	v['program_INST_DIR'] = 'bin'
	v['shlib_INST_VAR'] = 'PREFIX'
	v['shlib_INST_DIR'] = 'lib'
	v['staticlib_INST_VAR'] = 'PREFIX'
	v['staticlib_INST_DIR'] = 'lib'

	return 1

def set_options(opt):
	try:
		opt.add_option('-d', '--debug-level',
		action = 'store',
		default = '',
		help = 'Specify the debug level, does nothing if CFLAGS is set in the environment. [Allowed Values: ultradebug, debug, release, optimized]',
		dest = 'debug_level')
	except optparse.OptionConflictError:
		# the g++ tool might have added that option already
		pass
