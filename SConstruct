import xmmsenv
import os
import os.path
import sys
import SCons
import re
import string
import new
from marshal import dump

try:
	head = " (git-commit: "+file(".git/HEAD").read().strip()+")"
except:
	head = ""
	pass

XMMS_VERSION = "0.1 DR1.1-WIP" + head

EnsureSConsVersion(0, 96)
EnsurePythonVersion(2, 1)
SConsignFile()

def SimpleListOption(key, help, default=[]):
	return(key, help, default, None, lambda val: string.split(val))


if sys.platform == 'win32':
	default_pyrex = 'pyrexc.py'
	default_prefix = 'c:\\xmms2'
	default_cxxflags = ['/Zi', '/TC']
	default_cflags = ['/Zi', '/TC']
	default_cpppath = ['z:\\xmms2\\winlibs\\include']
else:
	default_pyrex = 'pyrexc'
	default_prefix = '/usr/local/'
	default_cxxflags = ['-g', '-Wall', '-O0']
	default_cflags = ['-g', '-Wall', '-O0']
	if sys.platform == 'darwin':
		default_cpppath = ['/sw/lib']
	else:
		default_cpppath = []

opts = Options("options.cache")
opts.Add('CC', 'C compiler to use')
opts.Add('CXX', 'C++ compiler to use')
opts.Add('LD', 'Linker to use')
opts.Add(SimpleListOption('LINKFLAGS', 'Linker flags', []))
opts.Add(SimpleListOption('LIBPATH', 'Path to libs', []))
opts.Add(SimpleListOption('CPPPATH', 'path to include files', default_cpppath))
opts.Add(SimpleListOption('CXXFLAGS', 'C++ compilerflags', default_cxxflags))
opts.Add(SimpleListOption('CCFLAGS', 'C compilerflags', default_cflags))
opts.Add('PREFIX', 'install prefix', default_prefix)
opts.Add('MANDIR', 'manual directory', '$PREFIX/man')
opts.Add('RUBYARCHDIR', 'Path to install Ruby bindings')
opts.Add('INSTALLDIR', 'install dir')
opts.Add('PKGCONFIGDIR', 'Where should we put our .pc files?', '$PREFIX/lib/pkgconfig')
opts.Add(BoolOption('SHOWCACHE', 'show what flags that lives inside cache', 0))
opts.Add(SimpleListOption('EXCLUDE', 'exclude these modules', []))
opts.Add(BoolOption('CONFIG', 'run configuration commands again', 0))

# base CCPATH
base_env = xmmsenv.XMMSEnvironment(options=opts)
base_env["CONFIG"] = 0
opts.Save("options.cache", base_env)


base_env.Append(CPPPATH=["#src/include"])

Help(opts.GenerateHelpText(base_env))

def do_subst_in_file(targetfile, sourcefile, dict):
	"""Replace all instances of the keys of dict with their values.
	For example, if dict is {'%VERSION%': '1.2345', '%BASE%': 'MyProg'},
        then all instances of %VERSION% in the file will be replaced with 1.2345 etc.
        """
        try:
            f = open(sourcefile, 'rb')
            contents = f.read()
            f.close()
        except:
            raise SCons.Errors.UserError, "Can't read source file %s"%sourcefile
        for (k,v) in dict.items():
            contents = re.sub(k, v, contents)
        try:
            f = open(targetfile, 'wb')
            f.write(contents)
            f.close()
        except:
            raise SCons.Errors.UserError, "Can't write target file %s"%targetfile
        return 0 # success
 
def subst_in_file(target, source, env):
        if not env.has_key('SUBST_DICT'):
            raise SCons.Errors.UserError, "SubstInFile requires SUBST_DICT to be set."
        d = dict(env['SUBST_DICT']) # copy it
        for (k,v) in d.items():
            if callable(v):
                d[k] = env.subst(v())
            elif SCons.Util.is_String(v):
                d[k]=env.subst(v)
            else:
                raise SCons.Errors.UserError, "SubstInFile: key %s: %s must be a string or callable"%(k, repr(v))
        for (t,s) in zip(target, source):
            return do_subst_in_file(str(t), str(s), d)
 
def subst_in_file_string(target, source, env):
        """This is what gets printed on the console."""
        return '\n'.join(['Substituting vars from %s into %s'%(str(s), str(t))
                          for (t,s) in zip(target, source)])
 
def subst_emitter(target, source, env):
        """Add dependency from substituted SUBST_DICT to target.
        Returns original target, source tuple unchanged.
        """
        d = env['SUBST_DICT'].copy() # copy it
        for (k,v) in d.items():
            if callable(v):
                d[k] = env.subst(v())
            elif SCons.Util.is_String(v):
                d[k]=env.subst(v)
        Depends(target, SCons.Node.Python.Value(d))
        # Depends(target, source) # this doesn't help the install-sapphire-linux.sh problem
        return target, source
 
subst_action = Action (subst_in_file, subst_in_file_string)
base_env['BUILDERS']['SubstInFile'] = Builder(action=subst_action, emitter=subst_emitter)

python_executable = sys.executable

b = Builder(action = python_executable + ' src/xmms/generate-converter.py > src/xmms/converter.c')
base_env.Depends('#src/xmms/converter.c', 'src/xmms/generate-converter.py')
base_env.SourceCode('src/xmms/converter.c', b)

subst_dict = {"%VERSION%":XMMS_VERSION, "%PLATFORM%":"XMMS_OS_" + base_env.platform.upper(), 
	      "%PKGLIBDIR%":base_env["PREFIX"]+"/lib/xmms2",
	      "%SHAREDDIR%":base_env.sharepath,
	      "%PREFIX%":base_env.install_prefix}

config = base_env.SubstInFile("src/include/xmms/xmms_defs.h", "src/include/xmms/xmms_defs.h.in", SUBST_DICT=subst_dict)

class Target:
	def __init__(self, target, type):
		self.dir = os.path.dirname(target)
		self.type = type

		globs = {}
		globs['platform'] = base_env.platform
		globs['ConfigError'] = xmmsenv.ConfigError

		c = compile(file(target).read(), target, "exec")
		eval(c, globs)

		if not isinstance(globs.get("target"), str):
			raise RutimeError("Target file '%s' does not specify target, or target is not a string" % target)
		if not isinstance(globs.get("source"), list):
			raise RutimeError("Target file '%s' does not specify 'source', or 'source' is not a list" % target)


		self.source = [os.path.join(self.dir, s) for s in globs["source"]]
		self.target = os.path.join(self.dir, globs["target"])
		self.config = globs.get("config")

		self.install = globs.get("install", True)
		self.static = globs.get("static", True)
		self.shared = globs.get("shared", True)
		self.systemlibrary = globs.get("systemlibrary", False)
		if globs.has_key("supported_platforms"):
			raise RuntimeError("%s uses useless supported_platforms" % target)

def scan_dir(dir, dict):
	for d in os.listdir(dir):
		if d in base_env['EXCLUDE']:
			continue
		newdir = dir+"/"+d
		if os.path.isdir(newdir):
			scan_dir(newdir, dict)
		if os.path.isfile(newdir) and newdir[-1] != "~":
			if d.startswith('Plugin') and not d.endswith("~"):
				dict["plugin"].append(newdir)
			if d.startswith('Library') and not d.endswith("~"):
				dict["library"].append(newdir)
			if d.startswith('Program') and not d.endswith("~"):
				dict["program"].append(newdir)

targets = {"plugin":[], "library":[], "program":[]}
scan_dir("src", targets)

for t in targets["plugin"]:
	base_env.targets.append(Target(t, "plugin"))
for t in targets["library"]:
	base_env.targets.append(Target(t, "library"))
for t in targets["program"]:
	base_env.targets.append(Target(t, "program"))

for t in base_env.targets:
	env = base_env.Copy()
	env.dir = t.dir

	try:
		#plugins get glib automagically..
		if t.type == "plugin":
			env.pkgconfig("glib-2.0", libs=False)
		if t.config:
			r = t.config(env)
			if not r is None:
				raise RuntimeError("%s's config returned something!" % t.dir)
	except xmmsenv.ConfigError:
		continue

	if t.type == "plugin":
		env.add_plugin(t.target, t.source)
	if t.type == "library":
		env.add_library(t.target, t.source, t.static, t.shared, t.systemlibrary, t.install)
	if t.type == "program":
		env.add_program(t.target, t.source)

try:
	dump(base_env.config_cache, open("config.cache", "wb+"))
except IOError:
	print "Could not dump config.cache!"


#### INSTALL HEADERS!
def scan_headers(name):
	dir = "src/include/" + name
	for d in os.listdir(dir):
		newf = dir+"/"+d
		if os.path.isfile(newf) and newf.endswith('.h'):
			base_env.add_header(name, newf)
			
scan_headers("xmmsc")
scan_headers("xmms")
scan_headers("xmmsclient")

#### Generate pc files.

pc_files = [{"name": "xmms2-plugin", "lib":""}, 
	    {"name":"xmms2-client", "lib":"-lxmmsclient"},
	    {"name":"xmms2-client-glib", "lib":"-lxmmsclient-glib"},
	    {"name":"xmms2-client-ecore", "lib":"-lxmmsclient-ecore"}]

for p in pc_files:
	d = subst_dict.copy()
	d["%NAME%"] = p["name"]
	d["%LIB%"] = p["lib"]
	pc = base_env.SubstInFile(p["name"]+".pc", "xmms2.pc.in", SUBST_DICT=d)
	base_env.Install("$PKGCONFIGDIR", p["name"]+".pc")


print "====================================="
print " Configuration printout"
print "====================================="
print "Enabled plugins:",
foo = []
map(lambda x: foo.append(x[x.rindex("/")+1:]), base_env.plugins)
print ", ".join(foo)

base_env.add_shared("dismantled-the_swarm_clip.ogg")
base_env.Alias('install', base_env.install_targets)

