#! /usr/bin/env python
# encoding: utf-8
# Carlos Rafael Giani, 2007 (dv)

import sys

def setup(env):
	pass

def detect(conf):
	d_compiler = None
	if conf.env['D_COMPILER']:
		d_compiler = conf.env['D_COMPILER']
	if not d_compiler: d_compiler = conf.find_program('gdc', var='D_COMPILER')
	if not d_compiler: return 0

	conf.check_tool('d')

	if not conf.check_tool('ar'):
		Utils.error('ar is needed for static libraries - not found')
		return 0

	v = conf.env

	#compiler
	v['D_COMPILER']           = d_compiler

	v['DFLAGS']               = []
	v['_DIMPORTFLAGS']        = []

	v['D_SRC_F']              = ''
	v['D_TGT_F']              = '-c -o '
	v['DPATH_ST']             = '-I%s' # template for adding import paths

	# linker
	v['D_LINKER']             = v['D_COMPILER']
	v['DLNK_SRC_F']           = ''
	v['DLNK_TGT_F']           = '-o '

	v['DLIB_ST']              = '-l%s' # template for adding libs
	v['DLIBPATH_ST']          = '-L%s' # template for adding libpaths
	v['_DLIBDIRFLAGS']        = ''
	v['_DLIBFLAGS']           = ''

	# debug levels
	v['DLINKFLAGS']            = []
	v['DFLAGS_OPTIMIZED']      = ['-O3']
	v['DFLAGS_DEBUG']          = ['-O0']
	v['DFLAGS_ULTRADEBUG']     = ['-O0']
	v['DLINKFLAGS_OPTIMIZED']  = ['-s']
	v['DLINKFLAGS_RELEASE']    = ['-s']
	v['DLINKFLAGS_DEBUG']      = ['-g']
	v['DLINKFLAGS_ULTRADEBUG'] = ['-g3']


	if sys.platform == "win32":
		# shared library
		v['D_shlib_DFLAGS']        = ['']
		v['D_shlib_LINKFLAGS']     = ['-shared']
		v['D_shlib_obj_ext']       = ['.os']
		v['D_shlib_PREFIX']        = 'lib'
		v['D_shlib_SUFFIX']        = '.dll'
		v['shlib_IMPLIB_SUFFIX'] = ['.a']

		# static library
		v['D_staticlib_obj_ext']   = ['.o']
		v['D_staticlib_PREFIX']    = 'lib'
		v['D_staticlib_SUFFIX']    = '.a'

		# program
		v['D_program_obj_ext']     = ['.o']
		v['D_program_PREFIX']      = ''
		v['D_program_SUFFIX']      = '.exe'

	else:
		# shared library
		v['D_shlib_DFLAGS']        = ['']
		v['D_shlib_LINKFLAGS']     = ['-shared']
		v['D_shlib_obj_ext']       = ['.os']
		v['D_shlib_PREFIX']        = 'lib'
		v['D_shlib_SUFFIX']        = '.so'

		# static lib
		v['D_staticlib_obj_ext']   = ['.o']
		v['D_staticlib_PREFIX']    = 'lib'
		v['D_staticlib_SUFFIX']    = '.a'

		# program
		v['D_program_obj_ext']     = ['.o']
		v['D_program_PREFIX']      = ''
		v['D_program_SUFFIX']      = ''


	return 1


def set_options(opt):
	pass
