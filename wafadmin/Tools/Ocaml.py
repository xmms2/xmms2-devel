#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Ocaml support"

import os
import Params, Action, Object, Scan, Utils
from Params import error, fatal

g_map_id_to_obj = {}

class ocaml_scanner(Scan.scanner):
	def __init__(self):
		Scan.scanner.__init__(self)
	def may_start(self, task):
		global g_map_id_to_obj
		if task.m_idx in g_map_id_to_obj: g_map_id_to_obj[task.m_idx].comptask()
		return 1

g_caml_scanner = ocaml_scanner()

native_lst=['native', 'all', 'c_object']
bytecode_lst=['bytecode', 'all']
class ocamlobj(Object.genobj):
	s_default_ext = ['.mli', '.mll', '.mly', '.ml']
	def __init__(self, type='all', library=0):
		Object.genobj.__init__(self, 'other')

		self.m_type       = type
		self.m_source     = ''
		self.m_target     = ''
		self.islibrary    = library
		self._incpaths_lst = []
		self._bld_incpaths_lst = []
		self._mlltasks    = []
		self._mlytasks    = []

		self._mlitasks    = []
		self._native_tasks   = []
		self._bytecode_tasks = []
		self._linktasks      = []

		self.bytecode_env = None
		self.native_env   = None

		self.includes     = ''
		self.uselib       = ''

		self.out_nodes    = []

		self.are_deps_set = 0

		if not self.env: self.env = Params.g_build.m_allenvs['default']

		if not type in ['bytecode','native','all','c_object']:
			print 'type for camlobj is undefined '+type
			type='all'

		if type in native_lst:
			self.native_env                = self.env.copy()
			self.native_env['OCAMLCOMP']   = self.native_env['OCAMLOPT']
			self.native_env['OCALINK']     = self.native_env['OCAMLOPT']
		if type in bytecode_lst:
			self.bytecode_env              = self.env.copy()
			self.bytecode_env['OCAMLCOMP'] = self.bytecode_env['OCAMLC']
			self.bytecode_env['OCALINK']   = self.bytecode_env['OCAMLC']

		if self.islibrary:
			self.bytecode_env['OCALINKFLAGS'] = '-a'
			self.native_env['OCALINKFLAGS']   = '-a'

		if self.m_type == 'c_object':
			self.native_env['OCALINK'] = self.native_env['OCALINK']+' -output-obj'

	def lastlinktask(self):
		return self._linktasks[0]

	def _map_task(self, task):
		global g_caml_scanner
		task.m_scanner = g_caml_scanner
		global g_map_id_to_obj
		g_map_id_to_obj[task.m_idx] = self

	def apply_incpaths(self):
		inc_lst = self.includes.split()
		lst = self._incpaths_lst
		tree = Params.g_build
		for dir in inc_lst:
			node = self.path.find_source(dir)
			if not node:
				error("node not found dammit")
				continue
			Params.g_build.rescan(node)
			if not node in lst: lst.append( node )
			self._bld_incpaths_lst.append(node)
		# now the nodes are added to self._incpaths_lst

	def apply(self):
		self.apply_incpaths()

		for i in self._incpaths_lst:
			if self.bytecode_env:
				self.bytecode_env.append_value('OCAMLPATH', '-I %s' % i.srcpath(self.env))
				self.bytecode_env.append_value('OCAMLPATH', '-I %s' % i.bldpath(self.env))

			if self.native_env:
				self.native_env.append_value('OCAMLPATH', '-I %s' % i.bldpath(self.env))
				self.native_env.append_value('OCAMLPATH', '-I %s' % i.srcpath(self.env))

		varnames = ['INCLUDES', 'OCAMLFLAGS', 'OCALINKFLAGS', 'OCALINKFLAGS_OPT']
		for name in self.uselib.split():
			for vname in varnames:
				cnt = self.env[vname+'_'+name]
				if cnt:
					if self.bytecode_env: self.bytecode_env.append_value(vname, cnt)
					if self.native_env: self.native_env.append_value(vname, cnt)

		source_lst = self.to_list(self.source)
		nodes_lst = []

		# first create the nodes corresponding to the sources
		for filename in source_lst:
			base, ext = os.path.splitext(filename)
			node = self.path.find_build(filename)
			#if not ext in self.s_default_ext:
			#	print "??? ", filename

			if ext == '.mll':
				mll_task = self.create_task('ocamllex', self.native_env, 10)
				mll_task.set_inputs(node)
				mll_task.set_outputs(node.change_ext('.ml'))
				self._mlltasks.append(mll_task)

				node = mll_task.m_outputs[0]

			elif ext == '.mly':
				mly_task = self.create_task('ocamlyacc', self.native_env, 10)
				mly_task.set_inputs(node)
				mly_task.set_outputs([node.change_ext('.ml'), node.change_ext('.mli')])
				self._mlytasks.append(mly_task)

				task = self.create_task('ocamlcmi', self.native_env, 40)
				task.set_inputs(mly_task.m_outputs[1])
				task.set_outputs(mly_task.m_outputs[1].change_ext('.cmi'))

				node = mly_task.m_outputs[0]
			elif ext == '.mli':
				task = self.create_task('ocamlcmi', self.native_env, 40)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.cmi'))
				self._mlitasks.append(task)
				continue
			elif ext == '.c':
				task = self.create_task('ocamlcc', self.native_env, 60)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.o'))

				self.out_nodes += task.m_outputs
				continue
			else:
				pass

			if self.native_env:
				task = self.create_task('ocaml', self.native_env, 60)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.cmx'))
				self._map_task(task)
				self._native_tasks.append(task)
			if self.bytecode_env:
				task = self.create_task('ocaml', self.bytecode_env, 60)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.cmo'))
				self._map_task(task)
				self._bytecode_tasks.append(task)

		if self.bytecode_env:
			linktask = self.create_task('ocalink', self.bytecode_env, 101)
			objfiles = []
			for t in self._bytecode_tasks: objfiles.append(t.m_outputs[0])
			linktask.m_inputs  = objfiles
			linktask.set_outputs(self.path.find_build(self.get_target_name(bytecode=1)))
			self._linktasks.append(linktask)
		if self.native_env:
			linktask = self.create_task('ocalinkopt', self.native_env, 101)
			objfiles = []
			for t in self._native_tasks: objfiles.append(t.m_outputs[0])
			linktask.m_inputs  = objfiles
			linktask.set_outputs(self.path.find_build(self.get_target_name(bytecode=0)))
			#linktask.set_outputs(objfiles[0].m_parent.find_build(self.get_target_name(bytecode=0)))
			self._linktasks.append(linktask)

			#if self.m_type != 'c_object': self.out_nodes += linktask.m_outputs
			self.out_nodes += linktask.m_outputs

	def get_target_name(self, bytecode):
		if bytecode:
			if self.islibrary:
				return self.target+'.cma'
			else:
				return self.target+'.run'
		else:
			if self.m_type == 'c_object': return self.target+'.o'

			if self.islibrary:
				return self.target+'.cmxa'
			else:
				return self.target

	def comptask(self):
		"""
		use ocamldep to set the dependencies

		we cannot run this method when posting the object as the mly and mll tasks
		are not run yet, so the resulting .ml and .mli files do not exist, leading to
		incomplete dependencies
		"""

		if self.are_deps_set: return
		self.are_deps_set = 1

		#print "comptask called!"

		curdir = self.path
		file2task = {}

		dirs  = []
		milst = []
		lst = []
		for i in self._mlitasks + self._native_tasks + self._bytecode_tasks:
			node = i.m_inputs[0]
			path = node.bldpath(self.env)
			if not path in milst:
				milst.append(path)
				dir = node.m_parent.srcpath(self.env)
				if not dir in dirs: dirs.append(dir)

			m = i.m_outputs[0]
			file2task[m.relpath_gen(Params.g_build.m_bldnode)] = i

		cmd = ['ocamldep']
		for i in dirs:
			cmd.append('-I')
			cmd.append(i)
		for i in milst:
			cmd.append(i)

		cmd = " ".join(cmd)
		ret = os.popen(cmd).read().strip()

		hashdeps = {}
		lines = ret.split('\n')
		for line in lines:
			lst = line.split(': ')
			hashdeps[lst[0]] = lst[1].split()

			#print "found ", lst[0], file2task

			if lst[0] in file2task:
				t = file2task[lst[0]]

				for name in lst[1].split():
					if name in file2task:
						t.set_run_after(file2task[name])

		# sort the files in the link tasks .. damn
		def cmp(n1, n2):
			v1 = n1.relpath_gen(Params.g_build.m_bldnode)
			v2 = n2.relpath_gen(Params.g_build.m_bldnode)

			if v1 in hashdeps:
				if v2 in hashdeps[v1]:
					return 1
			elif v2 in hashdeps:
				if v1 in hashdeps[v2]:
					return -1
			return 0

		for task in self._linktasks:
			task.m_inputs.sort(cmp)
		#for task in self._native_tasks:
		#	print task.m_run_after


def setup(env):
	Object.register('ocaml', ocamlobj)
	Action.simple_action('ocaml', '${OCAMLCOMP} ${OCAMLPATH} ${OCAMLFLAGS} ${INCLUDES} -c -o ${TGT} ${SRC}', color='GREEN')
	Action.simple_action('ocalink', '${OCALINK} -o ${TGT} ${INCLUDES} ${OCALINKFLAGS} ${SRC}', color='YELLOW')
	Action.simple_action('ocalinkopt', '${OCALINK} -o ${TGT} ${INCLUDES} ${OCALINKFLAGS_OPT} ${SRC}', color='YELLOW')
	Action.simple_action('ocamlcmi', '${OCAMLC} ${OCAMLPATH} ${INCLUDES} -o ${TGT} -c ${SRC}', color='BLUE')
	Action.simple_action('ocamlcc', 'cd ${TGT[0].bld_dir(env)} && ${OCAMLOPT} ${OCAMLFLAGS} ${OCAMLPATH} ${INCLUDES} -c ${SRC[0].abspath(env)}', color='GREEN')
	Action.simple_action('ocamllex', '${OCAMLLEX} ${SRC} -o ${TGT}', color='BLUE')
	Action.simple_action('ocamlyacc', '${OCAMLYACC} -b ${TGT[0].bldbase(env)} ${SRC}', color='BLUE')

def detect(conf):
	opt = conf.find_program('ocamlopt', var='OCAMLOPT')
	occ = conf.find_program('ocamlc', var='OCAMLC')
	if (not opt) or (not occ):
		fatal('The objective caml compiler was not found:\n' \
			'install it or make it availaible in your PATH')

	lex  = conf.find_program('ocamllex', var='OCAMLLEX')
	yacc = conf.find_program('ocamlyacc', var='OCAMLYACC')

	conf.env['OCAMLC']       = occ
	conf.env['OCAMLOPT']     = opt
	conf.env['OCAMLLEX']     = lex
	conf.env['OCAMLYACC']    = yacc
	conf.env['OCAMLFLAGS']   = ''
	conf.env['OCALINK']      = ''
	conf.env['OCAMLLIB']     = os.popen(conf.env['OCAMLC']+' -where').read().strip()+os.sep
	conf.env['LIBPATH_OCAML'] = os.popen(conf.env['OCAMLC']+' -where').read().strip()+os.sep
	conf.env['CPPPATH_OCAML'] = os.popen(conf.env['OCAMLC']+' -where').read().strip()+os.sep
	conf.env['LIB_OCAML'] = 'camlrun'
	conf.env['OCALINKFLAGS'] = ''
	return 1

