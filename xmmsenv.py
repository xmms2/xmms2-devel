from SCons.Environment import Environment
import SCons
import sys, os
import shutil
import gzip
from marshal import load
from stat import *
import operator

global_libpaths = ["/lib", "/usr/lib"]

class ConfigError(Exception):
	pass

any = lambda x: reduce(operator.or_, x)

def installFunc(dest, source, env):
	"""Copy file, setting sane permissions"""
	
	if os.path.islink(source):
		os.symlink(os.readlink(source), dest)
	else:
		shutil.copy(source, dest)
		st = os.stat(source)
		mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
		if st[ST_MODE] & S_IXUSR:
			mode |= S_IXUSR | S_IXGRP | S_IXOTH
		os.chmod(dest, mode)
	return 0



class Target:
	def __init__(self, target, env):
		self.dir = os.path.dirname(target)

		self.globs = {}
		self.globs['platform'] = env.platform
		self.globs['ConfigError'] = ConfigError

		c = compile(file(target).read(), target, "exec")
		eval(c, self.globs)

		if not isinstance(self.globs.get("target"), str):
			raise RuntimeError("Target file '%s' does not specify target, or target is not a string" % target)
		if not isinstance(self.globs.get("source"), list):
			raise RuntimeError("Target file '%s' does not specify 'source', or 'source' is not a list" % target)


		self.source = [os.path.join(self.dir, s) for s in self.globs["source"]]
		self.target = os.path.join(self.dir, self.globs["target"])

	def config(self, env):
		self.globs.get("config", lambda x: None)(env)

class LibraryTarget(Target):
	def add(self, env):
		install = self.globs.get("install", True)
		static = self.globs.get("static", True)
		shared = self.globs.get("shared", True)
		systemlibrary = self.globs.get("systemlibrary", False)

		env.add_library(self.target, self.source, static, shared, systemlibrary, install)

class ProgramTarget(Target):
	def add(self, env):
		env.add_program(self.target, self.source)

class PluginTarget(Target):
	def config(self, env):
		env.pkgconfig("glib-2.0", fail=False, libs=False)
		Target.config(self, env)
	def add(self, env):
		env.add_plugin(self.target, self.source)

class XMMSEnvironment(Environment):
	targets=[]
	def __init__(self, parent=None, options=None, **kw):
		reconfigure = self.options_changed(options, ['INSTALLPATH'])
		Environment.__init__(self, options=options, ENV=os.environ)
		apply(self.Replace, (), kw)
		self.conf = SCons.SConf.SConf(self)

		if os.path.isfile("config.cache") and self["CONFIG"] == 0 and not reconfigure:
			try:
				self.config_cache=load(open("config.cache", 'rb+'))
			except:
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
		self.manpath = self["MANDIR"].replace("$PREFIX", self.install_prefix)
		self.pluginpath = os.path.join(self.install_prefix, "lib/xmms2")
		self.binpath = os.path.join(self.install_prefix, "bin")
		self.librarypath = os.path.join(self.install_prefix, "lib")
		self.sharepath = os.path.join(self.install_prefix, "share/xmms2")
		self.includepath = os.path.join(self.install_prefix, "include/xmms2")
		self.scriptpath = os.path.join(self.sharepath, "scripts")
		self["SHLIBPREFIX"] = "lib"
		self.shversion = "0"

		if sys.platform == 'linux2':
			self.platform = 'linux'
		elif sys.platform.startswith("freebsd"):
			self.platform = 'freebsd'
		elif sys.platform.startswith("openbsd"):
			self.platform = 'openbsd'
		elif sys.platform.startswith("netbsd"):
			self.platform = 'netbsd'
		else:
			self.platform = sys.platform
			
		if self.platform == 'darwin':
			self["SHLINKFLAGS"] = "$LINKFLAGS -multiply_defined suppress -flat_namespace -undefined suppress"

		self.potential_targets = []
		self.scan_dir("src")

	
	def Install(self, target, source):
		target = os.path.normpath(self.installdir + target)
		SCons.Environment.Environment.Install(self, target, source)
		self.install_targets.append(target)
			
	def tryaction(self, cmd):
		if self.config_cache.has_key(cmd):
			return self.config_cache[cmd]

		r = False

		# Execute command and obtain returncode from process
		# If ret is None the process exited with returncode 0
		ret = os.popen(cmd).close()
		if ret:
			statuscode = ret >> 8
		else:
			statuscode = 0

		if statuscode == 0:
			r = True

		self.config_cache[cmd] = r

		return r

	def run(self, cmd):
		if self.config_cache.has_key(cmd):
			return self.config_cache[cmd]

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
		self.configcmd(cmd, fail)
		

	def configcmd(self, cmd, fail=False):
		if self.config_cache.has_key(cmd):
			ret = self.config_cache[cmd]
		else:
			ret = os.popen(cmd).read()
			self.config_cache[cmd] = ret

		if ret == '':
			if fail:
				print "Could not find needed group %s!!! Aborting!" % cmd
				sys.exit(-1)
			raise ConfigError("Command '%s' failed" % cmd)
		ret = ret.strip()

		self.parse_config_string(ret)

	def checkheader(self, header, fail=False):
		key = ("HEADER", header)
		if not self.config_cache.has_key(key):
			self.config_cache[key] = self.conf.CheckCHeader(header)
		if not self.config_cache[key]:
			if fail:
				print "Aborting!"
				sys.exit(1)
			raise ConfigError("Headerfile '%s' not found" % header)

	def checklib(self, lib, func, fail=False):
		key = (lib, func)

		if not self.config_cache.has_key(key):
			#libtool_flags = None

			self.config_cache[key] = ""

			#for d in global_libpaths+self["LIBPATH"]:
			#	la = "%s/lib%s.la" % (d, lib)
			#	if os.path.isfile(la):
			#		print "found a libtoolfile", la
			#		libtool_flags = self.parse_libtool(la)
			#		self.parse_config_string(libtool_flags["dependency_libs"])
			#		self.config_cache[key] = libtool_flags["dependency_libs"]+" "
			#		break

			if self.conf.CheckLib(lib, func, 0):
				self.config_cache[key] += "-l"+lib
				self.parse_config_string("-l"+lib)
				return
			else:
				self.config_cache[key] = None

		if not self.config_cache[key]:
			if fail:
				print "Aborting!"
				sys.exit(1)
			raise ConfigError("Symbol '%s' in library '%s' not found" % (func, lib))

		self.parse_config_string(self.config_cache[key])

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
			elif arg[-3:] == '.la':
				la = self.parse_libtool(arg)
				lib = la["dlname"]
				if lib[:3] == 'lib':
					lib = lib[3:]
				lib = lib[:lib.index(".")]
				self.parse_config_string(la["dependency_libs"])
				self.parse_config_string("-l"+lib)

			i = i + 1

	def libname(self, target):
		return self["LIBPREFIX"] + os.path.basename(target) + self["LIBSUFFIX"]
	
	def shlibname(self, target):
		return self["SHLIBPREFIX"] + os.path.basename(target) + self["SHLIBSUFFIX"]

	def add_plugin(self, target, source):
		self.plugins.append(target)
		self["SHLIBPREFIX"]="libxmms_"
		if self.platform == 'darwin':
			self["SHLINKFLAGS"] += " -bundle"
		self.SharedLibrary(target, source)
		self.Install(self.pluginpath, os.path.join(self.dir, self.shlibname(target)))

	def add_library(self, target, source, static=True, shared=True, system=False, install=True):

		self.libs.append(target)
		if static:
			self.Library(target, source)
			if install:
				self.Install(self.librarypath, os.path.join(self.dir, self.libname(target)))
		if shared:
			if (self.platform in ["linux"]) and system:
				# Append the version string to the library and
				# create a symlink to it.
				shlib_unversioned = self.shlibname(target)
				shlibpath_unversioned = os.path.join(self.dir, shlib_unversioned)
				self["SHLIBSUFFIX"] += "." + self.shversion
				shlib = self.shlibname(target)
				shlibpath = os.path.join(self.dir, shlib)
				self.Command(shlibpath_unversioned, shlibpath, 
				             "ln -s %s %s" % (shlib, shlibpath_unversioned))
				if install:
					self.Install(self.librarypath, shlibpath_unversioned)
			if system:
				if self.platform == 'linux' or self.platform == 'freebsd':
					self["SHLINKFLAGS"] += " -Wl,-soname," + self.shlibname(target)
			self.SharedLibrary(target, source)
			if self.platform == 'darwin':
				self["SHLINKFLAGS"] += " -dynamiclib"
			if install:
				self.Install(self.librarypath, os.path.join(self.dir, self.shlibname(target)))


	def add_program(self, target, source):
		self.programs.append(target)
		self.Program(target, source)
		self.Install(self.binpath, target)

	def add_shared(self, source):
		self.Install(self.sharepath, source)

	def add_header(self, target, source):
		self.Install(os.path.join(self.includepath,target), source)

        def add_manpage(self, section, source):
                gzip.GzipFile(source+".gz", 'wb',9).write(file(source).read())
                self.Install(os.path.join(self.manpath, "man"+str(section)), source+'.gz')

	def add_script(self, target, source):
		self.Install(os.path.join(self.scriptpath,target), source)

	def options_changed(self, options, exclude=[]):
		"""NOTE: This method does not catch changed defaults."""
		cached = {}
		if options.files:
			for filename in options.files:
				if os.path.exists(filename):
					execfile(filename, cached)
		else:
			return False
	
		for option in options.options:
			if option.key in exclude: continue
			if options.args.has_key(option.key):
				if cached.has_key(option.key):
					if options.args[option.key] != cached[option.key]:
						# differnt value
						return True
				else:
					# previously unspecified option
					if options.args[option.key] != option.default:
						# that is different from the default
						return True

		return False

	def scan_dir(self, dir):
		for d in os.listdir(dir):
			if d in self['EXCLUDE']:
				continue
			if any([d.endswith(end) for end in ["~",".rej",".orig"]]):
				continue
			newdir = os.path.join(dir,d)
			if os.path.isdir(newdir):
				self.scan_dir(newdir)
			elif d[0].isupper():
				self.potential_targets.append((d, newdir))


	def parse_libtool(self, libtoolfile):
		""" 
		This will open the libtool file and read the lines
		that we need.
		"""
		f = file(libtoolfile)
		line = f.readline()
		ret = {}
		while line:
			if '=' in line:
				s = line.split("=")
				if len(s) == 2:
					ret[s[0]] = s[1].replace("'", "").strip()
			line = f.readline()

		return ret

	def handle_targets(self, targettype):
		cls = eval(targettype+"Target")
		targets = [cls(a[1], self) for a in self.potential_targets if a[0].startswith(targettype)]

		for t in targets:
			env = self.Copy()
			env.dir = t.dir

			try:
				t.config(env)
				t.add(env)
			except ConfigError, m:
				self.conf.logstream.write("xmmsscons: File %s reported error '%s' and was disabled.\n" % (t.target, m))
				continue
