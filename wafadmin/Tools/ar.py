#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)
# Ralf Habacker, 2006 (rh)

"ar and ranlib"

import Action

ar_str = '${AR} ${ARFLAGS} ${TGT} ${SRC} && ${RANLIB} ${RANLIBFLAGS} ${TGT}'

def setup(env):
	Action.simple_action('cpp_link_static', ar_str, color='YELLOW')
	Action.simple_action('cc_link_static', ar_str, color='YELLOW')

def detect(conf):
	comp = conf.find_program('ar', var='AR')
	if not comp: return 0;

	ranlib = conf.find_program('ranlib', var='RANLIB')
	if not ranlib: return 0

	v = conf.env
	v['AR']          = comp
	v['ARFLAGS']     = 'r'
	v['RANLIB']      = ranlib
	v['RANLIBFLAGS'] = ''
	return 1

