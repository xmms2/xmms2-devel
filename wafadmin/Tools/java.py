#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Java support"

import os
import Object, Action

class javaobj(Object.genobj):
	s_default_ext = ['.java']
	def __init__(self, type='all', library=0):
		Object.genobj.__init__(self, 'other')

		self.m_type   = type
		self.m_source = ''
		self.m_target = ''

	def apply(self):
		source_lst = self.source.split()
		nodes_lst = []

		self.env['CLASSPATH'] = '..:.'

		# first create the nodes corresponding to the sources
		for filename in source_lst:
			base, ext = os.path.splitext(filename)
			node = self.find(filename)
			if not ext in self.s_default_ext:
				print "??? ", filename

			task = self.create_task('javac', self.env, 1)
			task.set_inputs(node)
			task.set_outputs(node.change_ext('.class'))

def setup(env):
	Object.register('java', javaobj)
	Action.simple_action('javac', '${JAVAC} -classpath ${CLASSPATH} -d ${TGT[0].cd_to(env)} ${SRC}', color='BLUE')

def detect(conf):
	javac = conf.find_program('javac', var='JAVAC')
	if not javac: return 0

	conf.env['JAVA_EXT'] = ['.java']
	return 1

