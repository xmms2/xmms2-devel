#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)
# Ralf Habacker, 2006 (rh)

import os, sys, pproc, Object
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
	if not cc: cc = conf.find_program('cc', var='CC')
	if not cc:
		return 0;
	#TODO: Has anyone a better ida to check if this is a sun cc?
	ret = os.popen("%s -flags" %cc).close()
	if ret:
		conf.check_message('suncc', '', not ret)
		return 0 #at least gcc exit with error
	conf.check_tool('checks')
	
	# load the cc builders
	conf.check_tool('cc')

	# sun cc requires ar for static libs
	if not conf.check_tool('ar'):
		Utils.error('suncc needs ar - not found')
		return 0

	v = conf.env

	cpp = cc

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

	v['SHLIB_MARKER']        = '-Bdynamic'
	v['STATICLIB_MARKER']    = '-Bstatic'
	
	# shared library
	v['shlib_CCFLAGS']       = ['-Kpic', '-DPIC']
	v['shlib_LINKFLAGS']     = ['-G']
	v['shlib_obj_ext']       = ['.o']
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
	v['staticlib_LINKFLAGS'] = ['-Bstatic']
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
	ret = conf.run_check(test, "program", "cc")
	conf.check_message('compiler could create', 'pragramms', not (ret is False))
	if not ret:
		return 0
	ret = 0
	#test if the compiler could build a shlib
	lib_obj = Configure.check_data()
	lib_obj.code = "int k = 3;\n"
	lib_obj.env = v
	ret = conf.run_check(lib_obj, "shlib", "cc")
	conf.check_message('compiler could create', 'shared libs', not (ret is False))
	if not ret:
		return 0
	ret = 0
	#test if the compiler could build a staiclib
	lib_obj = Configure.check_data()
	lib_obj.code = "int k = 3;\n"
	lib_obj.env = v
	ret = conf.run_check(lib_obj, "staticlib", "cc")
	conf.check_message('compiler could create', 'static libs', not (ret is False))
	if not ret:
		return 0

	# compiler debug levels
	v['CCFLAGS'] = ['-O']
	if conf.check_flags('-O2'):
		v['CCFLAGS_OPTIMIZED'] = ['-O2']
		v['CCFLAGS_RELEASE'] = ['-O2']
	if conf.check_flags('-g -DDEBUG'):
		v['CCFLAGS_DEBUG'] = ['-g', '-DDEBUG']
	if conf.check_flags('-g3 -O0 -DDEBUG'):
		v['CCFLAGS_ULTRADEBUG'] = ['-g3', '-O0', '-DDEBUG']

	# see the option below
	try:
		v['CCFLAGS'] = v['CCFLAGS_'+Params.g_options.debug_level.upper()]
	except AttributeError:
		pass

	ron = os.environ
	def addflags(orig, dest=None):
		if not dest: dest=orig
		try: conf.env[dest] = ron[orig]
		except KeyError: pass
	addflags('CCFLAGS', 'CFLAGS')
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
