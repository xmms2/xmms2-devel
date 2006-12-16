#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"base for all c/c++ programs and libraries"

import os, types, sys, re, md5
import Action, Object, Params, Scan, Common, Utils
from Params import error, debug, fatal, warning
from Params import hash_sig_weak

g_prio_link=101
"""default priority for link tasks:
an even number means link tasks may be parallelized
an odd number is probably the thing to do"""

g_src_file_ext = ['.c', '.cpp', '.cc']
"default extensions for source files"

cregexp1 = re.compile(r'^[ \t]*#[ \t]*(?:include)[ \t]*(?:/\*.*?\*/)?[ \t]*(<|")([^>"]+)(>|")', re.M)
"regexp for computing dependencies (when not using the preprocessor)"

class c_scanner(Scan.scanner):
	"scanner for c/c++ files"
	def __init__(self):
		Scan.scanner.__init__(self)

	# re-scan a node, update the tree
	def do_scan(self, node, env, hashparams):
		debug("do_scan(self, node, env, hashparams)", 'ccroot')

		variant = node.variant(env)

		if not node:
			error("BUG rescanning a null node")
			return
		(nodes, names) = self.scan(node, env, **hashparams)
		if Params.g_zones: debug('scanner for %s returned %s %s' % (node.m_name, str(nodes), str(names)), 'deps')

		tree = Params.g_build
		tree.m_depends_on[variant][node] = nodes
		tree.m_raw_deps[variant][node] = names

		tree.m_deps_tstamp[variant][node] = tree.m_tstamp_variants[variant][node]
		if Params.g_preprocess:
			for n in nodes:
				try:
					tree.m_deps_tstamp[variant][n] = tree.m_tstamp_variants[variant][n]
				except:
					pass


	def get_signature(self, task):
		debug("get_signature(self, task)", 'ccroot')
		if Params.g_preprocess:
			if Params.g_strong_hash:
				return self._get_signature_preprocessor(task)
			else:
				return self._get_signature_preprocessor_weak(task)
		else:
			if Params.g_strong_hash:
				return self._get_signature_regexp_strong(task)
			else:
				return self._get_signature_regexp_weak(task)

	def scan(self, node, env, path_lst, defines=[]):
		debug("scan", 'ccroot')
		if Params.g_preprocess:
			return self._scan_preprocessor(node, env, path_lst, defines)
		else:
			# the regular scanner does not use the define values
			return self._scan_default(node, env, path_lst)
		#return self._scan_default(node, env, path_lst)

	def _scan_default(self, node, env, path_lst):
		debug("_scan_default(self, node, env, path_lst)", 'ccroot')
		variant = node.variant(env)
		file = open(node.abspath(env), 'rb')
		found = cregexp1.findall( file.read() )
		file.close()

		nodes = []
		names = []
		if not node: return (nodes, names)

		for (_, name, _) in found:
			#print 'boo', name

			# quite a few nested 'for' loops, looking suspicious
			found = None
			for dir in path_lst:
				found = dir.get_file(name)
				if found:
					break
			if found: nodes.append(found)
			else:     names.append(name)
		#print "-S ", nodes, names
		return (nodes, names)

	def _scan_preprocessor(self, node, env, path_lst, defines=[]):
		debug("_scan_preprocessor(self, node, env, path_lst)", 'croot')
		import preproc
		gruik = preproc.cparse(nodepaths = path_lst, defines=defines)
		gruik.start2(node, env)
		if Params.g_verbose:
			debug("nodes found for %s %s %s" % (str(node), str(gruik.m_nodes), str(gruik.m_names)), 'deps')
		return (gruik.m_nodes, gruik.m_names)

	def _get_signature_regexp_strong(self, task):
		m = md5.new()
		tree = Params.g_build
		seen = []
		env  = task.m_env
		def add_node_sig(node):
			if not node: warning("null node in get_node_sig")
			if node in seen: return

			# TODO - using the variant each time is stupid
			variant = node.variant(env)
			seen.append(node)

			# rescan if necessary, and add the signatures of the nodes it depends on
			if tree.needs_rescan(node, task.m_env):
				self.do_scan(node, task.m_env, task.m_scanner_params)
			lst = tree.m_depends_on[variant][node]
			for dep in lst: add_node_sig(dep)
			m.update(tree.m_tstamp_variants[variant][node])
		# add the signatures of the input nodes
		for node in task.m_inputs: add_node_sig(node)
		# add the signatures of the task it depends on
		for task in task.m_run_after: m.update(task.signature())
		return m.digest()

	def _get_signature_regexp_weak(self, task):
		msum = 0
		tree = Params.g_build
		seen = []
		env  = task.m_env
		def add_node_sig(node):
			if not node: warning("null node in get_node_sig")
			if node in seen: return 0

			sum = 0

			# TODO - using the variant each time is stupid
			variant = node.variant(env)
			seen.append(node)

			sum += tree.m_tstamp_variants[variant][node]
			# rescan if necessary, and add the signatures of the nodes it depends on
			if tree.needs_rescan(node, task.m_env): self.do_scan(node, task.m_env, task.m_scanner_params)
			lst = tree.m_depends_on[variant][node]
			for dep in lst: sum += add_node_sig(dep)
			return sum
		# add the signatures of the input nodes
		for node in task.m_inputs: msum = hash_sig_weak(msum, add_node_sig(node))
		# add the signatures of the task it depends on
		for task in task.m_run_after: msum = hash_sig_weak(msum, task.signature())
		return int(msum)

	def _get_signature_preprocessor_weak(self, task):
		msum = 0
		tree = Params.g_build
		rescan = 0
		env=task.m_env
		seen=[]
		def add_node_sig(n):
			if not n: warning("null node in get_node_sig")
			if n.m_name in seen: return 0

			# TODO - using the variant each time is stupid
			variant = n.variant(env)
			seen.append(n.m_name)
			return tree.m_tstamp_variants[variant][n]

		# there is only one c/cpp file as input
		node = task.m_inputs[0]

		variant = node.variant(env)
		if not variant == 0: fatal('variant is not 0')

		if tree.needs_rescan(node, task.m_env): rescan = 1
		if not rescan:
			for anode in tree.m_depends_on[variant][node]:
				if tree.needs_rescan(anode, task.m_env): rescan = 1

		# rescan the cpp file if necessary
		if rescan:
			#print "rescanning ", node
			self.do_scan(node, task.m_env, task.m_scanner_params)

#		print "rescan for ", task.m_inputs[0], " is ", rescan,  " and deps ", \
#			tree.m_depends_on[variant][node], tree.m_raw_deps[variant][node]

		# we are certain that the files have been scanned - compute the signature
		msum = hash_sig_weak(msum, add_node_sig(node))
		for n in tree.m_depends_on[variant][node]:
			msum = hash_sig_weak(msum, add_node_sig(n))

		# and now xor the signature with the other tasks
		for task in task.m_run_after:
			msum = hash_sig_weak(msum, task.signature())
		#debug("signature of the task %d is %s" % (task.m_idx, Params.vsig(sig)), 'ccroot')

		return int(msum)

	def _get_signature_preprocessor(self, task):
		# assumption: there is only one cpp file to compile in a task

		tree = Params.g_build
		rescan = 0

		m = md5.new()
		seen=[]
		env = task.m_env
		def add_node_sig(n):
			if not n: warning("warning: null node in get_node_sig")
			if n.m_name in seen: return

			# TODO - using the variant each time is stupid
			variant = n.variant(env)
			seen.append(n.m_name)
			m.update(tree.m_tstamp_variants[variant][n])

		# there is only one c/cpp file as input
		node = task.m_inputs[0]

		variant = node.variant(env)
		if not variant == 0: fatal('variant is not 0')

		if tree.needs_rescan(node, task.m_env): rescan = 1
		#if rescan: print "node has changed, a rescan is req ", node

		if not rescan:
			#print node, "depends on ", tree.m_depends_on[variant][node]
			for anode in tree.m_depends_on[variant][node]:
				if tree.needs_rescan(anode, task.m_env):
					#print "rescanning because of ", anode
					rescan = 1

		# rescan the cpp file if necessary
		if rescan:
			#print "rescanning ", node
			self.do_scan(node, task.m_env, task.m_scanner_params)

		# DEBUG
		#print "rescan for ", task.m_inputs[0], " is ", rescan,  " and deps ", \
		#	tree.m_depends_on[variant][node], tree.m_raw_deps[variant][node]

		# we are certain that the files have been scanned - compute the signature
		add_node_sig(node)
		for n in tree.m_depends_on[variant][node]: add_node_sig(n)

		#for task in task.m_run_after: m.update(task.signature())
		return m.digest()

g_c_scanner = c_scanner()
"scanner for c programs"


def read_la_file(path):
	sp = re.compile(r'^([^=]+)=\'(.*)\'$')
	dc={}
	file = open(path, "r")
	for line in file.readlines():
		try:
			#print sp.split(line.strip())
			_, left, right, _ = sp.split(line.strip())
			dc[left]=right
		except:
			pass
	file.close()
	return dc

# fake libtool files
fakelibtool_vardeps = ['CXX', 'PREFIX']
def fakelibtool_build(task):
	# Writes a .la file, used by libtool
	dest  = open(task.m_outputs[0].abspath(task.m_env), 'w')
	sname = task.m_inputs[0].m_name
	fu = dest.write
	fu("# Generated by ltmain.sh - GNU libtool 1.5.18 - (pwn3d by BKsys II code name WAF)\n")
	if task.m_env['vnum']:
		nums=task.m_env['vnum'].split('.')
		libname = task.m_inputs[0].m_name
		name3 = libname+'.'+task.m_env['vnum']
		name2 = libname+'.'+nums[0]
		name1 = libname
		fu("dlname='%s'\n" % name2)
		strn = " ".join([name3, name2, name1])
		fu("library_names='%s'\n" % (strn) )
	else:
		fu("dlname='%s'\n" % sname)
		fu("library_names='%s %s %s'\n" % (sname, sname, sname) )
	fu("old_library=''\n")
	vars = ' '.join(task.m_env['libtoolvars']+task.m_env['LINKFLAGS'])
	fu("dependency_libs='%s'\n" % vars)
	fu("current=0\n")
	fu("age=0\nrevision=0\ninstalled=yes\nshouldnotlink=no\n")
	fu("dlopen=''\ndlpreopen=''\n")
	fu("libdir='%s/lib'\n" % task.m_env['PREFIX'])
	dest.close()
	return 0
# TODO move this line out
Action.Action('fakelibtool', vars=fakelibtool_vardeps, func=fakelibtool_build, color='BLUE')

class ccroot(Object.genobj):
	"Parent class for programs and libraries in languages c, c++ and moc (Qt)"
	s_default_ext = []
	def __init__(self, type='program'):
		Object.genobj.__init__(self, type)

		self.env = Params.g_build.m_allenvs['default'].copy()
		if not self.env['tools']: fatal('no tool selected')

		# installation directory, to override PREFIX/bin or PREFIX/lib
		self.install_in = ''

		# includes, seen from the current directory
		self.includes=''

		self.rpaths=''

		self.uselib=''

		# new scheme: provide the names of the local libraries to link with
		# the objects found will be post()-ed
		self.uselib_local=''


		# add .o files produced by another Object subclass
		self.add_objects = ''

		self.m_linktask=None

		# libtool emulation
		self.want_libtool=0 # -1: fake; 1: real
		self.vnum=''

		self._incpaths_lst=[]
		self.defines_lst = []
		self._bld_incpaths_lst=[]

		self.p_compiletasks=[]

		# do not forget to set the following variables in a subclass
		self.p_flag_vars = []
		self.p_type_vars = []

		# TODO ???
		self.m_type_initials = ''

		self.chmod = 0755

	def create_task(self, type, env=None, nice=10):
		"overrides Object.create_task to catch the creation of cpp tasks"
		task = Object.genobj.create_task(self, type, env, nice)
		if type == self.m_type_initials:
			self.p_compiletasks.append(task)
		return task

	def get_valid_types(self):
		"subclass me"
		fatal('subclass method get_valid_types of ccroot')

	def find_sources_in_dirs(self, dirnames, excludes=[]):
		"subclass if necessary"
		lst=[]
		excludes = Utils.to_list(excludes)
		#make sure dirnames is a list helps with dirnames with spaces
		dirnames = Utils.to_list(dirnames)

		ext_lst = []
		ext_lst += self.s_default_ext
		try:
			for var in self.__class__.__dict__['all_hooks']:
				ext_lst += self.env[var]
		except:
			pass

		for name in dirnames:
			#print "name is ", name
			anode = Params.g_build.ensure_node_from_lst(self.m_current_path, 
				Utils.split_path(name))
			#print "anode ", anode.m_name, " ", anode.files()
			Params.g_build.rescan(anode)
			#print "anode ", anode.m_name, " ", anode.files()

			#node = self.m_current_path.find_node( name.split(os.sep) )
			for file in anode.files():
				#print "file found ->", file
				(base, ext) = os.path.splitext(file.m_name)
				if ext in ext_lst:
					s = file.relpath(self.m_current_path)
					if not s in lst:
						if s in excludes: continue
						lst.append(s)

		self.source = self.source+' '+(" ".join(lst))

	def apply(self):
		"adding some kind of genericity is tricky subclass this method if it does not suit your needs"
		debug("apply called for "+self.m_type_initials, 'ccroot')
		if not self.m_type in self.get_valid_types():
			fatal('Invalid type for object: '+self.m_type_initials)

		self.apply_type_vars()
		self.apply_incpaths()
		self.apply_defines()

		self.apply_core()

		self.apply_lib_vars()
		self.apply_obj_vars()
		if self.m_type != 'objects': self.apply_objdeps()

	def apply_core(self):
		if self.want_libtool and self.want_libtool>0:
			self.apply_libtool()

		if self.m_type == 'objects':
			type = 'program'
		else:
			type = self.m_type

		obj_ext = self.env[type+'_obj_ext'][0]
		pre = self.m_type_initials

		# get the list of folders to use by the scanners
		# all our objects share the same include paths anyway
		tree = Params.g_build
		dir_lst = { 'path_lst' : self._incpaths_lst, 'defines' : self.defines_lst }

		lst = self.to_list(self.source)
		find_node = self.m_current_path.find_node
		for filename in lst:
			#node = self.find(filename)
			node = self.m_current_path.find_node(Utils.split_path(filename))
			if not node: fatal("source not found: %s in %s" % (filename, str(self.m_current_path)))

			# Extract the extension and look for a handler hook.
			k = max(0, filename.rfind('.'))
			try:
				self.get_hook(filename[k:])(self, node)
				continue
			except TypeError:
				pass

			# create the compilation task: cpp or cc
			task = self.create_task(self.m_type_initials, self.env)

			task.m_scanner        = g_c_scanner
			task.m_scanner_params = dir_lst

			task.m_inputs = [node]
			task.m_outputs = [node.change_ext(obj_ext)]

		# if we are only building .o files, tell which ones we built
		if self.m_type=='objects':
			outputs = []
			app = outputs.append
			for t in self.p_compiletasks: app(t.m_outputs[0])
			self.out_nodes = outputs
			return

		# and after the objects, the remaining is the link step
		# link in a lower priority (101) so it runs alone (default is 10)
		global g_prio_link
		if self.m_type=='staticlib':
			linktask = self.create_task(pre+'_link_static', self.env, g_prio_link)
		else:
			linktask = self.create_task(pre+'_link', self.env, g_prio_link)
		outputs = []
		app = outputs.append
		for t in self.p_compiletasks: app(t.m_outputs[0])
		linktask.set_inputs(outputs)
		linktask.set_outputs(self.find(self.get_target_name()))

		self.m_linktask = linktask

		if self.m_type != 'program' and self.want_libtool:
			latask = self.create_task('fakelibtool', self.env, 200)
			latask.set_inputs(linktask.m_outputs)
			latask.set_outputs(linktask.m_outputs[0].change_ext('.la'))
			self.m_latask = latask

	def get_target_name(self, ext=None):
		return self.get_library_name(self.target, self.m_type, ext)

	def get_library_name(self, name, type, ext=None):
		prefix = self.env[type+'_PREFIX']
		suffix = self.env[type+'_SUFFIX']

		if ext: suffix = ext
		if not prefix: prefix=''
		if not suffix: suffix=''
		return ''.join([prefix, name, suffix])

	def apply_defines(self):
		"subclass me"
		pass

	def apply_incpaths(self):
		lst = []
		for i in self.to_list(self.uselib):
			if self.env['CPPPATH_'+i]:
				lst += self.to_list(self.env['CPPPATH_'+i])
		inc_lst = self.to_list(self.includes) + lst
		lst = self._incpaths_lst

		# add the build directory
		self._incpaths_lst.append(Params.g_build.m_bldnode)
		self._incpaths_lst.append(Params.g_build.m_srcnode)

		# now process the include paths
		tree = Params.g_build
		for dir in inc_lst:
			if os.path.isabs(dir[0]) or (len(dir) > 1 and dir[1] == ':'):
				self.env.appendValue('CPPPATH', dir)
				continue

			node = self.m_current_path.find_node(Utils.split_path(dir))
			if not node:
				debug("node not found in ccroot:apply_incpaths "+str(dir), 'ccroot')
				continue
			if not node in lst: lst.append(node)
			Params.g_build.rescan(node)
			self._bld_incpaths_lst.append(node)
		# now the nodes are added to self._incpaths_lst

	def apply_type_vars(self):
		debug('apply_type_vars called', 'ccroot')
		for var in self.p_type_vars:
			# each compiler defines variables like 'shlib_CXXFLAGS', 'shlib_LINKFLAGS', etc
			# so when we make a cppobj of the type shlib, CXXFLAGS are modified accordingly
			compvar = '_'.join([self.m_type, var])
			#print compvar
			value = self.env[compvar]
			if value: self.env.appendValue(var, value)

	def apply_obj_vars(self):
		debug('apply_obj_vars called for cppobj', 'ccroot')
		cpppath_st       = self.env['CPPPATH_ST']
		lib_st           = self.env['LIB_ST']
		staticlib_st     = self.env['STATICLIB_ST']
		libpath_st       = self.env['LIBPATH_ST']
		staticlibpath_st = self.env['STATICLIBPATH_ST']

		self.addflags('CXXFLAGS', self.cxxflags)
		self.addflags('CPPFLAGS', self.cppflags)

		app = self.env.appendValue

		# local flags come first
		# set the user-defined includes paths
		if not self._incpaths_lst: self.apply_incpaths()
		for i in self._bld_incpaths_lst:
			app('_CXXINCFLAGS', cpppath_st % i.bldpath(self.env))
			app('_CXXINCFLAGS', cpppath_st % i.srcpath(self.env))

		# set the library include paths
		for i in self.env['CPPPATH']:
			app('_CXXINCFLAGS', cpppath_st % i)
			#print self.env['_CXXINCFLAGS']
			#print " appending include ",i

		# this is usually a good idea
		app('_CXXINCFLAGS', cpppath_st % '.')
		app('_CXXINCFLAGS', cpppath_st % self.env.variant())
		tmpnode = Params.g_build.m_curdirnode
		app('_CXXINCFLAGS', cpppath_st % tmpnode.bldpath(self.env))
		app('_CXXINCFLAGS', cpppath_st % tmpnode.srcpath(self.env))

		for i in self.env['RPATH']:
			app('LINKFLAGS', i)

		for i in self.env['LIBPATH']:
			app('LINKFLAGS', libpath_st % i)

		for i in self.env['LIBPATH']:
			app('LINKFLAGS', staticlibpath_st % i)

		if self.env['STATICLIB']:
			app('LINKFLAGS', self.env['STATICLIB_MARKER'])
			for i in self.env['STATICLIB']:
				app('LINKFLAGS', staticlib_st % i)

		# fully static binaries ?
		if not self.env['FULLSTATIC']:
			if self.env['STATICLIB'] or self.env['LIB']:
				app('LINKFLAGS', self.env['SHLIB_MARKER'])

		if self.env['LIB']:
			for i in self.env['LIB']:
				app('LINKFLAGS', lib_st % i)

	def install(self):
		if not (Params.g_commands['install'] or Params.g_commands['uninstall']): return
		if self.install_in is 0: return
		dest_var    = ''
		dest_subdir = ''

		if self.install_in:
			dest_var    = self.install_in
			dest_subdir = ''
		else:
			if self.m_type == 'program':
				dest_var    = 'PREFIX'
				dest_subdir = 'bin'
			else:
				dest_var    = 'PREFIX'
				dest_subdir = 'lib'

		if self.m_type == 'program':
			self.install_results(dest_var, dest_subdir, self.m_linktask, chmod=self.chmod)
		elif self.m_type == 'shlib' or self.m_type == 'plugin':
			if self.want_libtool:
				self.install_results(dest_var, dest_subdir, self.m_latask)
			if sys.platform=='win32' or not self.vnum:
				self.install_results(dest_var, dest_subdir, self.m_linktask)
			else:
				libname = self.m_linktask.m_outputs[0].m_name

				nums=self.vnum.split('.')
				name3 = libname+'.'+self.vnum
				name2 = libname+'.'+nums[0]
				name1 = libname

				filename = self.m_linktask.m_outputs[0].relpath_gen(Params.g_build.m_curdirnode)

				Common.install_as(dest_var, dest_subdir+'/'+name3, filename)

				#print 'lib/'+name2, '->', name3
				#print 'lib/'+name1, '->', name2

				Common.symlink_as(dest_var, name3, dest_subdir+'/'+name2)
				Common.symlink_as(dest_var, name2, dest_subdir+'/'+name1)
		elif self.m_type == 'staticlib':
			self.install_results(dest_var, dest_subdir, self.m_linktask, chmod=0644)

	# This piece of code must suck, no one uses it
	def apply_libtool(self):
		self.env['vnum']=self.vnum

		paths=[]
		libs=[]
		libtool_files=[]
		libtool_vars=[]

		for l in self.env['LINKFLAGS']:
			if l[:2]=='-L':
				paths.append(l[2:])
			elif l[:2]=='-l':
				libs.append(l[2:])

		for l in libs:
			for p in paths:
				try:
					dict = read_la_file(p+'/lib'+l+'.la')
					linkflags2 = dict['dependency_libs']
					for v in linkflags2.split():
						if v[-3:] == '.la':
							libtool_files.append(v)
							libtool_vars.append(v)
							continue
						self.env.appendUnique('LINKFLAGS', v)
					break
				except:
					pass

		self.env['libtoolvars']=libtool_vars

		while libtool_files:
			file = libtool_files.pop()
			dict = read_la_file(file)
			for v in dict['dependency_libs'].split():
				if v[-3:] == '.la':
					libtool_files.append(v)
					continue
				self.env.appendUnique('LINKFLAGS', v)

	def apply_lib_vars(self):
		debug('apply_lib_vars called', 'ccroot')

		# 0. override if necessary
		self.process_vnum()

		# 1. the case of the libs defined in the project
		names = self.uselib_local.split()
		env=self.env
		htbl = Params.g_build.m_depends_on
		for name in names:
			for obj in Object.g_allobjs:
				if obj.name != name: continue

				if not obj.m_posted: obj.post()

				if obj.m_type == 'shlib':
					env.appendValue('LIB', obj.target)
				elif obj.m_type == 'plugin':
					if platform == 'darwin':
						env.appendValue('PLUGIN', obj.target)
					else:
						env.appendValue('LIB', obj.target)
				elif obj.m_type == 'staticlib':
					env.appendValue('STATICLIB', obj.target)
				else:
					error('unknown object type %s in apply_lib_vars' % obj.name)

				# add the path too
				tmp_path = obj.m_current_path.bldpath(self.env)
				if not tmp_path in env['LIBPATH']: env.appendValue('LIBPATH', tmp_path)

				# set the dependency over the link task
				self.m_linktask.m_run_after.append(obj.m_linktask)

				# make sure to rebuild our link task if obj.m_linktask is re-run
				try:
					lst = htbl[self.m_linktask.m_outputs[0]]
					for a in obj.m_linktask:
						if not a in lst:
							lst.append(a)
				except:
					htbl[self.m_linktask.m_outputs[0]] = obj.m_linktask.m_outputs

				# do not continue on all objects, we have found the interesting one
				break

		# 2. the case of the libs defined outside
		libs = self.uselib.split()
		for l in libs:
			for v in self.p_flag_vars:
				val=self.env[v+'_'+l]
				if val: self.env.appendValue(v, val)
	def process_vnum(self):
		if self.vnum and sys.platform != 'darwin' and sys.platform != 'win32':
			nums=self.vnum.split('.')
			# this is very unix-specific
			try: name3 = self.soname
			except: name3 = self.m_linktask.m_outputs[0].m_name+'.'+self.vnum.split('.')[0]
			self.env.appendValue('LINKFLAGS', '-Wl,-soname,'+name3)

	def apply_objdeps(self):
		"add the .o files produced by some other object files in the same manner as uselib_local"
		lst = self.add_objects.split()
		if not lst: return
		for obj in Object.g_allobjs:
			if obj.target in lst or obj.name in lst:
				if not obj.m_posted: obj.post()
				self.m_linktask.m_inputs += obj.out_nodes

	def addflags(self, var, value):
		"utility function for cc.py and ccroot.py: add self.cxxflags to CXXFLAGS"
		if type(var) is types.StringType:
			for i in value.split():
				self.env.appendValue(var, i)
		else:
			# TODO: double-check
			self.env[var] += value

