#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"""
Batched builds
instead of compiling object files one by one, c/c++ compilers are often able to compile at once:
cc -c ../file1.c ../file2.c ../file3.c

Files are output on the directory where the compiler is called, and dependencies are more difficult
to track (do not run the command on all source files if only one file changes)

As such, we do as if the files were compiled one by one, but no command is actually run:
replace each cc/cpp Task by a TaskSlave
A new task called TaskMaster collects the signatures from each slave and finds out the command-line
to run.

To set this up, the method ccroot::create_task is replaced by a new version, to enable batched builds
it is only necessary to import this module in the configuration (no other change required)
"""

import shutil
import Action, Object, Task, ccroot

class TaskMaster(Task.Task):
	def __init__(self, action_name, env, priority=5, normal=1, master=None):
		Task.Task.__init__(self, action_name, env, priority, normal)
		self.slaves=[]
		self.m_inputs2=[]
		self.m_outputs2=[]

	def add_slave(self, slave):
		self.slaves.append(slave)
		self.set_run_after(slave)

	def may_start(self):
		for t in self.m_run_after:
			if not t.m_hasrun: return 0

		for t in self.slaves:
			self.m_inputs.append(t.m_inputs[0])
			self.m_outputs.append(t.m_outputs[0])
			if t.m_must_run:
				self.m_inputs2.append(t.m_inputs[0])
				self.m_outputs2.append(t.m_outputs[0])
		return 1

	def run(self):
		tmpinputs = self.m_inputs
		self.m_inputs = self.m_inputs2
		tmpoutputs = self.m_outputs
		self.m_outputs = self.m_outputs2

		ret = self.m_action.run(self)

		# unfortunately building the files in batch mode outputs them in the current folder (the build dir)
		# now move the files from the top of the builddir to the correct location
		for i in self.m_outputs:
			name = i.m_name
			if name[-1] == "s": name = name[:-1] # extension for shlib is .os, remove the s
			shutil.move(name, i.bldpath(self.m_env))

		self.m_inputs = tmpinputs
		self.m_outputs = tmpoutputs

		return ret

class TaskSlave(Task.Task):
	def __init__(self, action_name, env, priority=5, normal=1, master=None):
		Task.Task.__init__(self, action_name, env, priority, normal)
		self.m_master = master

	def get_display(self):
		self.m_display=""
		return ""

	def update_stat(self):
		self.m_executed=1

	def must_run(self):
		self.m_must_run = Task.Task.must_run(self)
		return self.m_must_run

	def run(self):
		return 0

	def can_retrieve_cache(self, sig):
		return None

def create_task_new(self, type, env=None, nice=100):
	if type == "cc" or type == "cpp":

		if env is None: env=self.env
		try:
			mm = self.mastertask
		except AttributeError:
			mm = TaskMaster("all_"+type, env, nice)
			self.mastertask = mm

		task = TaskSlave(type, env, nice, master=mm)
		self.m_tasks.append(task)

		mm.add_slave(task)

		if type == self.m_type_initials:
			self.p_compiletasks.append(task)
		return task

	task = Object.genobj.create_task(self, type, env, nice)
	if type == self.m_type_initials:
		self.p_compiletasks.append(task)
	return task

def detect(conf):
	return 1

def setup(env):
	cc_str = '${CC} ${CCFLAGS} ${CPPFLAGS} ${_CCINCFLAGS} ${_CCDEFFLAGS} -c ${SRC}'
	Action.simple_action('all_cc', cc_str, 'GREEN')

	cpp_str = '${CXX} ${CXXFLAGS} ${CPPFLAGS} ${_CXXINCFLAGS} ${_CXXDEFFLAGS} -c ${SRC}'
	Action.simple_action('all_cpp', cpp_str, color='GREEN')

	ccroot.ccroot.create_task = create_task_new

