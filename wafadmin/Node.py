#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"A node represents a file"

import os
import Params, Utils
from Params import debug, error, fatal

class Node:
	def __init__(self, name, parent):

		self.m_name = name
		self.m_parent = parent
		self.m_cached_path = ""

		# Lookup dictionaries for O(1) access
		self.m_dirs_lookup = {}
		self.m_files_lookup = {}
		self.m_build_lookup = {}

		# Node name must contain only one level
		if Utils.split_path(name)[0] != name:
		#name == '.' or name == '..' or '/' in name or os.sep in name:
		#if '/' in name and parent:
			fatal('name forbidden '+name)

		if parent:
			if parent.get_file(name):
				error('node %s exists in the parent files %s already' % (name, str(parent)))
				raise "inconsistency"

			if parent.get_build(name):
				error('node %s exists in the parent build %s already' % (name, str(parent)))
				raise "inconsistency"

		# contents of this node (filesystem structure)
		# these lists contain nodes too
		#self.m_dirs		= [] # sub-folders
		#self.m_files	= [] # files existing in the src dir
		#self.m_build	= [] # nodes produced in the build dirs

		# debugging only - a node is supposed to represent exactly one folder
		# if os.sep in name: print "error in name ? "+name

		# timestamp or hash of the file (string hash or md5) - we will use the timestamp by default
		#self.m_tstamp	 = None

		# IMPORTANT:
		# Some would-be class properties are stored in Build: nodes to depend on, signature, flags, ..
		# In fact, unused class members increase the .wafpickle file size sensibly with lots of objects
		#	eg: the m_tstamp is used for every node, while the signature is computed only for build files

	def dirs(self):
		return self.m_dirs_lookup.values() #self.m_dirs

	def get_dir(self,name,default=None):
		return self.m_dirs_lookup.get(name,default)

	def append_dir(self, dir):
		self.m_dirs_lookup[dir.m_name]=dir
		#self.m_dirs.append(dir)

	def files(self):
		return self.m_files_lookup.values() #self.m_files

	def set_files(self,files):
		self.m_files_lookup={}
		for i in files: self.m_files_lookup[i.m_name]=i
		#self.m_files=files

	def get_file(self,name,default=None):
		return self.m_files_lookup.get(name,default)

	def append_file(self, dir):
		self.m_files_lookup[dir.m_name]=dir
		#self.m_files.append(dir)

	def build(self):
		return self.m_build_lookup.values() #self.m_build

	def set_build(self, build):
		self.m_build_lookup={}
		for i in build: self.m_build_lookup[i.m_name]=i
		#self.m_build=build

	def get_build(self,name,default=None):
		return self.m_build_lookup.get(name,default)

	def append_build(self, dir):
		self.m_build_lookup[dir.m_name]=dir
		#self.m_build.append(dir)

	def __str__(self):
		#TODO: fix this
		if self in self.m_parent.files(): isbld = ""
		#if self.m_parent.get_file(self.m_name): isbld = ""
		else: isbld = "b:"
		return "<%s%s>" % (isbld, self.abspath())

	def __repr__(self):
		#TODO: fix this
		if self in self.m_parent.files(): isbld = ""
		#if self.m_parent.get_file(self.m_name): isbld = ""
		else: isbld = "b:"
		return "<%s%s>" % (isbld, self.abspath())

	# ====================================================== #

	# for the build variants, the same nodes are used to spare memory
	# the timestamps/signatures are accessed using the following methods

	def get_tstamp_variant(self, variant):
		vars = Params.g_build.m_tstamp_variants[variant]
		try: return vars[variant]
		except: return None

	def set_tstamp_variant(self, variant, value):
		Params.g_build.m_tstamp_variants[variant][self] = value

	def get_tstamp_node(self):
		try: return Params.g_build.m_tstamp_variants[0][self]
		except: return None

	def set_tstamp_node(self, value):
		Params.g_build.m_tstamp_variants[0][self] = value

	# ====================================================== #

	# size of the subtree
	def size(self):
		l_size=1
		for i in self.dirs(): l_size += i.size()
		l_size += len(self.files())
		return l_size

	# uses a cache, so calling height should have no overhead
	def height(self):
		try:
			return Params.g_build.m_height_cache[self]
		except:
			if not self.m_parent: val=0
			else:				  val=1+self.m_parent.height()
			Params.g_build.m_height_cache[self]=val
			return val

	def child_of_name(self, name):
		return self.get_dir(name,None)
		#for d in self.m_dirs:
		#	debug('child of name '+d.m_name, 300)
		#	if d.m_name == name:
		#		return d
		# throw an exception ?
		#return None

	## ===== BEGIN relpath-related methods	===== ##

	# list of file names that separate a node from a child
	def difflst(self, child):
		if not child: error('Node difflst takes a non-null parameter!')
		lst=[]
		node = child
		while child != self:
			lst.append(child.m_name)
			child=child.m_parent
		lst.reverse()
		return lst

	## ------------ TODO : the following may need to be improved
	# list of paths up to the root
	# cannot remember where it is used (ita)
	def path_list(self):
		if not self.m_parent: return []
		return self.m_parent.pathlist().append(self.m_name)

	# returns a joined path string that can be reduced to the absolute path
	# DOES NOT needs to be reversed anymore (used by abspath)
	def __pathstr2(self):
		if self.m_cached_path: return self.m_cached_path
		dirlist = [self.m_name]
		cur_dir=self.m_parent
		while cur_dir:
			if cur_dir.m_name:
				dirlist = dirlist + [cur_dir.m_name]
			cur_dir = cur_dir.m_parent
		dirlist.reverse()

		joined = ""
		for f in dirlist: joined = os.path.join(joined,f)
		if not os.path.isabs(joined):
			joined = os.sep + joined

		self.m_cached_path=joined
		return joined
		#if not self.m_parent: return [Params.g_rootname]
		#return [self.m_name, os.sep]+self.m_parent.pathlist2()

	def find_node_by_name(self, name, lst):
		res = self.get_dir(name,None)
		if not res:
			res=self.get_file(name)
		if not res:
			res=self.get_build(name)
		if not res: return None

		return res.find_node( lst[1:] )

	# TODO : make it procedural, not recursive
	# find a node given an existing node and a list like ['usr', 'local', 'bin']
	def find_node(self, lst):
		#print self, lst
		if not lst: return self
		name=lst[0]

		Params.g_build.rescan(self)

		#print self.dirs()
		#print self.files()

		if name == '.':  return self.find_node( lst[1:] )
		if name == '..': return self.m_parent.find_node( lst[1:] )

		res = self.find_node_by_name(name,lst)
		if res: return res

		if len(lst)>0:
			node = Node(name, self)
			self.append_dir(node)
			#self.m_dirs.append(node)
			return node.find_node(lst[1:])

		#debug('find_node returns nothing '+str(self)+' '+str(lst), 300)
		return None

	def search_existing_node(self, lst):
		"returns a node from the tree, do not create if missing"
		#print self, lst
		if not lst: return self
		name=lst[0]

		Params.g_build.rescan(self)

		if name == '.':  return self.search_existing_node(lst[1:])
		if name == '..': return self.m_parent.search_existing_node(lst[1:])

		res = self.get_dir(name,None)
		if not res: res=self.get_file(name)
		if not res: res=self.get_build(name)
		if res: return res.search_existing_node(lst[1:])

		debug('search_existing_node returns nothing %s %s' % (str(self), str(lst)), 'node')
		return None

	# absolute path
	def abspath(self, env=None):
		if not env: variant = 0
		else: variant = self.variant(env)

		#print "variant is", self.m_name, variant, "and env is ", env

		## 1. stupid method
		# if self.m_parent is None: return ''
		# return self.m_parent.abspath()+os.sep+self.m_name
		#
		## 2. without a cache
		# return ''.join( self.pathlist2() )
		#
		## 3. with the cache
		try:
			return Params.g_build.m_abspath_cache[variant][self]
		except KeyError:
			if not variant:
				val=self.__pathstr2()
				#lst.reverse() - no need to reverse list anymore
				#val=''.join(lst)
				Params.g_build.m_abspath_cache[variant][self]=val
				return val
			else:
				p = Utils.join_path(Params.g_build.m_bldnode.abspath(),env.variant(),
					self.relpath(Params.g_build.m_srcnode))
				debug("var is p+q is "+p, 'node')
				return p

	# the build is launched from the top of the build dir (for example, in _build_/)
	def bldpath(self, env=None):
		name = self.m_name
		x = self.m_parent.get_file(name)
		if x: return self.relpath_gen(Params.g_build.m_bldnode)
		return Utils.join_path(env.variant(),self.relpath(Params.g_build.m_srcnode))

	# the build is launched from the top of the build dir (for example, in _build_/)
	def srcpath(self, env):
		name = self.m_name
		x = self.m_parent.get_build(name)
		if x: return self.bldpath(env)
		return self.relpath_gen(Params.g_build.m_bldnode)

	def bld_dir(self, env):
		return self.m_parent.bldpath(env)

	def bldbase(self, env):
		i = 0
		n = self.m_name
		while 1:
			try:
				if n[i]=='.': break
			except:
				break
			i += 1
		s = n[:i]
		return Utils.join_path(self.bld_dir(env),s)

	# returns the list of names to the node
	# make sure to reverse it (used by relpath)
	def pathlist3(self, node):
		if self is node: return ['.']
		return [self.m_name, os.sep]+self.m_parent.pathlist3(node) #TODO: fix this

	# same as pathlist3, but do not append './' at the beginning
	def pathlist4(self, node):
		if self.m_parent is node: return [self.m_name]
		return [self.m_name, os.sep]+self.m_parent.pathlist4(node) #TODO: fix this

	# path relative to a direct parent
	def relpath(self, parent):
		#print "relpath", self, parent
		#try:
		#	return Params.g_build.m_relpath_cache[self][parent]
		#except:
		#	lst=self.pathlist3(parent)
		#	lst.reverse()
		#	val=''.join(lst)

		#	try:
		#		Params.g_build.m_relpath_cache[self][parent]=val
		#	except:
		#		Params.g_build.m_relpath_cache[self]={}
		#		Params.g_build.m_relpath_cache[self][parent]=val
		#	return val
		if self is parent: return ''

		lst=self.pathlist4(parent)
		lst.reverse()
		val=''.join(lst)
		return val


	# find a common ancestor for two nodes - for the shortest path in hierarchy
	def find_ancestor(self, node):
		dist=self.height()-node.height()
		if dist<0: return node.find_ancestor(self)
		# now the real code
		cand=self
		while dist>0:
			cand=cand.m_parent
			dist=dist-1
		if cand is node: return cand
		cursor=node
		while cand.m_parent:
			cand   = cand.m_parent
			cursor = cursor.m_parent
			if cand is cursor: return cand

	# prints the amount of "../" between two nodes
	def invrelpath(self, parent):
		lst=[]
		cand=self
		while cand is not parent:
			cand=cand.m_parent
			lst+=['..',os.sep] #TODO: fix this
		return lst

	# TODO: do this in a single function (this one uses invrelpath, find_ancestor and pathlist4)
	# string representing a relative path between two nodes, we are at relative_to
	def relpath_gen(self, going_to):
		if self is going_to: return '.'
		if going_to.m_parent is self: return '..'

		# up_path is '../../../' and down_path is 'dir/subdir/subdir/file'
		ancestor  = self.find_ancestor(going_to)
		up_path   = going_to.invrelpath(ancestor)
		down_path = self.pathlist4(ancestor)
		down_path.reverse()
		return "".join( up_path+down_path )

	# TODO look at relpath_gen - it is certainly possible to get rid of find_ancestor
	def relpath_gen2(self, going_to):
		if self is going_to: return '.'
		ancestor = Params.srcnode()
		up_path   = going_to.invrelpath(ancestor)
		down_path = self.pathlist4(ancestor)
		down_path.reverse()
		return "".join( up_path+down_path )

	## ===== END relpath-related methods  ===== ##

	def debug(self):
		print "========= debug node ============="
		print "dirs are ", self.dirs()
		print "files are", self.files()
		print "======= end debug node ==========="

	def is_child_of(self, node):
		if self.m_parent:
			if self.m_parent is node: return 1
			else: return self.m_parent.is_child_of(node)
		return 0

	#def ensure_scan(self):
	#	if not self in Params.g_build.m_scanned_folders:
	#		Params.g_build.rescan(self)
	#		Params.g_build.m_scanned_folders.append(self)

	def cd_to(self, env=None):
		return self.m_parent.bldpath(env)


	def variant(self, env):
		name = self.m_name
		i = self.m_parent.get_file(name)
		if i: return 0
		return env.m_table.get('_VARIANT_', 'default')

	# =============================================== #
	# helpers for building things
	def change_ext(self, ext):
		name = self.m_name
		newname = os.path.splitext(name)[0] + ext

		n = self.m_parent.get_file(newname)
		if not n:
			n = self.m_parent.get_build(newname)
		if n: return n

		newnode = Node(newname, self.m_parent)
		self.m_parent.append_build(newnode)

		return newnode

	# =============================================== #
	# obsolete code

	def unlink(self, env=None):
		ret = os.unlink(self.abspath(env))
		print ret

	# TODO FIXME
	def get_sig(self):
		try: return Params.g_build.m_tstamp_variants[0][self]
		except: return Params.sig_nil

