#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"""
Custom objects:
 - execute a function everytime
 - copy a file somewhere else
"""

import shutil, re, os, types
import Object, Action, Node, Params, Utils
from Params import fatal

def copy_func(task):
	"Make a file copy. This might be used to make other kinds of file processing (even calling a compiler is possible)"
	env = task.m_env
	infile = task.m_inputs[0].abspath(env)
	outfile = task.m_outputs[0].abspath(env)
	try:
		shutil.copy2(infile, outfile)
		if task.chmod: os.chmod(outfile, task.chmod)
		return 0
	except:
		return 1

def action_process_file_func(task):
	"Ask the function attached to the task to process it"
	if not task.fun: fatal('task must have a function attached to it for copy_func to work!')
	return task.fun(task)

class cmdobj(Object.genobj):
	"This object will call a command everytime"
	def __init__(self, type='none'):
		Object.genobj.__init__(self, 'other')
		self.m_type = type
		self.prio   = 1
		self.fun    = None

	def apply(self):
		# create a task
		if not self.fun: fatal('cmdobj needs a function!')
		import Task
		Task.TaskCmd(self.fun, self.env, self.prio)

class copyobj(Object.genobj):
	"By default, make a file copy, if fun is provided, fun will make the copy (or call a compiler, etc)"
	def __init__(self, type='none'):
		Object.genobj.__init__(self, 'other')

		self.source = ''
		self.target = ''
		self.chmod  = ''
		self.fun = copy_func

		self.env = Params.g_build.m_allenvs['default'].copy()

	def apply(self):

		lst = self.to_list(self.source)

		for filename in lst:
			node = self.m_current_path.find_node( 
				Utils.split_path(filename) )
			if not node: fatal('cannot find input file %s for processing' % filename)

			target = self.target
			if not target or len(lst)>1: target = node.m_name

			# TODO the file path may be incorrect
			newnode = self.m_current_path.search_existing_node( 
				Utils.split_path(target) )
			if not newnode:
				newnode = Node.Node(target, self.m_current_path)
				self.m_current_path.append_build(newnode)

			task = self.create_task('copy', self.env, 8)
			task.set_inputs(node)
			task.set_outputs(newnode)
			task.m_env = self.env
			task.fun = self.fun
			task.chmod = self.chmod

			if not task.m_env:
				task.debug()
				fatal('task witout an environment')

def subst_func(task):
	"Substitutes variables in a .in file"

	m4_re = re.compile('@(\w+)@', re.M)

	env = task.m_env
	infile = task.m_inputs[0].abspath(env)
	outfile = task.m_outputs[0].abspath(env)

	file = open(infile, 'r')
	code = file.read()
	file.close()

	s = m4_re.sub(r'%(\1)s', code)

	dict = task.dict
	if not dict:
		names = m4_re.findall(code)
		for i in names:
			if task.m_env[i] and type(task.m_env[i]) is types.ListType :
				dict[i] = " ".join( task.m_env[i] )
			else: dict[i] = task.m_env[i]

	file = open(outfile, 'w')
	file.write(s % dict)
	file.close()

	return 0

class substobj(Object.genobj):
	def __init__(self, type='none'):
		Object.genobj.__init__(self, 'other')
		self.fun = subst_func
		self.dict = {}
		self.prio = 8
	def apply(self):

		lst = self.to_list(self.source)

		for filename in lst:
			node = self.m_current_path.find_node(Utils.split_path(filename))
			if not node: fatal('cannot find input file %s for processing' % filename)

			newnode = node.change_ext('')

			task = self.create_task('copy', self.env, self.prio)
			task.set_inputs(node)
			task.set_outputs(newnode)
			task.m_env = self.env
			task.fun = self.fun
			task.dict = self.dict

			if not task.m_env:
				task.debug()
				fatal('task witout an environment')

def setup(env):
	Object.register('cmd', cmdobj)
	Object.register('copy', copyobj)
	Object.register('subst', substobj)
	Action.Action('copy', vars=[], func=action_process_file_func)

def detect(conf):
	return 1

