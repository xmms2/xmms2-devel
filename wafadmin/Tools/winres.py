#! /usr/bin/env python
# encoding: utf-8
# Brant Young, 2007

"This hook is called when the class cppobj/ccobj encounters a '.rc' file: X{.rc -> [.res|.rc.o]}"

import os
import Action

winrc_str = '${WINRC} ${_CPPDEFFLAGS} ${_CXXDEFFLAGS} ${_CCDEFFLAGS} ${WINRCFLAGS} ${_CPPINCFLAGS} ${_CXXINCFLAGS} ${_CCINCFLAGS} ${WINRC_TGT_F}${TGT} ${WINRC_SRC_F}${SRC}'

def rc_file(self, node):
	rctask = self.create_task('winrc', nice=40)
	rctask.set_inputs(node)
	rctask.set_outputs(node.change_ext(self.env['winrc_obj_ext']))

	# make linker can find compiled resource files
	self.p_compiletasks.append(rctask)

def setup(env):
	# create our action, for use with rc file
	Action.simple_action('winrc', winrc_str, color='BLUE')

	# register the hook for use with cppobj and ccobj
	try: env.hook('cpp', 'WINRC_EXT', rc_file)
	except: pass

	try: env.hook('cc', 'WINRC_EXT', rc_file)
	except: pass

def detect(conf):
	v = conf.env

	cc = os.path.basename(''.join(v['CC']).lower())
	cxx = os.path.basename(''.join(v['CXX']).lower())

	if cc.find('gcc')>-1 or cc.find('cc')>-1 or cxx.find('g++')>-1 or cxx.find('c++')>-1 :
		# find windres while use gcc toolchain
		winrc = conf.find_program('windres', var='WINRC')
		v['WINRC_TGT_F'] = '-o '
		v['WINRC_SRC_F'] = '-i '
		v['winrc_obj_ext'] = '.rc.o' # in case if a .o already exists
	elif cc.find('cl.exe')>-1 or cxx.find('cl.exe')>-1 :
		# find rc.exe while use msvc
		winrc = conf.find_program('RC', var='WINRC')
		v['WINRC_TGT_F'] = '/fo '
		v['WINRC_SRC_F'] = ' '
		v['winrc_obj_ext'] = '.res'
	else :
		return 0

	if not winrc :
		return 0 # make it fatal

	v['WINRC_EXT'] = ['.rc']
	v['WINRCFLAGS'] = ''
	return 1

