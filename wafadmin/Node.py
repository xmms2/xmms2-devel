#! /usr/bin/env python
# encoding: utf-8
# Thomas Nagy, 2005 (ita)

"""
Node: filesystem structure, contains lists of nodes
self.m_dirs  : sub-folders
self.m_files : files existing in the src dir
self.m_build : nodes produced in the build dirs

A folder is represented by exactly one node

IMPORTANT:
Some would-be class properties are stored in Build: nodes to depend on, signature, flags, ..
In fact, unused class members increase the .wafpickle file size sensibly with lots of objects
eg: the m_tstamp is used for every node, while the signature is computed only for build files

the build is launched from the top of the build dir (for example, in _build_/)
"""

import os
import Params, Utils
from Params import debug, error, fatal

g_launch_node=None

class Node:
	def __init__(self, name, parent):
		self.m_name = name
		self.m_parent = parent
		self.m_cached_path = ""

		# Lookup dictionaries for O(1) access
		self.m_dirs_lookup = {}
		self.m_files_lookup = {}
		self.m_build_lookup = {}

		# The checks below could be disabled for speed, if necessary
		# TODO check for . .. / \ in name

		# Node name must contain only one level
		if Utils.split_path(name)[0] != name:
			fatal('name forbidden '+name)

		if parent:
			if parent.get_file(name):
				fatal('node %s exists in the parent files %s already' % (name, str(parent)))

			if parent.get_build(name):
				fatal('node %s exists in the parent build %s already' % (name, str(parent)))

	def __str__(self):
		if self.m_name in self.m_parent.m_files_lookup: isbld = ""
		else: isbld = "b:"
		return "<%s%s>" % (isbld, self.abspath())

	def __repr__(self):
		if self.m_name in self.m_parent.m_files_lookup: isbld = ""
		else: isbld = "b:"
		return "<%s%s>" % (isbld, self.abspath())

	def dirs(self):
		return self.m_dirs_lookup.values()

	def get_dir(self,name,default=None):
		return self.m_dirs_lookup.get(name,default)

	def append_dir(self, dir):
		self.m_dirs_lookup[dir.m_name]=dir

	def files(self):
		return self.m_files_lookup.values()

	def get_file(self,name,default=None):
		return self.m_files_lookup.get(name,default)

	def append_file(self, dir):
		self.m_files_lookup[dir.m_name]=dir

	def get_build(self,name,default=None):
		return self.m_build_lookup.get(name,default)

	# for the build variants, the same nodes are used to save memory
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

	## ===== BEGIN find methods	===== ##

	def find_build(self, path, create=0):
		#print "find build", path
		lst = Utils.split_path(path)
		return self.find_build_lst(lst)

	def find_build_lst(self, lst, create=0):
		"search a source or a build node in the filesystem, rescan intermediate folders, create if necessary"
		rescan = Params.g_build.rescan
		current = self
		while lst:
			rescan(current)
			name= lst[0]
			lst = lst[1:]
			prev = current

			if name == '.':
				continue
			elif name == '..':
				current = current.m_parent
				continue
			if lst:
				current = prev.m_dirs_lookup.get(name, None)
				if not current and create:
					current = Node(name, prev)
					prev.m_dirs_lookup[name] = current
			else:
				current = prev.m_build_lookup.get(name, None)
				# next line for finding source files too
				if not current: current = prev.m_files_lookup.get(name, None)
				# TODO do not use this for finding folders
				if not current:
					current = Node(name, prev)
					# last item is the build file (rescan would have found the source)
					prev.m_build_lookup[name] = current
		return current

	def find_source(self, path, create=1):
		lst = Utils.split_path(path)
		return self.find_source_lst(lst, create)

	def find_source_lst(self, lst, create=1):
		"search a source in the filesystem, rescan intermediate folders, create intermediate folders if necessary"
		rescan = Params.g_build.rescan
		current = self
		while lst:
			rescan(current)
			name= lst[0]
			lst = lst[1:]
			prev = current

			if name == '.':
				continue
			elif name == '..':
				current = current.m_parent
				continue
			if lst:
				current = prev.m_dirs_lookup.get(name, None)
				if not current and create:
					# create a directory
					current = Node(name, prev)
					prev.m_dirs_lookup[name] = current
			else:
				current = prev.m_files_lookup.get(name, None)
				# try hard to find something
				if not current and lst: current = prev.m_dirs_lookup.get(name, None)
				if not current: current = prev.m_build_lookup.get(name, None)
			if not current: return None
		return current

	def find_raw(self, path):
		lst = Utils.split_path(path)
		return self.find_raw_lst(lst)

	def find_raw_lst(self, lst):
		"just find a node in the tree, do not rescan folders"
		current = self
		while lst:
			name=lst[0]
			lst=lst[1:]
			prev = current
			if name == '.':
				continue
			elif name == '..':
				current = current.m_parent
				continue
			current = prev.m_dirs_lookup[name]
			if not current: current=prev.m_files_lookup[name]
			if not current: current=prev.m_build_lookup[name]
			if not current: return None
		return current

	def ensure_node_from_lst(self, plst):
		curnode=self
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
				found = Node(dirname, curnode)
				curnode.append_dir(found)
			curnode = found
		return curnode

	def find_dir(self, path):
		lst = Utils.split_path(path)
		return self.find_dir_lst(lst)

	def find_dir_lst(self, lst):
		"search a folder in the filesystem, do not scan, create if necessary"
		current = self
		while lst:
			name= lst[0]
			lst = lst[1:]
			prev = current

			if name == '.':
				continue
			elif name == '..':
				current = current.m_parent
			else:
				current = prev.m_dirs_lookup.get(name, None)
				if not current:
					current = Node(name, prev)
					# create a directory
					prev.m_dirs_lookup[name] = current
		return current


	## ===== END find methods	===== ##


	## ===== BEGIN relpath-related methods	===== ##

	# same as pathlist3, but do not append './' at the beginning
	def pathlist4(self, node):
		#print "pathlist4 called"
		if self.m_parent is node: return [self.m_name]
		return [self.m_name, os.sep]+self.m_parent.pathlist4(node)

	def relpath(self, parent):
		"path relative to a direct parent, as string"
		lst=[]
		p=self
		h1=parent.height()
		h2=p.height()
		while h2>h1:
			h2-=1
			lst.append(p.m_name)
			p=p.m_parent
		if lst:
			lst.reverse()
			ret=os.path.join(*lst)
		else:
			ret=''
		return ret

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





	def nice_path(self, env=None):
		"printed in the console, open files easily from the launch directory"
		tree = Params.g_build
		global g_launch_node
		if not g_launch_node:
			g_launch_node = tree.m_root.find_dir(Params.g_cwd_launch)

		name = self.m_name
		x = self.m_parent.get_file(name)
		if x: return self.relative_path(g_launch_node)
		else: return os.path.join(tree.m_bldnode.relative_path(g_launch_node), env.variant(), self.relative_path(tree.m_srcnode))

	def relative_path(self, folder):
		"relative path between a node and a directory node"
		hh1 = h1 = self.height()
		hh2 = h2 = folder.height()
		p1=self
		p2=folder
		while h1>h2:
			p1=p1.m_parent
			h1-=1
		while h2>h1:
			p2=p2.m_parent
			h2-=1

		# now we have two nodes of the same height
		ancestor = None
		if p1.m_name == p2.m_name:
			ancestor = p1
		while p1.m_parent:
			p1=p1.m_parent
			p2=p2.m_parent
			if p1.m_name != p2.m_name:
				ancestor = None
			elif not ancestor:
				ancestor = p1

		anh = ancestor.height()
		n1 = hh1-anh
		n2 = hh2-anh

		lst=[]
		tmp = self
		while n1:
			n1 -= 1
			lst.append(tmp.m_name)
			tmp = tmp.m_parent

		lst.reverse()
		up_path=os.sep.join(lst)
		down_path = (".."+os.sep) * n2

		return "".join(down_path+up_path)

	## ===== END relpath-related methods  ===== ##

	def debug(self):
		print "========= debug node ============="
		print "dirs are ", self.dirs()
		print "files are", self.files()
		print "======= end debug node ==========="

	def is_child_of(self, node):
		"does this node belong to the subtree node"
		p=self
		h1=self.height()
		h2=node.height()
		diff=h1-h2
		while diff>0:
			diff-=1
			p=p.m_parent
		return p.equals(node)

	def equals(self, node):
		"is one node equal to another one"
		p1 = self
		p2 = node
		while p1 and p2:
			if p1.m_name != p2.m_name:
				return 0
			p1=p1.m_parent
			p2=p2.m_parent
		if p1 or p2: return 0
		return 1

	def variant(self, env):
		"variant, or output directory for this node, a source has for variant 0"
		if not env: return 0
		i = self.m_parent.get_file(self.m_name)
		if i: return 0
		return env.variant()

	def size_subtree(self):
		"for debugging, returns the amount of subnodes"
		l_size=1
		for i in self.dirs(): l_size += i.size_subtree()
		l_size += len(self.files())
		return l_size

	def height(self):
		"amount of parents"
		# README a cache can be added here if necessary
		d = self
		val = 0
		while d.m_parent:
			d=d.m_parent
			val += 1
		return val

	# helpers for building things

	def abspath(self, env=None):
		"absolute path"
		variant = self.variant(env)
		try:
			ret= Params.g_build.m_abspath_cache[variant][self]
			return ret
		except KeyError:
			if not variant:
				cur=self
				lst=[]
				while cur:
					lst.append(cur.m_name)
					cur=cur.m_parent
				lst.reverse()
				val=os.path.join(*lst)
			else:
				val=os.path.join(Params.g_build.m_bldnode.abspath(),env.variant(),
					self.relpath(Params.g_build.m_srcnode))
			Params.g_build.m_abspath_cache[variant][self]=val
			return val

	def change_ext(self, ext):
		"node of the same path, but with a different extension"
		name = self.m_name
		k = name.rfind('.')
		newname = name[:k]+ext

		p = self.m_parent
		n = p.m_files_lookup.get(newname, None)
		if not n: n = p.m_build_lookup.get(newname, None)
		if n: return n

		newnode = Node(newname, p)
		p.m_build_lookup[newnode.m_name]=newnode

		return newnode

	def bld_dir(self, env):
		"build path without the file name"
		return self.m_parent.bldpath(env)

	def bldbase(self, env):
		"build path without the extension: src/dir/foo(.cpp)"
		l = len(self.m_name)
		n = self.m_name
		while l>0:
			l -= 1
			if n[l]=='.': break
		s = n[:l]
		return Utils.join_path(self.bld_dir(env),s)

	def bldpath(self, env=None):
		"path seen from the build dir default/src/foo.cpp"
		x = self.m_parent.get_file(self.m_name)
		if x: return self.relpath_gen(Params.g_build.m_bldnode)
		return Utils.join_path(env.variant(), self.relpath(Params.g_build.m_srcnode))

	def srcpath(self, env):
		"path in the srcdir from the build dir ../src/foo.cpp"
		x = self.m_parent.get_build(self.m_name)
		if x: return self.bldpath(env)
		return self.relpath_gen(Params.g_build.m_bldnode)


