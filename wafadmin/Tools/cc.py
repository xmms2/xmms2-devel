#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Base for c programs/libraries"

import ccroot
import Object, Params, Action
from Params import debug

g_cc_flag_vars = [
'FRAMEWORK', 'FRAMEWORKPATH',
'STATICLIB', 'LIB', 'LIBPATH', 'LINKFLAGS', 'RPATH',
'INCLUDE',
'CCFLAGS', 'CPPPATH', 'CPPLAGS', 'CCDEFINES']

cctypes=['plugin', 'shlib', 'program', 'staticlib', 'objects']
g_cc_type_vars=['CCFLAGS', 'LINKFLAGS', 'obj_ext']
class ccobj(ccroot.ccroot):
	s_default_ext = ['.c', '.cc', '.C']
	def __init__(self, type='program'):
		ccroot.ccroot.__init__(self, type)

		self.ccflags=''
		self.cppflags=''
		self.defines=''

		self._incpaths_lst=[]
		self._bld_incpaths_lst=[]

		self.m_linktask=None
		self.m_deps_linktask=[]

		self.m_type_initials = 'cc'

		global g_cc_flag_vars
		self.p_flag_vars = g_cc_flag_vars

		global g_cc_type_vars
		self.p_type_vars = g_cc_type_vars

	def get_valid_types(self):
		global cctypes
		return cctypes

	def apply_obj_vars(self):
		debug('apply_obj_vars called for ccobj', 'cc')
		env = self.env
		app = env.appendValue

		cpppath_st       = env['CPPPATH_ST']
		lib_st           = env['LIB_ST']
		staticlib_st     = env['STATICLIB_ST']
		libpath_st       = env['LIBPATH_ST']
		staticlibpath_st = env['STATICLIBPATH_ST']

		self.addflags('CCFLAGS', self.ccflags)
		self.addflags('CPPFLAGS', self.cppflags)

		# local flags come first
		# set the user-defined includes paths
		if not self._incpaths_lst: self.apply_incpaths()
		for i in self._bld_incpaths_lst:
			app('_CCINCFLAGS', cpppath_st % i.bldpath(env))
			app('_CCINCFLAGS', cpppath_st % i.srcpath(env))

		# set the library include paths
		for i in env['CPPPATH']:
			app('_CCINCFLAGS', cpppath_st % i)

		# this is usually a good idea
		app('_CCINCFLAGS', cpppath_st % '.')
		app('_CCINCFLAGS', cpppath_st % env.variant())
		try:
			tmpnode = Params.g_build.m_curdirnode
			app('_CCINCFLAGS', cpppath_st % tmpnode.bldpath(env))
			app('_CCINCFLAGS', cpppath_st % tmpnode.srcpath(env))
		except:
			pass

		for i in env['RPATH']:   app('LINKFLAGS', i)
		for i in env['LIBPATH']: app('LINKFLAGS', libpath_st % i)
		for i in env['LIBPATH']: app('LINKFLAGS', staticlibpath_st % i)

		if env['STATICLIB']:
			app('LINKFLAGS', env['STATICLIB_MARKER'])
			for i in env['STATICLIB']:
				app('LINKFLAGS', staticlib_st % i)

		# i doubt that anyone will make a fully static binary anyway
		if not env['FULLSTATIC']:
			if env['STATICLIB'] or env['LIB']:
				app('LINKFLAGS', env['SHLIB_MARKER'])

		for i in env['LIB']: app('LINKFLAGS', lib_st % i)

	def apply_defines(self):
		lst = self.to_list(self.defines)
		milst = self.defines_lst

		# now process the local defines
		tree = Params.g_build
		for defi in lst:
			if not defi in milst:
				milst.append(defi)

		# CCDEFINES_
		libs = self.to_list(self.uselib)
		for l in libs:
			val=''
			try:    val = self.env['CCDEFINES_'+l]
			except: pass
			if val: milst += val

		y = self.env['CCDEFINES_ST']
		self.env['_CCDEFFLAGS'] = map(lambda x: y%x, milst)

def setup(env):
	cc_str = '${CC} ${CCFLAGS} ${CPPFLAGS} ${_CCINCFLAGS} ${_CCDEFFLAGS} ${CC_SRC_F}${SRC} ${CC_TGT_F}${TGT}'
	link_str = '${LINK_CC} ${CCLNK_SRC_F}${SRC} ${CCLNK_TGT_F}${TGT} ${LINKFLAGS} ${_LIBDIRFLAGS} ${_LIBFLAGS}'

	Action.simple_action('cc', cc_str, 'GREEN')

	# on windows libraries must be defined after the object files
	Action.simple_action('cc_link', link_str, color='YELLOW')

	Object.register('cc', ccobj)

def detect(conf):
	return 1

