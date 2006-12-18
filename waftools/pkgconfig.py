""" tool to generate .pc files for pkgconfig """

import Common
import Object
import Utils
import Params
import Node
import os
from misc import copyobj, subst_func

class pkgcobj(copyobj):
	def __init__(self, type='none'):
		copyobj.__init__(self, type)
		self.fun = subst_func
		self.libs = []

	def apply(self):
		for name, lib in self.libs:
			val = {}
			p = self.env["PREFIX"]
			val["PREFIX"] = p
			val["BINDIR"] = os.path.join("${prefix}", "bin")
			val["LIBDIR"] = os.path.join("${prefix}", "lib")
			val["INCLUDEDIR"] = os.path.join("${prefix}", "include", "xmms2")
			val["VERSION"] = self.version

			val["NAME"] = name
			val["LIB"] = lib

			node = self.m_current_path.find_node(Utils.split_path('xmms2.pc.in'))
			newnode = self.m_current_path.search_existing_node(Utils.split_path(name+'.pc'))
			if not newnode:
				newnode = Node.Node(name+'.pc', self.m_current_path)
				self.m_current_path.append_build(newnode)

			task = self.create_task('copy', self.env, 8)
			task.set_inputs(node)
			task.set_outputs(newnode)
			task.m_env = self.env
			task.fun = self.fun
			task.dict = val

			if Params.g_commands['install'] or Params.g_commands['uninstall']:
				Common.install_files('PKGCONFIGDIR', '', newnode.abspath(self.env))

def setup(env):
	Object.register('pkgc', pkgcobj)

def detect(conf):
	return 1

