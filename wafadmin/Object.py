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

import os, types, time
import Params, Task, Common, Node, Utils
from Params import debug, error, fatal

g_allobjs=[]
"contains all objects, provided they are created (not in distclean or in dist)"

def flush():
	"object instances under the launch directory create the tasks now"

	tree = Params.g_build
	debug("delayed operation Object.flush() called", 'object')

	dir_lst = Utils.split_path(Params.g_cwd_launch)
	launch_dir_node = tree.m_root.find_dir(Params.g_cwd_launch)

	if launch_dir_node.is_child_of(tree.m_bldnode):
		launch_dir_node=tree.m_srcnode

	if Params.g_options.compile_targets:
		compile_targets = Params.g_options.compile_targets.split(',')
	else:
		compile_targets = None

	for obj in tree.m_outstanding_objs:
		debug("posting object", 'object')

		if obj.m_posted: continue

		if launch_dir_node:
			objnode = obj.path
			if not objnode.is_child_of(launch_dir_node):
				continue
		if compile_targets:
			if obj.name and not (obj.name in compile_targets):
				debug('skipping because of name', 'object')
				continue
			if not obj.target in compile_targets:
				debug('skipping because of target', 'object')
				continue
		obj.post()
		if Params.g_options.verbose == 3:
			print "flushed at ", time.asctime(time.localtime())

def hook(objname, var, func):
	"Attach a new method to an object class (objname is the name of the class)"
	klass = g_allclasses[objname]
	klass.__dict__[var] = func
	try: klass.__dict__['all_hooks'].append(var)
	except KeyError: klass.__dict__['all_hooks'] = [var]

class genobj:
	def __init__(self, type):
		self.m_type  = type
		self.m_posted = 0
		self.path = Params.g_build.m_curdirnode # emulate chdir when reading scripts
		self.name = '' # give a name to the target (static+shlib with the same targetname ambiguity)

		# targets / sources
		self.source = ''
		self.target = ''

		# collect all tasks in a list - a few subclasses need it
		self.m_tasks = []

		# no default environment - in case if
		self.env = None

		# register ourselves - used at install time
		g_allobjs.append(self)

		# allow delayed operations on objects created (declarative style)
		# an object is then posted when another one is added
		# Objects can be posted manually, but this can break a few things, use with care
		Params.g_build.m_outstanding_objs.append(self)

		if not type in self.get_valid_types():
			error("BUG genobj::init : invalid type given")

	def get_valid_types(self):
		return ['program', 'shlib', 'staticlib', 'other']

	def get_hook(self, ext):
		env=self.env
		try:
			for i in self.__class__.__dict__['all_hooks']:
				if ext in env[i]:
					return self.__class__.__dict__[i]
		except KeyError:
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
		groups are sorted in ascending order [2, 3, 4], the tasks with lower nice will run first
		if tasks have an odd priority number, they will be run only sequentially
		if tasks have an even priority number, they will be allowed to be run in parallel
		"""
		if env is None: env=self.env
		task = Task.Task(type, env, nice)
		self.m_tasks.append(task)
		return task

	def apply(self):
		"Subclass me"
		fatal("subclass me!")

	def install(self):
		"subclass me"
		pass

	def cleanup(self):
		"subclass me if necessary"
		pass

	def install_results(self, var, subdir, task, chmod=0644):
		debug('install results called', 'object')
		current = Params.g_build.m_curdirnode
		lst=map(lambda a: a.relpath_gen(current), task.m_outputs)
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

	def find_sources_in_dirs(self, dirnames, excludes=[]):
		"subclass if necessary"
		lst=[]
		excludes = self.to_list(excludes)
		#make sure dirnames is a list helps with dirnames with spaces
		dirnames = self.to_list(dirnames)

		ext_lst = []
		ext_lst += self.s_default_ext
		try:
			for var in self.__class__.__dict__['all_hooks']:
				ext_lst += self.env[var]
		except KeyError:
			pass

		for name in dirnames:
			#print "name is ", name
			anode = self.path.ensure_node_from_lst(Utils.split_path(name))
			#print "anode ", anode.m_name, " ", anode.files()
			Params.g_build.rescan(anode)
			#print "anode ", anode.m_name, " ", anode.files()

			for file in anode.files():
				#print "file found ->", file
				(base, ext) = os.path.splitext(file.m_name)
				if ext in ext_lst:
					s = file.relpath(self.path)
					if not s in lst:
						if s in excludes: continue
						lst.append(s)

		lst.sort()
		self.source = self.source+' '+(" ".join(lst))

g_cache_max={}
def sign_env_vars(env, vars_list):
	" ['CXX', ..] -> [env['CXX'], ..]"

	# ccroot objects use the same environment for building the .o at once
	# the same environment and the same variables are used
	s = str([env.m_idx]+vars_list)
	try: return g_cache_max[s]
	except KeyError: pass

	lst = map(lambda a: env.get_flat(a), vars_list)
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

