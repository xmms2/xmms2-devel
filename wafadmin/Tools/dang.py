#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Demo: this hook is called when the class cppobj encounters a '.coin' file: X{.coin -> .cpp -> .o}"

import Action

dang_str = '${DANG} ${SRC} > ${TGT}'
"our action"

def coin_file(self, node):
	"""Create the task for the coin file
	the action 'dang' above is called for this
	the number '4' in the parameters is the priority of the task
	 - lower number means high priority
	 - odd means the task can be run in parallel with others of the same priority number
	"""
	cointask = self.create_task('dang', nice=40)
	cointask.set_inputs(node)
	cointask.set_outputs(node.change_ext('.cpp'))

	# now we also add the task that creates the object file ('.o' file)
	cpptask = self.create_task('cpp')
	cpptask.set_inputs(cointask.m_outputs)
	cpptask.set_outputs(node.change_ext('.o'))

	# for debugging a task, use the following code:
	#cointask.debug(1)

def setup(env):
	# create our action, for use with coin_file
	Action.simple_action('dang', dang_str, color='BLUE')

	# register the hook for use with cppobj
	env.hook('cpp', 'DANG_EXT', coin_file)

def detect(conf):
	dang = conf.find_program('cat', var='DANG')
	if not dang: return 0 # make it fatal
	conf.env['DANG_EXT'] = ['.coin']
	return 1

