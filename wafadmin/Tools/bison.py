#! /usr/bin/env python
# encoding: utf-8
# John O'Meara, 2006

"Bison processing"

import Action, Object, os
from Params import set_globals

bison_str = 'cd ${SRC[0].bld_dir(env)} && ${BISON} ${BISONFLAGS} ${SRC[0].abspath()} -o ${TGT[0].m_name}'

# we register our extensions to global variables
set_globals('EXT_BISON_C', '.tab.c')

def yc_file(self, node):
	yctask = self.create_task('bison', nice=40)
	yctask.set_inputs(node)

	c_ext = self.env['EXT_BISON_C']
	h_ext = c_ext.replace('.c', '.h')
	o_ext = c_ext.replace('.c', '.o')

	# figure out what nodes bison will build
	sep=node.m_name.rfind(os.extsep)
	endstr = node.m_name[sep+1:]
	if len(endstr) > 1:
		endstr = endstr[1:]
	else:
		endstr = ""
	# set up the nodes
	newnodes = [node.change_ext(c_ext + endstr)]
	if "-d" in self.env['BISONFLAGS']:
		newnodes.append(node.change_ext(h_ext+endstr))
	yctask.set_outputs(newnodes)

	task = self.create_task(self.m_type_initials)
	task.set_inputs(yctask.m_outputs[0])
	task.set_outputs(node.change_ext(o_ext))

def setup(env):
	# create our action here
	Action.simple_action('bison', bison_str, color='BLUE')

	# register the hook for use with cppobj and ccobj
	try: Object.hook('cpp', 'BISON_EXT', yc_file)
	except: pass

	try: Object.hook('cc', 'BISON_EXT', yc_file)
	except: pass

def detect(conf):
	bison = conf.find_program('bison', var='BISON')
	if not bison: return 0
	v = conf.env
	v['BISON']      = bison
	v['BISONFLAGS'] = '-d'
	v['BISON_EXT']  = ['.y', '.yc']
	return 1

