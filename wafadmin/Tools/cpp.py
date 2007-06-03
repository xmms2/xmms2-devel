#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Base for c++ programs and libraries"

import ccroot
import Object, Params, Action

g_cpp_flag_vars = [
'FRAMEWORK', 'FRAMEWORKPATH',
'STATICLIB', 'LIB', 'LIBPATH', 'LINKFLAGS', 'RPATH',
'INCLUDE',
'CXXFLAGS', 'CCFLAGS', 'CPPPATH', 'CPPLAGS', 'CXXDEFINES']
"main cpp variables"

g_cpp_type_vars=['CXXFLAGS', 'LINKFLAGS', 'obj_ext']
class cppobj(ccroot.ccroot):
	s_default_ext = ['.c', '.cpp', '.cc', '.cxx']
	def __init__(self, type='program', subtype=None):
		ccroot.ccroot.__init__(self, type, subtype)

		self.cxxflags=''
		self.cppflags=''

		self._incpaths_lst=[]
		self._bld_incpaths_lst=[]

		self.m_linktask=None
		self.m_deps_linktask=[]

		self.m_type_initials = 'cpp'

		global g_cpp_flag_vars
		self.p_flag_vars = g_cpp_flag_vars

		global g_cpp_type_vars
		self.p_type_vars = g_cpp_type_vars

	def apply_defines(self):
		tree = Params.g_build
		lst = self.to_list(self.defines)+self.to_list(self.env['CXXDEFINES'])
		milst = []

		# now process the local defines
		for defi in lst:
			if not defi in milst:
				milst.append(defi)

		# CXXDEFINES_USELIB
		libs = self.to_list(self.uselib)
		for l in libs:
			val = self.env['CXXDEFINES_'+l]
			if val: milst += self.to_list(val)
		self.env['DEFLINES'] = map(lambda x: "define %s"%  ' '.join(x.split('=', 1)), milst)
		y = self.env['CXXDEFINES_ST']
		self.env['_CXXDEFFLAGS'] = map(lambda x: y%x, milst)

def setup(env):
	cpp_str = '${CXX} ${CXXFLAGS} ${CPPFLAGS} ${_CXXINCFLAGS} ${_CXXDEFFLAGS} ${CXX_SRC_F}${SRC} ${CXX_TGT_F}${TGT}'
	link_str = '${LINK_CXX} ${CPPLNK_SRC_F}${SRC} ${CPPLNK_TGT_F}${TGT} ${LINKFLAGS} ${_LIBDIRFLAGS} ${_LIBFLAGS}'

	Action.simple_action('cpp', cpp_str, color='GREEN')

	# on windows libraries must be defined after the object files
	Action.simple_action('cpp_link', link_str, color='YELLOW')

	Object.register('cpp', cppobj)

def detect(conf):
	return 1

