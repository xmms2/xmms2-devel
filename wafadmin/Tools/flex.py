#! /usr/bin/env python
# encoding: utf-8
# John O'Meara, 2006

"Flex processing"

import Action
from Params import fatal
from Params import set_globals

flex_str = '${FLEX} -o ${TGT} ${FLEXFLAGS} ${SRC}'

# we register our extensions to global variables
set_globals('EXT_FLEX_C', '.lex.c')
set_globals('EXT_FLEX_CC', '.lex.cc')
set_globals('EXT_FLEX_OUT', '.lex.o')

def l_file(self, node):
	if self.__class__.__name__ == 'ccobj':
		ext = self.env['EXT_FLEX_C']
	elif self.__class__.__name__ == 'cppobj':
		ext = self.env['EXT_FLEX_CC']
	else:
		fatal('neither c nor c++ for flex.py')

	obj_ext = self.env['EXT_FLEX_OUT']

	ltask = self.create_task('flex', nice=40)
	ltask.set_inputs(node)
	ltask.set_outputs(node.change_ext(ext))

	task = self.create_task(self.m_type_initials)
	task.set_inputs(ltask.m_outputs)
	task.set_outputs(node.change_ext(obj_ext))

def setup(env):
	# create our action here
	Action.simple_action('flex', flex_str, color='BLUE')

	# register the hook for use with cppobj and ccobj
	try: env.hook('cpp', 'FLEX_EXT', l_file)
	except: pass
	try: env.hook('cc', 'FLEX_EXT', l_file)
	except: pass

def detect(conf):
	flex = conf.find_program('flex', var='FLEX')
	if not flex: return 0
	v = conf.env
	v['FLEX']      = flex
	v['FLEXFLAGS'] = ''
	v['FLEX_EXT']  = ['.l']
	return 1

