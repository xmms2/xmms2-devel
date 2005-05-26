import xmmsenv
import os
import os.path
import sys
import SCons
import re
import string
from marshal import dump

EnsureSConsVersion(0, 96)
EnsurePythonVersion(2, 1)
SConsignFile()

def SimpleListOption(key, help, default=[]):
	return(key, help, default, None, lambda val: string.split(val))


opts = Options("options.cache")
opts.Add('PYREX', 'PyREX compiler', 'pyrexc')
opts.Add('CC', 'C compiler to use', 'gcc')
opts.Add('CXX', 'C++ compiler to use', 'g++')
opts.Add('LD', 'Linker to use', 'ld')
opts.Add(SimpleListOption('LINKFLAGS', 'Linker flags', []))
opts.Add(SimpleListOption('LIBPATH', 'Path to libs', ['/sw/lib']))
opts.Add(SimpleListOption('CXXFLAGS', 'C++ compilerflags', ['-g', '-Wall', '-O0']))
opts.Add(SimpleListOption('CCFLAGS', 'C compilerflags', ['-g', '-Wall', '-O0']))
opts.Add(SimpleListOption('CPPPATH', 'path to include files', []))
opts.Add('PREFIX', 'install prefix', '/usr/local')
opts.Add('MANDIR', 'manual directory', '$PREFIX/man')
opts.Add('RUBYARCHDIR', 'Path to install Ruby bindings')
opts.Add('INSTALLDIR', 'install dir')
opts.Add('PKGCONFIGDIR', 'Where should we put our .pc files?', '$PREFIX/lib/pkgconfig')
opts.Add(BoolOption('SHOWCACHE', 'show what flags that lives inside cache', 0))
opts.Add(BoolOption('CONFIG', 'run configuration commands again', 0))
opts.Add(SimpleListOption('EXCLUDE', 'exclude these modules', []))

# base CCPATH

base_env = xmmsenv.XMMSEnvironment(options=opts)
opts.Save("options.cache", base_env)

base_env.Append(CPPPATH=["#src/include"])
base_env.pkgconfig("sqlite3", fail=True, libs=False)
base_env.pkgconfig("glib-2.0", fail=True, libs=False)
base_env["LIBS"]=[]

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

b = Builder(action = 'python src/xmms/generate-converter.py > src/xmms/converter.c')
base_env.Depends('#src/xmms/converter.c', 'src/xmms/generate-converter.py')
base_env.SourceCode('src/xmms/converter.c', b)

subst_dict = {"%VERSION%":"0.1 DR1.1", "%PLATFORM%":"XMMS_OS_" + base_env.platform.upper(), 
	      "%PKGLIBDIR%":base_env["PREFIX"]+"/lib/xmms2",
	      "%SHAREDDIR%":base_env.sharepath,
	      "%PREFIX%":base_env.install_prefix}

config = base_env.SubstInFile("src/include/xmms/xmms_defs.h", "src/include/xmms/xmms_defs.h.in", SUBST_DICT=subst_dict)

class Target:
	def __init__(self, target, type):
		self.dir = os.path.dirname(target)
		self.type = type
		self.static = True
		self.shared = True
		self.install = True
		self.systemlibrary = False
		my_global = {}
		f = file(target)
		source = f.read()
		c = compile(source, target, "exec")
		eval(c, my_global)
		if my_global["target"] and my_global["source"] and my_global["config"]:
			self.source = [os.path.join(self.dir, s) for s in my_global["source"]]
			self.target = os.path.join(self.dir, my_global["target"])
			self.config = my_global["config"]
			if my_global.has_key("install"):
				self.install = my_global["install"]
			if my_global.has_key("static"):
				self.static = my_global["static"]
			if my_global.has_key("shared"):
				self.shared = my_global["shared"]
			if my_global.has_key("systemlibrary"):
				self.systemlibrary = my_global["systemlibrary"]
		else:
			raise RutimeError("Wrong file %s passed to Target!" % target)

def scan_dir(dir, dict):
	for d in os.listdir(dir):
		if d in base_env['EXCLUDE']:
			continue
		newdir = dir+"/"+d
		if os.path.isdir(newdir):
			scan_dir(newdir, dict)
		if os.path.isfile(newdir) and newdir[-1] != "~":
			if d.startswith('Plugin'):
				dict["plugin"].append(newdir)
			if d.startswith('Library'):
				dict["library"].append(newdir)
			if d.startswith('Program'):
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
	if t.config(env):
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

