#! /usr/bin/env python
# encoding: utf-8

# Copyright (C) 2006  Matthias Jahn <jahn.matthias@freenet.de>

import os
from optparse import OptionParser

REVISION="0.1.2"

class libtool_la_file:
	def __init__ (self, la_filename):
		self.__la_filename = la_filename
		self.linkname = str(os.path.split(la_filename)[-1])
		self.linkname = str(str(self.linkname).split(".")[0])
		if self.linkname.startswith("lib"):
			self.linkname = self.linkname[3:]
		# The name that we can dlopen(3).
		self.dlname = None
		# Names of this library
		self.library_names = None
		# The name of the static archive.
		self.old_library = None
		# Libraries that this one depends upon.
		self.dependency_libs = None
		# Version information for libIlmImf.
		self.current = None
		self.age = None
		self.revision = None
		# Is this an already installed library?
		self.installed = None
		# Should we warn about portability when linking against -modules?
		self.shouldnotlink = None
		# Files to dlopen/dlpreopen
		self.dlopen = None
		self.dlpreopen = None
		# Directory that this library needs to be installed in:
		self.libdir = '/usr/lib'
		if not self.__parse():
			raise "file %s not found!!" %(la_filename)
	
	def __parse(self):
		"Retrieve the variables from a file"
		if not os.path.isfile(self.__la_filename): return 0
		la_file=open(self.__la_filename, 'r')
		for line in la_file:
			ln = line.strip()
			if not ln: continue
			if ln[0]=='#': continue
			(key,value) = str(ln).split('=')
			value = value.strip()
			if value == "no": value = False
			if value == "yes": value = True
			line = 'self.%s = %s'%(key.strip(), value)
			exec line
		la_file.close()	
		return 1

	def get_libs(self):
		"""return linkflags for this lib"""
		libs = []
		if self.dependency_libs:
			libs = str(self.dependency_libs).strip().split()
		if libs == None:
			libs = []
		# add la lib and libdir
		libs.insert(0, "-l%s" % self.linkname.strip())
		libs.insert(0,"-L%s" % self.libdir.strip())
		return libs
	
	def __str__(self):
		return_str = ""
		return_str += "dlname = \"%s\"\n" % self.dlname
		return_str += "library_names = \"%s\"\n" % self.library_names
		return_str += "old_library = \"%s\"\n" % self.old_library
		return_str += "dependency_libs = \"%s\"\n" % self.dependency_libs
		return_str += "version = %s.%s.%s\n" %(self.current, self.age, self.revision)
		return_str += "installed = \"%s\"\n" % self.installed
		return_str += "shouldnotlink = \"%s\"\n" % self.shouldnotlink
		return_str += "dlopen = \"%s\"\n" % self.dlopen
		return_str += "dlpreopen = \"%s\"\n" % self.dlpreopen
		return_str += "libdir = \"%s\"\n" % self.libdir
		return return_str
	
	
class libtool_config:
	def __init__ (self, la_filename):
		self.__libtool_la_file = libtool_la_file(la_filename)
		tmp = self.__libtool_la_file
		self.__version = "%s.%s.%s\n" %(tmp.current, tmp.age, tmp.revision)
		self.__sub_la_files = []
		self.__sub_la_files.append(la_filename)
		self.__libs = None
		
	def __cmp__(self, other):
		"""make it compareable with X.Y.Z versions 
		(Y and Z are optional)"""
		othervers = str(other).strip().split(".")
		if not othervers:
			return 1
		othernum = 0
		selfnum = 0
		# we need a version in this formal X.Y.Z
		# add_zero would be 2 if we only had X
		# this way we need to add X.0.0
		add_zero = 3-len(othervers)
		while add_zero:
			add_zero -= 1
			othervers.append("0")
		
		for num in othervers:
			othernum = othernum + int(num)
			othernum *= 1000
		for num in str(self.__version).split("."):
			selfnum = selfnum + int(num)
			selfnum *= 1000
		
		if selfnum == othernum:
			return 0
		if selfnum > othernum:
			return 1
		if selfnum < othernum:
			return -1
		return 0
		
	def __str__(self):
		tmp = str(self.__libtool_la_file)
		tmp += str(" ").join(self.__libtool_la_file.get_libs())
		tmp += "\nNew getlibs:\n"
		tmp += str(" ").join(self.get_libs())
		return tmp
	
	def __get_la_libs(self, la_filename):
		return libtool_la_file(la_filename).get_libs()
	
	def get_libs(self):
		"""return the complete uniqe linkflags that do not 
		contain .la files anymore"""
		libs_list = list(self.__libtool_la_file.get_libs())
		libs_map = {}
		while len(libs_list) > 0:
			entry = libs_list.pop(0)
			if entry:
				if str(entry).endswith(".la"):
					## prevents duplicate .la checks
					if entry not in self.__sub_la_files:
						self.__sub_la_files.append(entry)
						libs_list.extend(self.__get_la_libs(entry))
				else:
					libs_map[entry]=1
		self.__libs = libs_map.keys()
		return self.__libs

	def get_libs_only_L(self):
		if not self.__libs:
			self.get_libs()
		libs = self.__libs
		libs = filter(lambda s: str(s).startswith('-L'), libs)
		return libs
	
	def get_libs_only_l(self):
		if not self.__libs:
			self.get_libs()
		libs = self.__libs
		libs = filter(lambda s: str(s).startswith('-l'), libs)
		return libs
		
	def get_libs_only_other(self):
		if not self.__libs:
			self.get_libs()
		libs = self.__libs
		libs = filter(lambda s: not (str(s).startswith('-L') or str(s).startswith('-l')), libs)
		return libs
		


def useCmdLine():
	"""parse cmdline args and control build"""
	usage = "Usage: %prog [options] PathToFile.la \
	\nexample: %prog --atleast-version=2.0.0 /usr/lib/libIlmImf.la \
	\nor: %prog --libs /usr/lib/libamarok.la"
	parser = OptionParser( usage )
	parser.add_option( "--version", dest = "versionNumber", 
					action = "store_true", default = False,
					help = "output version of libtool-config"
				)
	parser.add_option( "--debug", dest = "debug", 
					action = "store_true", default = False,
					help = "enable debug"
				)
	parser.add_option( "--libs", dest = "libs", 
					action = "store_true", default = False,
					help = "output all linker flags"
				)
	parser.add_option( "--libs-only-l", dest = "libs_only_l",
					action = "store_true", default = False,
					help = "output -l flags"
					)
	parser.add_option( "--libs-only-L", dest = "libs_only_L",
					action = "store_true", default = False,
					help = "output -L flags"
					)
	parser.add_option( "--libs-only-other", dest = "libs_only_other",
					action = "store_true", default = False,
					help = "output other libs (e.g. -pthread)"
					)
	parser.add_option( "--atleast-version", dest = "atleast_version", 
					default=None, 
					help = "return 0 if the module is at least version ATLEAST_VERSION"
				)
	parser.add_option( "--exact-version", dest = "exact_version", 
					default=None, 
					help = "return 0 if the module is exactly version EXACT_VERSION"
				)
	parser.add_option( "--max-version", dest = "max_version", 
					default=None, 
					help = "return 0 if the module is at no newer than version MAX_VERSION"
				)

	( options, args ) = parser.parse_args()
	if len( args ) != 1 and not options.versionNumber:
		parser.error( "incorrect number of arguments" )
	if options.versionNumber:
		print "libtool-config version %s" % REVISION
		return 0
	ltf = libtool_config(args[0])
	if options.debug:
		print(ltf)
	if options.atleast_version:
		if ltf >= options.atleast_version:
			return 0
		else:
			return 1
	if options.exact_version:
		if ltf == options.exact_version:
			return 0
		else:
			return 1
	if options.max_version:
		if ltf <= options.max_version:
			return 0
		else:
			return 1
	if options.libs:
		print str(" ").join(ltf.get_libs())
		return 0
	if options.libs_only_l:
		libs = ltf.get_libs_only_l()
		print str(" ").join(libs)
		return 0
	if options.libs_only_L:
		libs = ltf.get_libs_only_L()
		print str(" ").join(libs)
		return 0
	if options.libs_only_other:
		libs = ltf.get_libs_only_other()
		print str(" ").join(libs)
		return 0

if __name__ == "__main__":
	useCmdLine()
