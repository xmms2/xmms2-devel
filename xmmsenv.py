from SCons.Environment import Environment
import SCons
import sys, os
import shutil
from marshal import load
from stat import *

def installFunc(dest, source, env):
	"""Copy file, setting sane permissions"""
	
	shutil.copy(source, dest)
	st = os.stat(source)
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
	if st[ST_MODE] & S_IXUSR:
		mode |= S_IXUSR | S_IXGRP | S_IXOTH
	os.chmod(dest, mode)
	return 0

class XMMSEnvironment(Environment):
	targets=[]
	def __init__(self, parent=None, options=None, **kw):
		Environment.__init__(self, options=options)
		apply(self.Replace, (), kw)
		self.conf = SCons.SConf.SConf(self)

		if os.path.isfile("config.cache") and self["CONFIG"] == 0:
			try:
				self.config_cache=load(open("config.cache", 'rb+'))
			except IOError:
				print "Could not load config.cache!"
				self.config_cache={}
		else:
			self.config_cache={}

		self.plugins=[]
		self.libs=[]
		self.programs=[]
		self.install_targets=[]

		if self.has_key("INSTALLDIR"):
			self.installdir = os.path.normpath(self["INSTALLDIR"] + '/')
		else:
			self.installdir = ""
		self["INSTALL"] = installFunc

		self.install_prefix = self["PREFIX"]
		self["MANDIR"] = self["MANDIR"].replace("$PREFIX", self.install_prefix)
		self.pluginpath = os.path.join(self.install_prefix, "lib/xmms2")
		self.binpath = os.path.join(self.install_prefix, "bin")
		self.librarypath = os.path.join(self.install_prefix, "lib")
		self.sharepath = os.path.join(self.install_prefix, "share/xmms2")
		self["SHLIBPREFIX"] = "lib"

		if sys.platform == 'linux2':
			self.platform = 'linux'
		elif sys.platform.startswith("freebsd"):
			self.platform = 'freebsd'
		else:
			self.platform = sys.platform
			
		if self.platform == 'darwin':
			self["SHLINKFLAGS"] = "$LINKFLAGS -multiply_defined suppress -flat_namespace -undefined suppress"
	
	def Install(self, target, source):
		target = os.path.normpath(self.installdir + target)
		SCons.Environment.Environment.Install(self, target, source)
		self.install_targets.append(target)
			
	def tryaction(self, cmd):
		if self.config_cache.has_key(cmd):
			return self.config_cache[cmd]

		r = False

		try:
			buf = os.popen(cmd).read()
			if not buf == '':
				r = True
		except:
			pass

		self.config_cache[cmd] = r

		return r

	def run(self, cmd):
		if self.config_cache.has_key(cmd):
			return self.config_cache[cmd]

		print "Running", cmd

		try:
			r = os.popen(cmd).read()
		except:
			r = None

		self.config_cache[cmd] = r
		return r
	
	def pkgconfig(self, module, fail=False, headers=True, libs=True):
		cmd = "pkg-config --silence-errors"
		if headers:
			cmd += " --cflags"
		if libs:
			cmd += " --libs" 
		cmd += " " + module
		return self.configcmd(cmd, fail)

	def configcmd(self, cmd, fail=False):
		if self.config_cache.has_key(cmd):
			ret = self.config_cache[cmd]
		else:
			print "Running %s" % cmd
			ret = os.popen(cmd).read()
			self.config_cache[cmd] = ret

		if ret == '':
			if fail:
				print "Could not find needed group %s!!! Aborting!" % cmd
				sys.exit(-1)
			return False
		ret = ret.strip()
		self.parse_config_string(ret)

		return True

	def checklibs(self, lib, func, fail=False):
		if self.config_cache.has_key((lib,func)):
			ret = self.config_cache[(lib,func)]
		else:
			ret = self.conf.CheckLib(lib, func, 0)
			self.config_cache[(lib,func)] = ret
			
		if not ret:
			if fail:
				print "Could not find needed group %s!!! Aborting!" % cmd
				sys.exit(-1)
			return False

		self.parse_config_string("-l"+lib)
		return True

	def parse_config_string(self, flags):
		"""We want our own ParseConfig, that supports some more
		flags, and that takes the argument as a string"""

		params = self.Split(flags)
		i = 0
		while( i < len( params ) ):
			arg = params[i]
			switch = arg[0:1]
			opt = arg[1:2]

			if switch == '-':
				if opt == 'L':
					self.Append( LIBPATH = [ arg[2:] ] )
				elif opt == 'l':
					self.Append( LIBS = [ arg[2:] ] )
				elif opt == 'I':
					self.Append( CPPPATH = [ arg[2:] ] )
				elif opt == 'D':
					self.Append( CPPFLAGS = [ arg ] )
				elif arg[1:] == 'pthread':
					self.Append( LINKFLAGS = [ arg ] )
					self.Append( CPPFLAGS = [ arg ] )
				elif arg[1:6] == 'rpath':
					self.Append( LINKFLAGS = [ arg ] )
				elif arg[1:4] == 'Wl,':
					self.Append( LINKFLAGS = [ arg ] )
				elif arg[1:] == 'framework':
					self.Append( LINKFLAGS = [ arg ] )
					self.Append( LINKFLAGS = [ params[i+1] ] )
					i = i + 1
				elif arg[1:] == 'z':
					self.Append( LINKFLAGS = [ arg ] )
					self.Append( LINKFLAGS = [ params[i+1] ] )
					i = i + 1
				elif arg[1:5] == 'arch':
					i = i + 1
				elif arg[1:] == 'ObjC':
					self.Append( CPPFLAGS = [ arg ] )
				elif opt == 'm':
					pass
				else:
					break
			elif arg[:3] == 'yes' :
				i = i + 3
				pass

			i = i + 1

	def add_plugin(self, target, source):
		self.plugins.append(target)
		self["SHLIBPREFIX"]="libxmms_"
		if self.platform == 'darwin':
			self["SHLINKFLAGS"] += " -bundle"
		self.SharedLibrary(target, source)
		self.Install(self.pluginpath, self.dir+"/"+self["SHLIBPREFIX"]+target[target.rindex("/")+1:]+self["SHLIBSUFFIX"])

	def add_library(self, target, source, static=True, shared=True):
		self.libs.append(target)
		if static:
			self.Library(target, source)
			self.Install(self.librarypath, self.dir+"/"+self["LIBPREFIX"]+target[target.rindex("/")+1:]+self["LIBSUFFIX"])
		if shared:
			self.SharedLibrary(target, source)
			if self.platform == 'darwin':
				self["SHLINKFLAGS"] += " -dynamiclib"
			self.Install(self.librarypath, self.dir+"/"+self["SHLIBPREFIX"]+target[target.rindex("/")+1:]+self["SHLIBSUFFIX"])


	def add_program(self, target, source):
		self.programs.append(target)
		self.Program(target, source)
		self.Install(self.binpath, target)

	def add_shared(self, source):
		self.Install(self.sharepath, source)

