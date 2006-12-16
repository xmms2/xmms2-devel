#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"Dependency tree holder"

import os, cPickle
import Params, Runner, Object, Node, Task, Scripting, Utils, Environment
from Params import debug, error, fatal, warning

g_saved_attrs = 'm_root m_srcnode m_bldnode m_tstamp_variants m_depends_on m_deps_tstamp m_raw_deps'.split()
"Build class members to save"

class BuildDTO:
	"holds the data to store using cPickle"
	def __init__(self):
		pass
	def init(self, bdobj):
		global g_saved_attrs
		for a in g_saved_attrs:
			setattr(self, a, getattr(bdobj, a))
	def update_build(self, bdobj):
		global g_saved_attrs
		for a in g_saved_attrs:
			setattr(bdobj, a, getattr(self, a))

class Build:
	"holds the dependency tree"
	def __init__(self):

		# dependency tree
		self._init_data()

		# ======================================= #
		# globals

		# map a name to an environment, the 'default' must be defined
		self.m_allenvs = {}

		# there should be only one build dir in use at a time
		Params.g_build = self

		# ======================================= #
		# code for reading the scripts

		# project build directory - do not reset() from load_dirs() or _init_data()
		self.m_bdir = ''

		# the current directory from which the code is run
		# the folder changes everytime a wscript is read
		self.m_curdirnode = None

		# temporary holding the subdirectories containing scripts - look in Scripting.py
		self.m_subdirs=[]

		# ======================================= #
		# cache variables

		# local cache for absolute paths - m_abspath_cache[variant][node]
		self.m_abspath_cache = {}

		# local cache for relative paths
		# two nodes - hashtable of hashtables - g_relpath_cache[child][parent])
		self._relpath_cache = {}

		# cache for height of the node (amount of folders from the root)
		self.m_height_cache = {}

		# list of folders that are already scanned
		# so that we do not need to stat them one more time
		self.m_scanned_folders  = []

		# file contents
		self._cache_node_content = {}

		# ======================================= #
		# tasks and objects

		# objects that are not posted and objects already posted
		# -> delay task creation
		self.m_outstanding_objs = []

		# build dir variants (release, debug, ..)
		self.set_variants(['default'])

		# TODO used by xmlwaf
		self.pushed = []

	def _init_data(self):
		debug("init data called", 'build')

		# filesystem root - root name is Params.g_rootname
		self.m_root            = Node.Node('', None)

		# source directory
		self.m_srcnode         = None
		# build directory
		self.m_bldnode         = None

		# TODO: this code does not look too good
		# nodes signatures: self.m_tstamp_variants[variant_name][node] = signature_value
		self.m_tstamp_variants = {}

		# one node has nodes it depends on, tasks cannot be stored
		# self.m_depends_on[variant][node] = [node1, node2, ..]
		self.m_depends_on      = {}

		# m_deps_tstamp[variant][node] = node_tstamp_of_the_last_scan
		self.m_deps_tstamp     = {}

		# results of a scan: self.m_raw_deps[variant][node] = [filename1, filename2, filename3]
		# for example, find headers in c files
		self.m_raw_deps        = {}

	def _init_variants(self):
		#print "init variants called"
		debug("init variants", 'build')
		for name in Utils.to_list(self._variants)+[0]:
			for v in 'm_tstamp_variants m_depends_on m_deps_tstamp m_raw_deps m_abspath_cache'.split():
				var = getattr(self, v)
				if not name in var:
					var[name] = {}

		for env in self.m_allenvs.values():
			for f in env['dep_files']:
				lst = f.split('/')
				topnode = self.m_srcnode.find_node(lst[:-1])
				newnode = topnode.search_existing_node([lst[-1]])
				if not newnode:
					newnode = Node.Node(lst[-1], topnode)
					topnode.append_build(newnode)
				try:
					hash = Params.h_file(topnode.abspath(env)+os.sep+lst[-1])
				except IOError:
					hash = Params.sig_nil
				self.m_tstamp_variants[env.variant()][newnode] = hash

	# load existing data structures from the disk (stored using self._store())
	def _load(self):
		try:
			file = open(os.path.join(self.m_bdir, Params.g_dbfile), 'rb')
			dto = cPickle.load(file)
			dto.update_build(self)
			file.close()
		except:
			debug("resetting the build object (dto failed)", 'build')
			self._init_data()
			self._init_variants()
		#self.dump()

	# store the data structures on disk, retrieve with self._load()
	def _store(self):
		file = open(os.path.join(self.m_bdir, Params.g_dbfile), 'wb')
		dto = BuildDTO()
		dto.init(self)
		cPickle.dump(dto, file, -1) # remove the '-1' for unoptimized version
		file.close()

	# ======================================= #

	def set_variants(self, variants):
		debug("set_variants", 'build')
		self._variants = variants
		self._init_variants()

	def save(self):
		self._store()

	def clean(self):
		debug("clean called", 'build')
		Object.flush()
		# if something special is needed
		for obj in Object.g_allobjs: obj.cleanup()
		# now for each task, make sure to remove the objects
		# 4 for loops
		#import Task
		for group in Task.g_tasks.groups:
			for p in group.prio:
				for t in group.prio[p]:
					try:
						for node in t.m_outputs:
							try: os.remove(node.abspath(t.m_env))
							except: pass
					except:
						pass

	def compile(self):
		debug("compile called", 'build')
		ret = 0

		os.chdir(self.m_bdir)

		#import hotshot
		#prof = hotshot.Profile("/tmp/proftest.txt")
		#prof.runcall(Object.flush)
		#prof.close()
		Object.flush()

		#self.dump()

		if Params.g_options.jobs <=1:
			generator = Runner.JobGenerator(self)
			executor = Runner.Serial(generator)
		else:
			executor = Runner.Parallel(self, Params.g_options.jobs)

		self.m_generator = executor.m_generator

		debug("executor starting", 'build')
		try:

			#import hotshot
			#prof = hotshot.Profile("/tmp/proftest.txt")
			#prof.runcall(executor.start)
			#prof.close()
			ret = executor.start()
			#ret = 0
		except KeyboardInterrupt:
			os.chdir(self.m_srcnode.abspath())
			self._store()
			raise
		except Runner.CompilationError:
			fatal("Compilation failed")
		#finally:
		#	os.chdir( self.m_srcnode.abspath() )

		#self.dump()

		os.chdir( self.m_srcnode.abspath() )
		return ret

	def install(self):
		"this function is called for both install and uninstall"
		debug("install called", 'build')
		Object.flush()
		for obj in Object.g_allobjs:
			if obj.m_posted: obj.install()

	def add_subdirs(self, dirs):
		lst = Utils.to_list(dirs)
		for d in lst:
			if not d: continue
			Scripting.add_subdir(d, self)

	def create_obj(self, objname, *k, **kw):
		try:
			return Object.g_allclasses[objname](*k, **kw)
		except:
			print "error in create_obj", objname
			raise

	def load_envs(self):
		cachedir = Params.g_cachedir
		try:
			lst = os.listdir(cachedir)
		except:
			fatal('The project was not configured: run "waf configure" first!')

		if not lst: raise "file not found"
		for file in lst:
			if len(file) < 3: continue
			if file[-3:] != '.py': continue

			env = Environment.Environment()
			ret = env.load(os.path.join(cachedir, file))
			name = file.split('.')[0]

			if not ret:
				print "could not load env ", name
				continue
			self.m_allenvs[name] = env
			try:
				for t in env['tools']: env.setup(**t)
			except:
				print "loading failed:", file
				raise

	# ======================================= #
	# node and folder handling

	# this should be the main entry point
	def load_dirs(self, srcdir, blddir, isconfigure=None):
		"this functions should be the start of everything"

		# there is no reason to bypass this check
		try:
			if srcdir == blddir or os.path.samefile(srcdir, blddir):
				fatal("build dir must be different from srcdir ->"+str(srcdir)+" ->"+str(blddir))
		except:
			pass

		# set the source directory
		if not os.path.isabs(srcdir):
			srcdir = Utils.join_path(os.path.abspath('.'),srcdir)

		# set the build directory it is a path, not a node (either absolute or relative)
		if not os.path.isabs(blddir):
			self.m_bdir = os.path.abspath(blddir)
		else:
			self.m_bdir = blddir

		if not isconfigure:
			self._load()
			if self.m_srcnode:
				self.m_curdirnode = self.m_srcnode
				return


		self.m_srcnode = self.ensure_node_from_path(srcdir)
		debug("srcnode is %s and srcdir %s" % (str(self.m_srcnode), srcdir), 'build')

		self.m_curdirnode = self.m_srcnode

		self.m_bldnode = self.ensure_node_from_path(self.m_bdir)

		# create this build dir if necessary
		try: os.makedirs(blddir)
		except: pass

	def ensure_node_from_path(self, abspath):
		"return a node corresponding to an absolute path, creates nodes if necessary"
		debug('ensure_node_from_path %s' % (abspath), 'build')
		plst = Utils.split_path(abspath)
		curnode = self.m_root # root of the tree
		for dirname in plst:
			if not dirname: continue
			if dirname == '.': continue
			#found=None
			found = curnode.get_dir(dirname,None)
			#for cand in curnode.m_dirs:
			#	if cand.m_name == dirname:
			#		found = cand
			#		break
			if not found:
				found = Node.Node(dirname, curnode)
				curnode.append_dir(found)
			curnode = found

		return curnode

	def ensure_node_from_lst(self, node, plst):
		"ensure a directory node from a list, given a node to start from"
		if not node:
			error('ensure_node_from_lst node is not defined')
			raise "pass a valid node to ensure_node_from_lst"

		curnode=node
		for dirname in plst:
			if not dirname: continue
			if dirname == '.': continue
			if dirname == '..':
				curnode = curnode.m_parent
				continue
			#found=None
			found=curnode.get_dir(dirname, None)
			#for cand in curnode.m_dirs:
			#	if cand.m_name == dirname:
			#		found = cand
			#		break
			if not found:
				found = Node.Node(dirname, curnode)
				curnode.append_dir(found)
			curnode = found
		return curnode

	def rescan(self, src_dir_node):
		""" first list the files in the src dir and update the nodes
		    - for each variant build dir (multiple build dirs):
		        - list the files in the build dir, update the nodes

		this makes (n bdirs)+srdir to scan (at least 2 folders)
		so we might want to do it in parallel in some future
		"""

		# do not rescan over and over again
		if src_dir_node in self.m_scanned_folders: return

		#debug("rescanning "+str(src_dir_node), 'build')

		# list the files in the src directory, adding the signatures
		files = self.scan_src_path(src_dir_node, src_dir_node.abspath(), src_dir_node.files())
		#debug("files found in folder are "+str(files), 'build')
		src_dir_node.set_files(files)

		# list the files in the build dirs
		# remove the existing timestamps if the build files are removed
		lst = self.m_srcnode.difflst(src_dir_node)
		for variant in Utils.to_list(self._variants):
			sub_path = Utils.join_path(self.m_bldnode.abspath(), variant , *lst)
			try:
				files = self.scan_path(src_dir_node, sub_path, src_dir_node.build(), variant)
				src_dir_node.set_build(files)
			except OSError:
				#debug("osError on " + sub_path, 'build')

				# listdir failed, remove all sigs of nodes
				dict = self.m_tstamp_variants[variant]
				for node in src_dir_node.build():
					if node in dict:
						dict.__delitem__(node)
				os.makedirs(sub_path)
				src_dir_node.set_build([])
		self.m_scanned_folders.append(src_dir_node)

	def needs_rescan(self, node, env):
		"tell if a node has changed, to update the cache"
		#print "needs_rescan for ", node, node.m_tstamp
		variant = node.variant(env)
		try:
			if self.m_deps_tstamp[variant][node] == self.m_tstamp_variants[variant][node]:
				#print "no need to rescan", node.m_tstamp
				return 0
		except KeyError:
			return 1
		return 1

	# ======================================= #
	def scan_src_path(self, i_parent_node, i_path, i_existing_nodes):

		# read the dir contents, ignore the folders in it
		l_names_read = os.listdir(i_path)

		debug("folder contents "+str(l_names_read), 'build')

		# there are two ways to obtain the partitions:
		# 1 run the comparisons two times (not very smart)
		# 2 reduce the sizes of the list while looping

		l_names = l_names_read
		l_nodes = i_existing_nodes
		l_kept  = []

		for node in l_nodes:
			i     = 0
			name  = node.m_name
			l_len = len(l_names)
			while i < l_len:
				if l_names[i] == name:
					l_kept.append(node)
					break
				i += 1
			if i < l_len:
				del l_names[i]

		# Now:
		# l_names contains the new nodes (or files)
		# l_kept contains only nodes that actually exist on the filesystem
		for node in l_kept:
			try:
				# update the time stamp
				self.m_tstamp_variants[0][node] = Params.h_file(node.abspath())
			except:
				fatal("a file is readonly or has become a dir "+node.abspath())

		debug("new files found "+str(l_names), 'build')

		l_path = i_path + os.sep
		for name in l_names:
			try:
				# will throw an exception if not a file or if not readable
				# we assume that this is better than performing a stat() first
				# TODO is it possible to distinguish the cases ?
				st = Params.h_file(l_path + name)
				l_child = Node.Node(name, i_parent_node)
			except:
				continue
			self.m_tstamp_variants[0][l_child] = st
			l_kept.append(l_child)
		return l_kept

	def scan_path(self, i_parent_node, i_path, i_existing_nodes, i_variant):
		"""in this function we do not add timestamps but we remove them
		when the files no longer exist (file removed in the build dir)"""

		# read the dir contents, ignore the folders in it
		l_names_read = os.listdir(i_path)

		# there are two ways to obtain the partitions:
		# 1 run the comparisons two times (not very smart)
		# 2 reduce the sizes of the list while looping

		l_names = l_names_read
		l_nodes = i_existing_nodes
		l_rm    = []

		for node in l_nodes:
			i     = 0
			name  = node.m_name
			l_len = len(l_names)
			while i < l_len:
				if l_names[i] == name:
					break
				i += 1
			if i < l_len:
				del l_names[i]
			else:
				l_rm.append(node)

		# remove the stamps of the nodes that no longer exist in the build dir
		for node in l_rm:

			#print "\nremoving the timestamp of ", node, node.m_name
			#print node.m_parent.m_build
			#print l_names_read
				#print l_names

			if node in self.m_tstamp_variants[i_variant]:
				self.m_tstamp_variants[i_variant].__delitem__(node)
		return l_nodes

	def dump(self):
		"for debugging"
		def printspaces(count):
			if count>0: return printspaces(count-1)+"-"
			return ""
		def recu(node, count):
			accu = printspaces(count)
			accu+= "> "+node.m_name+" (d)\n"
			for child in node.files():
				accu+= printspaces(count)
				accu+= '-> '+child.m_name+' '

				for variant in self.m_tstamp_variants:
					#print "variant %s"%variant
					var = self.m_tstamp_variants[variant]
					#print var
					if child in var:
						accu+=' [%s,%s] ' % (str(variant), Params.vsig(var[child]))

				accu+='\n'
				#accu+= ' '+str(child.m_tstamp)+'\n'
				# TODO #if node.files()[file].m_newstamp != node.files()[file].m_oldstamp: accu += "\t\t\t(modified)"
				#accu+= node.files()[file].m_newstamp + "< >" + node.files()[file].m_oldstamp + "\n"
			for child in node.build():
				accu+= printspaces(count)
				accu+= '-> '+child.m_name+' (b) '

				for variant in self.m_tstamp_variants:
					#print "variant %s"%variant
					var = self.m_tstamp_variants[variant]
					#print var
					if child in var:
						accu+=' [%s,%s] ' % (str(variant), Params.vsig(var[child]))

				accu+='\n'
				#accu+= ' '+str(child.m_tstamp)+'\n'
				# TODO #if node.files()[file].m_newstamp != node.files()[file].m_oldstamp: accu += "\t\t\t(modified)"
				#accu+= node.files()[file].m_newstamp + "< >" + node.files()[file].m_oldstamp + "\n"
			for dir in node.dirs(): accu += recu(dir, count+1)
			return accu

		Params.pprint('CYAN', recu(self.m_root, 0) )
		Params.pprint('CYAN', 'size is '+str(self.m_root.size()))

		#keys = self.m_name2nodes.keys()
		#for k in keys:
		#	print k, '\t\t', self.m_name2nodes[k]


	def pushdir(self, dir):
		node = self.ensure_node_from_lst(self.m_curdirnode, Utils.split_path(dir))
		self.pushed = [self.m_curdirnode]+self.pushed
		self.m_curdirnode = node

	def popdir(self):
		self.m_curdirnode = self.pushed.pop(0)

	def env_of_name(self, name):
		if not name:
			error('env_of_name called with no name!')
			return None
		try:
			return self.m_allenvs[name]
		except:
			error('no such environment'+name)
			return None

	def add_group(self, name=''):
		Object.flush()
		Task.g_tasks.add_group(name)

	def createObj(self, objname, *k, **kw):
		"deprecated"
		warning('use bld.create_obj instead of bld.createObj')
		return self.create_obj(objname, *k, **kw)


if 0: #sys.version_info[:2] < [2,3]:
	def scan_src_path(self, i_parent_node, i_path, i_existing_nodes):
		l_names_read = os.listdir(i_path)

		l_names = set(l_names_read)
		l_nodes = i_existing_nodes
		l_kept  = []
		l_kept_names = set()

		for node in l_nodes:
			if node.m_name in l_names:
				l_kept.append(node)
				l_kept_names.add(node.m_name)

		l_new_files = l_names.difference(l_kept_names)

		# Now:
		# l_names contains the new nodes (or files)
		# l_kept contains only nodes that actually exist on the filesystem
		for node in l_kept:
			try:
			# update the time stamp
				self.m_tstamp_variants[0][node] = Params.h_file(node.abspath())
			except:
				fatal("a file is readonly or has become a dir "+node.abspath())

		debug("new files found "+str(l_names), 'build')

		for name in l_new_files:#l_names:
			try:
				# will throw an exception if not a file or if not readable
				# we assume that this is better than performing a stat() first
				# TODO is it possible to distinguish the cases ?
				st = Params.h_file(Utils.join_path(i_path,name))
				l_child = Node.Node(name, i_parent_node)
			except:
				continue
			self.m_tstamp_variants[0][l_child] = st
			l_kept.append(l_child)
		return l_kept

	def scan_path(self, i_parent_node, i_path, i_existing_nodes, i_variant):
		l_names_read = os.listdir(i_path)

		l_names = set(l_names_read)
		l_node_names = set()
		l_nodes = i_existing_nodes
		l_rm    = []

		for node in l_nodes:
			if not node.m_name in l_names:
				l_rm.append(node)

		for node in l_rm:
			if node in self.m_tstamp_variants[i_variant]:
				self.m_tstamp_variants[i_variant].__delitem__(node)
		return l_nodes

	Build.scan_src_path = scan_src_path
	Build.scan_path = scan_path

