#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Atomic operations that create nodes or execute commands"

import os, types, shutil
try: from hashlib import md5
except ImportError: from md5 import md5

import Params, Scan, Action, Runner, Object
from Params import debug, error, warning

g_tasks_done    = []
"tasks that have been run, this is used in tests to check which tasks were actually launched"

g_default_param = {'path_lst':[]}
"the default scanner parameter"

class TaskManager:
	"""There is a single instance of TaskManager held by Task.py:g_tasks
	The manager holds a list of TaskGroup
	Each TaskGroup contains a map(priority, list of tasks)"""
	def __init__(self):
		self.groups = []
		self.idx    = 0
	def add_group(self, name=''):
		if not name:
			try: size = len(self.groups)
			except: size = 0
			name = 'group-%d' % size
		if not self.groups:
			self.groups = [TaskGroup(name)]
			return
		if not self.groups[0].prio:
			warning('add_group: an empty group is already present')
			return
		self.groups = self.groups + [TaskGroup(name)]
	def add_task(self, task, prio):
		if not self.groups: self.add_group('group-0')
		task.m_idx = self.idx
		self.idx += 1
		self.groups[-1].add_task(task, prio)
	def total(self):
		total = 0
		if not self.groups: return 0
		for group in self.groups:
			for p in group.prio:
				total += len(group.prio[p])
		return total
	def debug(self):
		for i in self.groups:
			print "-----group-------", i.name
			for j in i.prio:
				print "prio: ", j, str(i.prio[j])

"the container of all tasks (instance of TaskManager)"
g_tasks = TaskManager()

class TaskGroup:
	"A TaskGroup maps priorities (integers) to lists of tasks"
	def __init__(self, name):
		self.name = name
		self.info = ''
		self.prio = {} # map priority numbers to tasks
	def add_task(self, task, prio):
		try: self.prio[prio].append(task)
		except: self.prio[prio] = [task]

class TaskBase:
	"TaskBase is the base class for task objects"
	def __init__(self, priority, normal=1):
		self.m_display = ''
		self.m_hasrun=0
		global g_tasks
		if normal:
			# add to the list of tasks
			g_tasks.add_task(self, priority)
		else:
			self.m_idx = g_tasks.idx
			g_tasks.idx += 1
	def may_start(self):
		"return non-zero if the task may is ready"
		return 1
	def must_run(self):
		"return 0 if the task does not need to run"
		return 1
	def prepare(self):
		"prepare the task for further processing"
		pass
	def update_stat(self):
		"update the dependency tree (node stats)"
		pass
	def debug_info(self):
		"return debug info"
		return ''
	def debug(self):
		"prints the debug info"
		pass
	def run(self):
		"process the task"
		pass
	def color(self):
		"return the color to use for the console messages"
		return 'BLUE'
	def set_display(self, v):
		self.m_display = v
	def get_display(self):
		return self.m_display

class Task(TaskBase):
	"Task is the more common task. It has input nodes and output nodes"
	def __init__(self, action_name, env, priority=5, normal=1):
		TaskBase.__init__(self, priority, normal)

		# name of the action associated to this task type
		self.m_action = Action.g_actions[action_name]

		# environment in use
		self.m_env = env

		# inputs and outputs are nodes
		# use setters when possible
		self.m_inputs  = []
		self.m_outputs = []

		# scanner function
		self.m_scanner        = Scan.g_default_scanner

		# TODO get rid of this:
		# default scanner parameter
		global g_default_param
		self.m_scanner_params = g_default_param

		# additionally, you may define the following
		# self.dep_vars = 'some_env_var'


	def set_inputs(self, inp):
		if type(inp) is types.ListType: self.m_inputs = inp
		else: self.m_inputs = [inp]

	def set_outputs(self, out):
		if type(out) is types.ListType: self.m_outputs = out
		else: self.m_outputs = [out]

	def set_run_after(self, task):
		"set (scheduler) dependency on another task"
		# TODO: handle list or object
		assert isinstance(task, TaskBase)
		try: self.m_run_after.append(task)
		except AttributeError: self.m_run_after = [task]

	def get_run_after(self):
		try: return self.m_run_after
		except AttributeError: return []

	def add_file_dependency(self, filename):
		"TODO user-provided file dependencies"
		node = Params.g_build.m_current.find_build(filename)
		try: self.m_deps_nodes.append(node)
		except: self.m_deps_nodes = [node]

	#------------ users are probably less interested in the following methods --------------#

	def signature(self):
		# compute the result one time, and suppose the scanner.get_signature will give the good result
		try: return self._sign_all
		except AttributeError: pass

		tree = Params.g_build

		m = md5()

		dep_sig = Params.sig_nil
		scan = None
		try:
			scan = self.m_scanner
		except AttributeError: # there is no scanner for the task
			for x in self.m_inputs:
				variant = x.variant(self.m_env)
				v = tree.m_tstamp_variants[variant][x]
				dep_sig = hash( (dep_sig, v) )
				m.update(v)
		if scan:
			dep_sig = scan.get_signature(self)
			m.update(dep_sig)

		act_sig = None
		try: act_sig = self.m_action.signature(self)
		except AttributeError: act_sig = Object.sign_env_vars(self.m_env, self.m_action.m_vars)
		m.update(act_sig)

		var_sig = None
		try:
			var_sig = Object.sign_env_vars(self.m_env, self.dep_vars)
			m.update(var_sig)
		except AttributeError:
			pass

		node_sig = Params.sig_nil
		try:
			for x in self.dep_nodes:
				variant = x.variant(self.m_env)
				v = tree.m_tstamp_variants[variant][x]
				node_sig = hash( (node_sig, v) )
				m.update(v)
		except AttributeError:
			pass

		# hash additional node dependencies
		ret = m.digest()

		self.cache_sig = [ret, dep_sig, act_sig, var_sig, node_sig]

		self._sign_all = ret
		return ret

	def may_start(self):
		"wait for other tasks to complete"
		if (not self.m_inputs) or (not self.m_outputs):
			if not (not self.m_inputs) and (not self.m_outputs):
				error("potentially grave error, task is invalid : no inputs or outputs")
				self.debug()

		# the scanner has its word to say
		try:
			if not self.m_scanner.may_start(self):
				return 1
		except AttributeError:
			pass

		# this is a dependency using the scheduler, as opposed to hash-based ones
		for t in self.get_run_after():
			if not t.m_hasrun:
				return 0
		return 1

	def must_run(self):
		"see if the task must be run or not"
		#return 0 # benchmarking

		tree = Params.g_build
		ret = 0

		# for tasks that have no inputs or outputs and are run all the time
		if not self.m_inputs and not self.m_outputs:
			self.m_dep_sig = Params.sig_nil
			return 1

		# look at the previous signature first
		try:
			node = self.m_outputs[0]
			variant = node.variant(self.m_env)
			time = tree.m_tstamp_variants[variant][node]
			key = hash( (variant, node, time, self.m_scanner.__class__.__name__) )
			prev_sig = tree.m_sig_cache[key][0]
		except KeyError:
			# an exception here means the object files do not exist
			debug("task #%d should run as the first node does not exist" % self.m_idx, 'task')

			# maybe we can just retrieve the object files from the cache then
			ret = self.can_retrieve_cache(self.signature())
			return not ret

		new_sig = self.signature()

		# debug if asked to
		if Params.g_zones:
			self.debug_why(tree.m_sig_cache[key])


		if new_sig != prev_sig:
			# if the node has not changed, try to use the cache
			ret = self.can_retrieve_cache(new_sig)
			return not ret

		return 0

	def update_stat(self):
		"this is called after a sucessful task run"
		tree = Params.g_build
		env  = self.m_env
		sig = self.signature()

		cnt = 0
		for node in self.m_outputs:
			variant = node.variant(env)
			#if node in tree.m_tstamp_variants[variant]:
			#	print "variant is ", variant
			#	print "self sig is ", Params.vsig(tree.m_tstamp_variants[variant][node])

			# check if the node exists ..
			try:
				os.stat(node.abspath(env))
			except:
				error('a node was not produced for task %s %s' % (str(self.m_idx), node.abspath(env)))
				raise

			# important, store the signature for the next run
			tree.m_tstamp_variants[variant][node] = sig

			# We could re-create the signature of the task with the signature of the outputs
			# in practice, this means hashing the output files
			# this is unnecessary

			if Params.g_usecache:
				ssig = sig.encode('hex')
				dest = os.path.join(Params.g_usecache, ssig+'-'+str(cnt))
				try: shutil.copy2(node.abspath(env), dest)
				except IOError: warning('could not write the file to the cache')
				cnt += 1

		# keep the signatures in the first node
		node = self.m_outputs[0]
		variant = node.variant(self.m_env)
		time = tree.m_tstamp_variants[variant][node]
		key = hash( (variant, node, time, self.m_scanner.__class__.__name__) )
		val = self.cache_sig
		tree.set_sig_cache(key, val)

		self.m_executed=1

	def can_retrieve_cache(self, sig):
		"""Retrieve build nodes from the cache - the file time stamps are updated
		for cleaning the least used files from the cache dir - be careful when overriding"""
		if not Params.g_usecache: return None
		if Params.g_options.nocache: return None

		env  = self.m_env
		sig = self.signature()

		try:
			cnt = 0
			for node in self.m_outputs:
				variant = node.variant(env)

				ssig = sig.encode('hex')
				orig = os.path.join(Params.g_usecache, ssig+'-'+str(cnt))
				shutil.copy2(orig, node.abspath(env))

				# touch the file
				# what i would like to do is to limit the max size of the cache, using either
				# df (file system full) or a fixed size (like say no more than 400Mb of cache)
				# removing the files would be done by order of timestamps (TODO ITA)
				os.utime(orig, None)
				cnt += 1

				Params.g_build.m_tstamp_variants[variant][node] = sig
				if not Runner.g_quiet: Params.pprint('GREEN', 'restored from cache %s' % node.bldpath(env))
		except:
			debug("failed retrieving file", 'task')
			return None
		return 1

	def prepare(self):
		try: self.m_action.prepare(self)
		except AttributeError: pass

	def run(self):
		return self.m_action.run(self)

	def get_display(self):
		if self.m_display: return self.m_display
		self.m_display=self.m_action.get_str(self)
		return self.m_display

	def color(self):
		return self.m_action.m_color

	def debug_info(self):
		ret = []
		ret.append('-- task details begin --')
		ret.append('action: %s' % str(self.m_action))
		ret.append('idx:    %s' % str(self.m_idx))
		ret.append('source: %s' % str(self.m_inputs))
		ret.append('target: %s' % str(self.m_outputs))
		ret.append('-- task details end --')
		return '\n'.join(ret)

	def debug(self, level=0):
		fun=Params.debug
		if level>0: fun=Params.error
		fun(self.debug_info())

	def debug_why(self, old_sigs):
		"explains why a task is run"

		new_sigs = self.cache_sig
		v = Params.vsig

		debug("Task %s must run: %s" % (self.m_idx, old_sigs[0] != new_sigs[0]), 'task')
		if (new_sigs[1] != old_sigs[1]):
			debug(' -> A source file (or a dependency) has changed %s %s' % (v(old_sigs[1]), v(new_sigs[1])), 'task')
		if (new_sigs[2] != old_sigs[2]):
			debug(' -> An environment variable has changed %s %s' % (v(old_sigs[2]), v(new_sigs[2])), 'task')
		if (new_sigs[3] != old_sigs[3]):
			debug(' -> A manual dependency has changed %s %s' % (v(old_sigs[3]), v(new_sigs[3])), 'task')
		if (new_sigs[4] != old_sigs[4]):
			debug(' -> A user-given environment variable has changed %s %s' % (v(old_sigs[4]), v(new_sigs[4])), 'task')

class TaskCmd(TaskBase):
	"TaskCmd executes commands. Instances always execute their function."
	def __init__(self, fun, env, priority):
		TaskBase.__init__(self, priority)
		self.fun = fun
		self.env = env
	def prepare(self):
		self.display = "* executing: "+self.fun.__name__
	def debug_info(self):
		return 'TaskCmd:fun %s' % self.fun.__name__
	def debug(self):
		return 'TaskCmd:fun %s' % self.fun.__name__
	def run(self):
		self.fun(self)

