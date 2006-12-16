#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"""
genobj is an abstract class for declaring targets:
  - creates tasks (consisting of a task, an environment, a list of source and list of target)
  - sets environment on the tasks (which are copies most of the time)
  - modifies environments as needed
  - genobj cannot be used as it is, so you must create a subclass

subclassing
  - makes it possible to share environment copies for several objects at once (efficiency)
  - be careful to call Object.genobj.__init__(...) in the __init__ of your subclass
  - examples are ccroot, ocamlobj, ..

hooks
  - declare new kind of targets quickly (give a pattern ? and the action name)
  - several extensions are mapped to a single method
  - they do not work with all objects (work with ccroot)
  - cf bison.py and flex.py for more details on this scheme
"""

import types
import Params, Task, Common, Node, Utils
from Params import debug, error, fatal

g_allobjs=[]

def find_launch_node(node, lst):
	#if node.m_parent: print node, lst
	#else: print '/', lst
	if not lst: return node
	name=lst[0]
	if not name:     return find_launch_node(node, lst[1:])
	if name == '.':  return find_launch_node(node, lst[1:])
	if name == '..': return find_launch_node(node.m_parent, lst[1:])

	res = node.get_dir(name)
	if not res:
		res = node.get_file(name)
	if res: return find_launch_node(res, lst[1:])

	return None

def flush():
	"force all objects to post their tasks"

	bld = Params.g_build
	debug("delayed operation Object.flush() called", 'object')

	dir_lst = Utils.split_path(Params.g_launchdir)
	root    = bld.m_root
	launch_dir_node = find_launch_node(root, dir_lst)
	if Params.g_options.compile_targets:
		compile_targets = Params.g_options.compile_targets.split(',')
	else:
		compile_targets = None

	for obj in bld.m_outstanding_objs:
		debug("posting object", 'object')

		if obj.m_posted: continue

		# compile only targets under the launch directory
		if launch_dir_node:
			objnode = obj.m_current_path
			if not (objnode is launch_dir_node or objnode.is_child_of(launch_dir_node)):
				continue
		if compile_targets:
			if obj.name and not (obj.name in compile_targets):
				debug("skipping because of name", 'object')
				continue
			if not obj.target in compile_targets:
				debug("skipping because of target", 'object')
				continue
		# post the object
		obj.post()

		if Params.g_options.verbose == 3:
			import time
			print "flushed at ", time.asctime(time.localtime())

def hook(objname, var, func):
	"Attach a new method to an object class (objname is the name of the class)"
	klass = g_allclasses[objname]
	klass.__dict__[var] = func
	try: klass.__dict__['all_hooks'].append(var)
	except: klass.__dict__['all_hooks'] = [var]

class genobj:
	def __init__(self, type):
		self.m_type  = type
		self.m_posted = 0
		self.m_current_path = Params.g_build.m_curdirnode # emulate chdir when reading scripts
		self.name = '' # give a name to the target (static+shlib with the same targetname ambiguity)

		# TODO if we are building something, we need to make sure the folder is scanned
		#if not Params.g_build.m_curdirnode in Params...

		# targets / sources
		self.source = ''
		self.target = ''

		# we use a simple list for the tasks TODO not used ?
		self.m_tasks = []

		# no default environment - in case if
		self.env = None

		# register ourselves - used at install time
		g_allobjs.append(self)

		# nodes that this object produces
		self.out_nodes = []

		# allow delayed operations on objects created (declarative style)
		# an object is then posted when another one is added
		# of course, you may want to post the object yourself first :)
		#flush()
		Params.g_build.m_outstanding_objs.append(self)

		if not type in self.get_valid_types():
			error("BUG genobj::init : invalid type given")

	def get_valid_types(self):
		return ['program', 'shlib', 'staticlib', 'other']

	def get_hook(self, ext):
		try:
			for i in self.__class__.__dict__['all_hooks']:
				if ext in self.env[i]:
					return self.__class__.__dict__[i]
		except:
			return None

	def post(self):
		"runs the code to create the tasks, do not subclass"
		if not self.env: self.env = Params.g_build.m_allenvs['default']
		if not self.name: self.name = self.target
		try:
			self.sources
			error("typo: self.sources -> self.source")
		except AttributeError:
			pass

		if self.m_posted:
			error("OBJECT ALREADY POSTED")
			return
		self.apply()
		self.m_posted=1

	def create_task(self, type, env=None, nice=10):
		"""the lower the nice is, the higher priority tasks will run at
		groups are sorted like this [2, 4, 5, 100, 200]
		the tasks with lower nice will run first
		if tasks have an odd priority number, they will be run only sequentially
		if tasks have an even priority number, they will be allowed to be run in parallel
		"""
		if env is None: env=self.env
		task = Task.Task(type, env, nice)
		self.m_tasks.append(task)
		return task

	def apply(self):
		"subclass me"
		fatal("subclass me!")

	def find(self, filename):
		node = self.m_current_path
		for name in Utils.split_path(filename):
			res = node.get_file(name)
			if not res:
				res = node.get_build(name)
			if not res:
				res = node.get_dir(name)

			if not res:
				# bld node does not exist, create it
				node2 = Node.Node(name, node)
				node.append_build(node2)
				node = node2
			else:
				node = res
		return node

	# an object is to be posted, even if only for install
	# the install function is called for uninstalling too
	def install(self):
		# subclass me
		pass

	def cleanup(self):
		# subclass me if necessary
		pass

	def install_results(self, var, subdir, task, chmod=0644):
		debug('install results called', 'object')
		current = Params.g_build.m_curdirnode
		# TODO what is the pythonic replacement for these three lines ?
		lst = []
		for node in task.m_outputs:
			lst.append( node.relpath_gen(current) )
		Common.install_files(var, subdir, lst, chmod=chmod)

	def clone(self, env):
		newobj = Utils.copyobj(self)

		if type(env) is types.StringType:
			newobj.env = Params.g_build.m_allenvs[env]
		else:
			newobj.env = env

		g_allobjs.append(newobj)
		Params.g_build.m_outstanding_objs.append(newobj)

		return newobj

	def to_list(self, value):
		"helper: returns a list"
		if type(value) is types.StringType: return value.split()
		else: return value

def flatten(env, var):
	try:
		v = env[var]
		if not v: debug("variable %s does not exist in env !" % var, 'object')

		if type(v) is types.ListType:
			return " ".join(v)
		else:
			return v
	except:
		fatal("variable %s does not exist in env !" % var)

g_cache_max={}
def sign_env_vars(env, vars_list):
	" ['CXX', ..] -> [env['CXX'], ..]"

	# ccroot objects use the same environment for building the .o at once
	# the same environment and the same variables are used
	s = str([env.m_idx]+vars_list)
	try: return g_cache_max[s]
	except KeyError: pass

	def get_env_value(var):
		v = env[var]
		if type(v) is types.ListType: return " ".join(v)
		else: return v

	lst = map(get_env_value, vars_list)
	ret = Params.h_list(lst)
	if Params.g_zones: debug("%s %s" % (Params.vsig(ret), str(lst)), 'envhash')
	# next time
	g_cache_max[s] = ret
	return ret

# TODO there is probably a way to make this more simple
g_allclasses = {}
def register(name, classval):
	global g_allclasses
	if name in g_allclasses:
		debug('class exists in g_allclasses '+name, 'object')
		return
	g_allclasses[name] = classval

