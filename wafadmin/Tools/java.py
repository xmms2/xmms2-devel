#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Java support"

import os
import Object, Action, Utils

class javaobj(Object.genobj):
	s_default_ext = ['.java']
	def __init__(self):
		Object.genobj.__init__(self, 'other')

		self.jarname = ''
		self.jaropts = ''
		self.classpath = '..:.'

	def apply(self):
		nodes_lst = []

		if self.classpath:
			self.env['CLASSPATH'] = self.classpath

		find_source_lst = self.path.find_source_lst

		# first create the nodes corresponding to the sources
		for filename in self.to_list(self.source):

			node = find_source_lst(Utils.split_path(filename))

			base, ext = os.path.splitext(filename)
			#node = self.path.find_build(filename)
			if not ext in self.s_default_ext:
				fatal("unknown file "+filename)

			task = self.create_task('javac', self.env, 10)
			task.set_inputs(node)
			task.set_outputs(node.change_ext('.class'))

			nodes_lst.append(task.m_outputs[0])

		if self.jarname:
			task = self.create_task('jar_create', self.env, 50)
			task.set_inputs(nodes_lst)
			task.set_outputs(self.path.find_build_lst(Utils.split_path(self.jarname)))

			if not self.env['JAROPTS']:
				if self.jaropts:
					self.env['JAROPTS'] = self.jaropts
				else:
					self.env.append_unique('JAROPTS', '-C %s .' % self.path.bldpath(self.env))

def setup(env):
	Object.register('java', javaobj)
	Action.simple_action('javac', '${JAVAC} -classpath ${CLASSPATH} -d ${TGT[0].bld_dir(env)} ${SRC}', color='BLUE')
	Action.simple_action('jar_create', '${JAR} cvf ${TGT} ${JAROPTS}', color='GREEN')

def detect(conf):
	javac = conf.find_program('javac', var='JAVAC')
	if not javac: return 0

	conf.find_program('jar', var='JAR')

	conf.env['JAVA_EXT'] = ['.java']
	return 1

