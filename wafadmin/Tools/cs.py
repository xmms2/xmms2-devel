#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"C# support"

import Params, Action, Object, Utils
from Params import error

g_types_lst = ['program', 'library']
class csobj(Object.genobj):
	def __init__(self, type='program'):
		Object.genobj.__init__(self, 'other')

		self.m_type       = type

		self.source       = ''
		self.target       = ''

		self.flags        = ''
		self.assemblies   = ''
		self.resources    = ''

		self.uselib       = ''

		self._flag_vars = ['FLAGS', 'ASSEMBLIES']

		if not self.env: self.env = Params.g_build.m_allenvs['default']

		if not type in g_types_lst:
			error('type for csobj is undefined '+type)
			type='program'

	def apply(self):
		self.apply_uselib()

		# process the flags for the assemblies
		assemblies_flags = []
		for i in self.to_list(self.assemblies) + self.env['ASSEMBLIES']:
			assemblies_flags += '/r:'+i
		self.env['_ASSEMBLIES'] += assemblies_flags

		# process the flags for the resources
		for i in self.to_list(self.resources):
			self.env['_RESOURCES'].append('/resource:'+i)

		# additional flags
		self.env['_FLAGS'] += self.to_list(self.flags) + self.env['FLAGS']

		curnode = self.m_current_path

		# process the sources
		nodes = []
		for i in self.to_list(self.source):
			nodes.append(curnode.find_node(Utils.split_path(i)))

		# create the task
		task = self.create_task('mcs', self.env, 101)
		task.m_inputs  = nodes
		task.set_outputs(self.find(self.target))

	def apply_uselib(self):
		if not self.uselib:
			return
		for var in self.to_list(self.uselib):
			for v in self._flag_vars:
				val=''
				try:    val = self.env[v+'_'+var]
				except: pass
				if val: self.env.appendValue(v, val)

def setup(env):
	Object.register('cs', csobj)
	Action.simple_action('mcs', '${MCS} ${SRC} /out:${TGT} ${_FLAGS} ${_ASSEMBLIES} ${_RESOURCES}', color='YELLOW')

def detect(conf):
	mcs = conf.find_program('mcs', var='MCS')
	if not mcs: mcs = conf.find_program('gmcs', var='MCS')
	return 1

