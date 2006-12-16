#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"LaTeX/PDFLaTeX support"

import os, re
import Utils, Params, Action, Object, Runner, Scan
from Params import error, warning, debug, fatal

tex_regexp = re.compile('^\\\\include{(.*)}', re.M)
#tex_regexp = re.compile('^[^%]*\\\\bringin{(.*)}', re.M)
class tex_scanner(Scan.scanner):
	def __init__(self):
		Scan.scanner.__init__(self)
	def scan(self, node, env, curdirnode):
		variant = node.variant(env)
		fi = open(node.abspath(env), 'r')
		content = fi.read()
		fi.close()

		found = tex_regexp.findall(content)

		nodes = []
		names = []
		if not node: return (nodes, names)

		abs = curdirnode.abspath()
		for path in found:
			#print 'boo', name

			filepath = os.path.join(abs, path)

			ok = 0
			try:
				os.stat(filepath)
				ok = filepath
			except:
				pass

			if not ok:
				for k in ['.tex', '.ltx']:
					try:
						debug("trying %s%s" % (filepath, k), 'tex')
						os.stat(filepath+k)
						ok = filepath+k
						path = path+k
					except:
						pass

			if ok:
				node = curdirnode.find_node(
					Utils.split_path(path))
				nodes.append(node)
			else:
				print 'could not find', filepath
				names.append(path)

		debug("found the following : %s and names %s" % (nodes, names), 'tex')
		return (nodes, names)

g_tex_scanner = tex_scanner()


g_bibtex_re = re.compile('bibdata', re.M)
def tex_build(task, command='LATEX'):
	env = task.m_env

	if env['PROMPT_LATEX']:
		exec_cmd = Runner.exec_command_interact
		com = '%s %s' % (env[command], env[command+'FLAGS'])
	else:
		exec_cmd = Runner.exec_command
		com = '%s %s %s' % (env[command], env[command+'FLAGS'], '-interaction=batchmode')


	node = task.m_inputs[0]
	reldir  = node.cd_to(env)


	srcfile = node.srcpath(env)

	lst = []
	for c in Utils.split_path(reldir):
		if c: lst.append('..')
	sr = Utils.join_path_list(lst + [srcfile])
	sr2 = Utils.join_path_list(lst + [node.m_parent.srcpath(env)])

	aux_node = node.change_ext('.aux')
	idx_node = node.change_ext('.idx')

	hash     = ''
	old_hash = ''




	nm = aux_node.m_name
	docuname = nm[ : len(nm) - 4 ] # 4 is the size of ".aux"

	latex_compile_cmd = 'cd %s && TEXINPUTS=%s:$TEXINPUTS %s %s' % (reldir, sr2, com, sr)
	warning('first pass on %s' % command)
	ret = exec_cmd(latex_compile_cmd)
	if ret: return ret

	# look in the .aux file if there is a bibfile to process
	try:
		file = open(aux_node.abspath(env), 'r')

		ct = file.read()
		file.close()
		#print '---------------------------', ct, '---------------------------'

		fo = g_bibtex_re.findall(ct)

		# yes, there is a .aux file to process
		if fo:
			bibtex_compile_cmd = 'cd %s && BIBINPUTS=%s:$BIBINPUTS %s %s' % (reldir, sr2, env['BIBTEX'], docuname)

			warning('calling bibtex')
			ret = exec_cmd(bibtex_compile_cmd)
			if ret:
				error('error when calling bibtex %s' % bibtex_compile_cmd)
				return ret

	except:
		error('erreur bibtex scan')
		pass

	# look on the filesystem if there is a .idx file to process
	try:
		idx_path = idx_node.abspath(env)
		os.stat(idx_path)

		makeindex_compile_cmd = 'cd %s && %s %s' % (reldir, env['MAKEINDEX'], idx_path)
		warning('calling makeindex')
		ret = exec_cmd(makeindex_compile_cmd)
		if ret:
			error('error when calling makeindex %s' % makeindex_compile_cmd)
			return ret
	except:
		error('erreur file.idx scan')
		pass

	i = 0
	while i < 10:
		# prevent against infinite loops - one never knows
		i += 1

		# watch the contents of file.aux
		old_hash = hash
		try:
			hash = Params.h_md5_file(aux_node.abspath(env))
		except:
			error('could not read aux.h -> %s' % aux_node.abspath(env))
			pass

		# debug
		#print "hash is, ", hash, " ", old_hash

		# stop if file.aux does not change anymore
		if hash and hash == old_hash: break

		# run the command
		warning('calling %s' % command)
		ret = exec_cmd(latex_compile_cmd)
		if ret:
			error('error when calling %s %s' % (command, latex_compile_cmd))
			return ret

	# 0 means no error
	return 0

latex_vardeps  = ['LATEX', 'LATEXFLAGS']
def latex_build(task):
	return tex_build(task, 'LATEX')

pdflatex_vardeps  = ['PDFLATEX', 'PDFLATEXFLAGS']
def pdflatex_build(task):
	return tex_build(task, 'PDFLATEX')

g_texobjs = ['latex','pdflatex']
class texobj(Object.genobj):
	s_default_ext = ['.tex', '.ltx']
	def __init__(self, type='latex'):
		Object.genobj.__init__(self, 'other')

		global g_texobjs
		if not type in g_texobjs:
			Params.niceprint('type %s not supported for texobj' % type, 'ERROR', 'texobj')
			import sys
			sys.exit(1)
		self.m_type   = type
		self.outs     = '' # example: "ps pdf"
		self.prompt   = 1  # prompt for incomplete files (else the batchmode is used)
		self.deps     = ''
	def apply(self):

		tree = Params.g_build
		outs = self.outs.split()
		self.env['PROMPT_LATEX'] = self.prompt

		deps_lst = []

		if self.deps:
			deps = self.to_list(self.deps)
			for filename in deps:
				n = self.m_current_path.find_node( 
					Utils.split_path(filename) )
				if not n in deps_lst: deps_lst.append(n)

		for filename in self.source.split():
			base, ext = os.path.splitext(filename)
			if not ext in self.s_default_ext: continue

			node = self.m_current_path.find_node( 
				Utils.split_path(filename) )
			if not node: fatal('cannot find %s' % filename)

			if self.m_type == 'latex':
				task = self.create_task('latex', self.env, 20)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.dvi'))
			elif self.m_type == 'pdflatex':
				task = self.create_task('pdflatex', self.env, 20)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.pdf'))
			else:
				fatal('no type or invalid type given in tex object (should be latex or pdflatex)')

			task.m_scanner = g_tex_scanner
			task.m_env     = self.env
			task.m_scanner_params = {'curdirnode':self.m_current_path}

			# add the manual dependencies
			if deps_lst:
				variant = node.variant(self.env)
				outnode = task.m_outputs[0]
				try:
					lst = tree.m_depends_on[variant][node]
					for n in deps_lst:
						if not n in lst:
							lst.append(n)
				except:
					tree.m_depends_on[variant][node] = deps_lst

			if self.m_type == 'latex':
				if 'ps' in outs:
					pstask = self.create_task('dvips', self.env, 40)
					pstask.set_inputs(task.m_outputs)
					pstask.set_outputs(node.change_ext('.ps'))
				if 'pdf' in outs:
					pdftask = self.create_task('dvipdf', self.env, 40)
					pdftask.set_inputs(task.m_outputs)
					pdftask.set_outputs(node.change_ext('.pdf'))
			elif self.m_type == 'pdflatex':
				if 'ps' in outs:
					pstask = self.create_task('pdf2ps', self.env, 40)
					pstask.set_inputs(task.m_outputs)
					pstask.set_outputs(node.change_ext('.ps'))

def detect(conf):
	v = conf.env

	for p in 'tex latex pdflatex bibtex dvips dvipdf ps2pdf makeindex'.split():
		conf.find_program(p, var=p.upper())
		v[p.upper()+'FLAGS'] = ''
	v['DVIPSFLAGS'] = '-Ppdf'
	return 1

def setup(env):
	Action.simple_action('tex', '${TEX} ${TEXFLAGS} ${SRC}', color='BLUE')
	Action.simple_action('bibtex', '${BIBTEX} ${BIBTEXFLAGS} ${SRC}', color='BLUE')
	Action.simple_action('dvips', '${DVIPS} ${DVIPSFLAGS} ${SRC} -o ${TGT}', color='BLUE')
	Action.simple_action('dvipdf', '${DVIPDF} ${DVIPDFFLAGS} ${SRC} ${TGT}', color='BLUE')
	Action.simple_action('pdf2ps', '${PDF2PS} ${PDF2PSFLAGS} ${SRC} ${TGT}', color='BLUE')

	Action.Action('latex', vars=latex_vardeps, func=latex_build)
	Action.Action('pdflatex', vars=pdflatex_vardeps, func=pdflatex_build)

	Object.register('tex', texobj)


