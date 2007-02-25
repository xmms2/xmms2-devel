#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2006 (ita)

"Gnome support"

import os, re
import Object, Action, Params, Common, Scan, Utils
import cc
from Params import fatal, error

n1_regexp = re.compile('<refentrytitle>(.*)</refentrytitle>', re.M)
n2_regexp = re.compile('<manvolnum>(.*)</manvolnum>', re.M)

class sgml_man_scanner(Scan.scanner):
	def __init__(self):
		Scan.scanner.__init__(self)
	def scan(self, node, env):
		variant = node.variant(env)

		fi = open(node.abspath(env), 'r')
		content = fi.read()
		fi.close()

		names = n1_regexp.findall(content)
		nums = n2_regexp.findall(content)

		name = names[0]
		num  = nums[0]

		doc_name = name+'.'+num

		return ([], [doc_name])

sgml_scanner = sgml_man_scanner()

# intltool
class gnome_intltool(Object.genobj):
	def __init__(self):
		Object.genobj.__init__(self, 'other')
		self.source  = ''
		self.destvar = ''
		self.subdir  = ''
		self.flags   = ''

		self.m_tasks = []

	def apply(self):
		self.env = self.env.copy()
		tree = Params.g_build
		current = tree.m_curdirnode
		for i in self.to_list(self.source):
			node = self.path.find_source(i)

			podirnode = self.path.find_source(self.podir)

			self.env['INTLCACHE'] = Utils.join_path(Params.g_build.m_curdirnode.bldpath(self.env),".intlcache")
			self.env['INTLPODIR'] = podirnode.bldpath(self.env)
			self.env['INTLFLAGS'] = self.flags

			task = self.create_task('intltool', self.env, 2)
			task.set_inputs(node)
			task.set_outputs(node.change_ext(''))

	def install(self):
		current = Params.g_build.m_curdirnode
		for task in self.m_tasks:
			out = task.m_outputs[0]
			Common.install_files(self.destvar, self.subdir, out.abspath(self.env), self.env)

# sgml2man
class gnome_sgml2man(Object.genobj):
	def __init__(self, appname):
		Object.genobj.__init__(self, 'other')
		self.m_tasks=[]
		self.m_appname = appname
	def apply(self):
		tree = Params.g_build
		for node in self.path.files():
			try:
				base, ext = os.path.splitext(node.m_name)
				if ext != '.sgml': continue

				if tree.needs_rescan(node, self.env):
					sgml_scanner.do_scan(node, self.env, hashparams={})

				variant = node.variant(self.env)

				try: tmp_lst = tree.m_raw_deps[variant][node]
				except: tmp_lst = []
				name = tmp_lst[0]

				task = self.create_task('sgml2man', self.env, 2)
				task.set_inputs(node)
				task.set_outputs(self.path.find_build(name))
			except:
				raise
				pass

	def install(self):
		current = Params.g_build.m_curdirnode

		for task in self.m_tasks:
			out = task.m_outputs[0]
			# get the number 1..9
			name = out.m_name
			ext = name[-1]
			# and install the file

			Common.install_files('DATADIR', 'man/man%s/' % ext, out.abspath(self.env), self.env)

# translations
class gnome_translations(Object.genobj):
	def __init__(self, appname):
		Object.genobj.__init__(self, 'other')
		self.m_tasks=[]
		self.m_appname = appname
	def apply(self):
		for file in self.path.files():
			try:
				base, ext = os.path.splitext(file.m_name)
				if ext != '.po': continue

				task = self.create_task('po', self.env, 2)
				task.set_inputs(file)
				task.set_outputs(file.change_ext('.gmo'))
			except: pass
	def install(self):
		destfilename = self.m_appname+'.mo'

		current = Params.g_build.m_curdirnode
		for file in self.path.files():
			lang, ext = os.path.splitext(file.m_name)
			if ext != '.po': continue

			node = self.path.find_source(lang+'.gmo')
			orig = node.relpath_gen(current)

			destfile = Utils.join_path(lang, 'LC_MESSAGES', destfilename)
			Common.install_as('GNOMELOCALEDIR', destfile, orig, self.env)


class gnomeobj(cc.ccobj):
	def __init__(self, type='program'):
		cc.ccobj.__init__(self, type)
		self.m_linktask = None
		self.m_latask   = None
		self.want_libtool = -1 # fake libtool here

		self._dbus_lst    = []
		self._marshal_lst = []

	def add_dbus_file(self, filename, prefix, mode):
		self._dbus_lst.append([filename, prefix, mode])

	def add_marshal_file(self, filename, prefix, mode):
		self._marshal_lst.append([filename, prefix, mode])

	def apply_core(self):
		for i in self._marshal_lst:
			node = self.path.find_source(i[0])

			if not node:
				fatal('file not found on gnome obj '+i[0])

			env = self.env.copy()

			if i[2] == '--header':

				env['GGM_PREFIX'] = i[1]
				env['GGM_MODE']   = i[2]

				task = self.create_task('glib_genmarshal', env, 2)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.h'))

			elif i[2] == '--body':
				env['GGM_PREFIX'] = i[1]
				env['GGM_MODE']   = i[2]

				task = self.create_task('glib_genmarshal', env, 2)
				task.set_inputs(node)
				task.set_outputs(node.change_ext('.c'))

				# this task is really created with self.env
				ctask = self.create_task('cc', self.env)
				ctask.m_inputs = task.m_outputs
				ctask.set_outputs(node.change_ext('.o'))

			else:
				error("unknown type for marshal "+i[2])


		for i in self._dbus_lst:
			node = self.path.find_source(i[0])

			if not node:
				fatal('file not found on gnome obj '+i[0])

			env = self.env.copy()

			env['DBT_PREFIX'] = i[1]
			env['DBT_MODE']   = i[2]

			task = self.create_task('dbus_binding_tool', env, 2)
			task.set_inputs(node)
			task.set_outputs(node.change_ext('.h'))

		# after our targets are created, process the .c files, etc
		cc.ccobj.apply_core(self)

def setup(env):
	Action.simple_action('po', '${POCOM} -o ${TGT} ${SRC}', color='BLUE')
	Action.simple_action('sgml2man', '${SGML2MAN} -o ${TGT[0].bld_dir(env)} ${SRC}  > /dev/null', color='BLUE')
	Action.simple_action( \
		'intltool', \
		'${INTLTOOL} ${INTLFLAGS} -q -u -c ${INTLCACHE} ${INTLPODIR} ${SRC} ${TGT}', \
		color='BLUE')

	Action.simple_action('glib_genmarshal',
		'${GGM} ${SRC} --prefix=${GGM_PREFIX} ${GGM_MODE} > ${TGT}',
		color='BLUE')

	Action.simple_action('dbus_binding_tool',
		'${DBT} --prefix=${DBT_PREFIX} --mode=${DBT_MODE} --output=${TGT} ${SRC}',
		color='BLUE')

	Object.register('gnome_translations', gnome_translations)
	Object.register('gnome_sgml2man', gnome_sgml2man)
	Object.register('gnome_intltool', gnome_intltool)
	Object.register('gnome', gnomeobj)

def detect(conf):

	conf.check_tool('checks')

	pocom = conf.find_program('msgfmt')
	#if not pocom:
	#	fatal('The program msgfmt (gettext) is mandatory!')
	conf.env['POCOM'] = pocom

	sgml2man = conf.find_program('docbook2man')
	#if not sgml2man:
	#	fatal('The program docbook2man is mandatory!')
	conf.env['SGML2MAN'] = sgml2man

	intltool = conf.find_program('intltool-merge')
	#if not intltool:
	#	fatal('The program intltool-merge (intltool, gettext-devel) is mandatory!')
	conf.env['INTLTOOL'] = intltool

	glib_genmarshal = conf.find_program('glib-genmarshal')
	conf.env['GGM'] = glib_genmarshal

	dbus_binding_tool = conf.find_program('dbus-binding-tool')
	conf.env['DBT'] = dbus_binding_tool

	def getstr(varname):
		#if env.has_key('ARGS'): return env['ARGS'].get(varname, '')
		v=''
		try: v = getattr(Params.g_options, varname)
		except: return ''
		return v

	prefix  = conf.env['PREFIX']
	datadir = getstr('datadir')
	libdir  = getstr('libdir')
	if not datadir: datadir = os.path.join(prefix,'share')
	if not libdir:  libdir  = os.path.join(prefix,'lib')

	# addefine also sets the variable to the env
	conf.add_define('GNOMELOCALEDIR', os.path.join(datadir, 'locale'))
	conf.add_define('DATADIR', datadir)
	conf.add_define('LIBDIR', libdir)

	# TODO: maybe the following checks should be in a more generic module.

	#always defined to indicate that i18n is enabled */
	conf.add_define('ENABLE_NLS', '1')

	# TODO
	#Define to 1 if you have the `bind_textdomain_codeset' function.
	conf.add_define('HAVE_BIND_TEXTDOMAIN_CODESET', '1')

	# TODO
	#Define to 1 if you have the `dcgettext' function.
	conf.add_define('HAVE_DCGETTEXT', '1')

	#Define to 1 if you have the <dlfcn.h> header file.
	conf.check_header('dlfcn.h', 'HAVE_DLFCN_H')

	# TODO
	#Define if the GNU gettext() function is already present or preinstalled.
	conf.add_define('HAVE_GETTEXT', '1')

	#Define to 1 if you have the <inttypes.h> header file.
	conf.check_header('inttypes.h', 'HAVE_INTTYPES_H')

	# TODO FIXME
	#Define if your <locale.h> file defines LC_MESSAGES.
	#conf.add_define('HAVE_LC_MESSAGES', '1')

	#Define to 1 if you have the <locale.h> header file.
	conf.check_header('locale.h', 'HAVE_LOCALE_H')

	#Define to 1 if you have the <memory.h> header file.
	conf.check_header('memory.h', 'HAVE_MEMORY_H')

	#Define to 1 if you have the <stdint.h> header file.
	conf.check_header('stdint.h', 'HAVE_STDINT_H')

	#Define to 1 if you have the <stdlib.h> header file.
	conf.check_header('stdlib.h', 'HAVE_STDLIB_H')

	#Define to 1 if you have the <strings.h> header file.
	conf.check_header('strings.h', 'HAVE_STRINGS_H')

	#Define to 1 if you have the <string.h> header file.
	conf.check_header('string.h', 'HAVE_STRING_H')

        #Define to 1 if you have the <sys/stat.h> header file.
	conf.check_header('sys/stat.h', 'HAVE_SYS_STAT_H')

	#Define to 1 if you have the <sys/types.h> header file.
	conf.check_header('sys/types.h', 'HAVE_SYS_TYPES_H')

	#Define to 1 if you have the <unistd.h> header file.
	conf.check_header('unistd.h', 'HAVE_UNISTD_H')

	return 1

def set_options(opt):
	try:
		# we do not know yet
		opt.add_option('--want-rpath', type='int', default=1, dest='want_rpath', help='set rpath to 1 or 0 [Default 1]')
	except:
		pass

	for i in "execprefix datadir libdir".split():
		opt.add_option('--'+i, type='string', default='', dest=i)

