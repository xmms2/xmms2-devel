#! /usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2006 (dv)
# Visual C support - beta, needs more testing

import os, sys
import Utils, Action, Params

def setup(env):
	static_link_str = '${STLIBLINK_CXX} ${CPPLNK_SRC_F}${SRC} ${CPPLNK_TGT_F}${TGT} ${LINKFLAGS} ${_LIBDIRFLAGS} ${_LIBFLAGS}'

	# on windows libraries must be defined after the object files
	Action.simple_action('cc_link_static', static_link_str, color='YELLOW')
	Action.simple_action('cpp_link_static', static_link_str, color='YELLOW')

def detect(conf):

	comp = conf.find_program('CL', var='CXX')
	if not comp:
		return 0;

	link = conf.find_program('LINK')
	if not link:
		return 0;

	stliblink = conf.find_program('LIB')
	if not stliblink:
		return 0;


	# load the cpp builders
	conf.check_tool('cc cpp')

	v = conf.env

	# c/c++ compiler - needs to be in parenthesis because the default path includes whitespaces
	v['CC']                 = '\"%s\"' % comp
	v['CXX']                 = '\"%s\"' % comp

	v['CPPFLAGS']            = ['/Wall', '/nologo', '/c', '/ZI', '/EHsc', '/errorReport:prompt']
	v['CCDEFINES']          = ['WIN32'] # command-line defines
	v['CXXDEFINES']          = ['WIN32'] # command-line defines

	v['_CCINCFLAGS']        = []
	v['_CCDEFFLAGS']        = []
	v['_CXXINCFLAGS']        = []
	v['_CXXDEFFLAGS']        = []

	v['CC_SRC_F']           = ''
	v['CC_TGT_F']           = '/Fo'
	v['CXX_SRC_F']           = ''
	v['CXX_TGT_F']           = '/Fo'

	v['CPPPATH_ST']          = '/I%s' # template for adding include paths


	# Subsystem specific flags
	v['CPPFLAGS_CONSOLE']		= ['/SUBSYSTEM:CONSOLE']
	v['CPPFLAGS_NATIVE']		= ['/SUBSYSTEM:NATIVE']
	v['CPPFLAGS_POSIX']			= ['/SUBSYSTEM:POSIX']
	v['CPPFLAGS_WINDOWS']		= ['/SUBSYSTEM:WINDOWS']
	v['CPPFLAGS_WINDOWSCE']	= ['/SUBSYSTEM:WINDOWSCE']


	# CRT specific flags
	v['CPPFLAGS_CRT_MULTITHREADED'] =						['/MT']
	v['CPPFLAGS_CRT_MULTITHREADED_DLL'] =				['/MD']
	v['CPPDEFINES_CRT_MULTITHREADED'] =					['_MT']
	v['CPPDEFINES_CRT_MULTITHREADED_DLL'] =			['_MT', '_DLL']

	v['CPPFLAGS_CRT_MULTITHREADED_DBG'] =				['/MTd']
	v['CPPFLAGS_CRT_MULTITHREADED_DLL_DBG'] =		['/MDd']
	v['CPPDEFINES_CRT_MULTITHREADED'] =					['_DEBUG', '_MT']
	v['CPPDEFINES_CRT_MULTITHREADED_DLL'] =			['_DEBUG', '_MT', '_DLL']


	# compiler debug levels
	v['CCFLAGS']            = ['/TC']
	v['CCFLAGS_OPTIMIZED']  = ['/O2', '/DNDEBUG']
	v['CCFLAGS_RELEASE']    = ['/O2', '/DNDEBUG']
	v['CCFLAGS_DEBUG']      = ['/Od', '/RTC1', '/D_DEBUG']
	v['CCFLAGS_ULTRADEBUG'] = ['/Od', '/RTC1', '/D_DEBUG']

	v['CXXFLAGS']            = ['/TP']
	v['CXXFLAGS_OPTIMIZED']  = ['/O2', '/DNDEBUG']
	v['CXXFLAGS_RELEASE']    = ['/O2', '/DNDEBUG']
	v['CXXFLAGS_DEBUG']      = ['/Od', '/RTC1', '/D_DEBUG']
	v['CXXFLAGS_ULTRADEBUG'] = ['/Od', '/RTC1', '/D_DEBUG']


	# linker
	v['STLIBLINK_CXX']       = '\"%s\"' % stliblink
	v['LINK_CXX']            = '\"%s\"' % link
	v['LIB']                 = []

	v['CPPLNK_TGT_F']        = '/OUT:'
	v['CPPLNK_SRC_F']        = ' '

	v['LIB_ST']              = '%s.lib'	# template for adding libs
	v['LIBPATH_ST']          = '/LIBPATH:%s' # template for adding libpathes
	v['STATICLIB_ST']        = '%s.lib'
	v['STATICLIBPATH_ST']    = '/LIBPATH:%s'
	v['CCDEFINES_ST']       = '/D%s'
	v['CXXDEFINES_ST']       = '/D%s'
	v['_LIBDIRFLAGS']        = ''
	v['_LIBFLAGS']           = ''

	v['SHLIB_MARKER']        = ''
	v['STATICLIB_MARKER']    = ''


	# linker debug levels
	v['LINKFLAGS']           = ['/NOLOGO', '/MACHINE:X86', '/ERRORREPORT:PROMPT']
	v['LINKFLAGS_OPTIMIZED'] = ['']
	v['LINKFLAGS_RELEASE']   = ['/OPT:REF', '/OPT:ICF', '/INCREMENTAL:NO']
	v['LINKFLAGS_DEBUG']     = ['/DEBUG', '/INCREMENTAL']
	v['LINKFLAGS_ULTRADEBUG'] = ['/DEBUG', '/INCREMENTAL']

	debuglevel = 'DEBUG'
	try:
		debuglevel = Params.g_options.debug_level.uppercase()
	except:
		pass
	v['CCFLAGS']   += v['CCFLAGS_'+debuglevel]
	v['LINKFLAGS'] += v['LINKFLAGS_'+debuglevel]

	def addflags(var):
		try:
			c = os.environ[var]
			if c: v[var].append(c)
		except:
			pass

	addflags('CXXFLAGS')
	addflags('CPPFLAGS')

	if not v['DESTDIR']: v['DESTDIR']=''


	# shared library
	v['shlib_CCFLAGS']    = ['']
	v['shlib_CXXFLAGS']    = ['']
	v['shlib_LINKFLAGS']   = ['/DLL']
	v['shlib_obj_ext']     = ['.obj']
	v['shlib_PREFIX']      = ''
	v['shlib_SUFFIX']      = '.dll'
	v['shlib_IMPLIB_SUFFIX'] = ['.lib']

	# static library
	v['staticlib_LINKFLAGS'] = ['']
	v['staticlib_obj_ext'] = ['.obj']
	v['staticlib_PREFIX']  = ''
	v['staticlib_SUFFIX']  = '.lib'

	# program
	v['program_obj_ext']   = ['.obj']
	v['program_SUFFIX']    = '.exe'

	return 1


def set_options(opt):
	try:
		opt.add_option('-d', '--debug-level',
		action = 'store',
		default = 'debug',
		help = 'Specify the debug level. [Allowed values: ultradebug, debug, release, optimized]',
		dest = 'debug_level')
	except:
		pass
