import xmmsenv
import os
import os.path
import sys
import SCons
import re
import string
import new
import gittools
from marshal import dump


commithash, changed = gittools.get_info()

if changed:
	changed = " + local changes"
else:
	changed = ""

XMMS_VERSION = "0.2 DrAlban+WIP (git commit: %s%s)" % (commithash, changed)

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
        return target, source
 
subst_action = Action (subst_in_file, subst_in_file_string)
base_env['BUILDERS']['SubstInFile'] = Builder(action=subst_action, emitter=subst_emitter)

python_executable = sys.executable

b = Builder(action = python_executable + ' src/xmms/generate-converter.py > src/xmms/converter.c')
base_env.Depends('#src/xmms/converter.c', 'src/xmms/generate-converter.py')
base_env.Depends('#src/xmms/sample.c', 'src/xmms/converter.c')
base_env.SourceCode('src/xmms/converter.c', b)

subst_dict = {"%VERSION%":XMMS_VERSION, "%PLATFORM%":"XMMS_OS_" + base_env.platform.upper(), 
	      "%PKGLIBDIR%":base_env["PREFIX"]+"/lib/xmms2",
	      "%BINDIR%":base_env["PREFIX"]+"/bin",
	      "%SHAREDDIR%":base_env.sharepath,
	      "%PREFIX%":base_env.install_prefix}

config = base_env.SubstInFile("src/include/xmms/xmms_defs.h", "src/include/xmms/xmms_defs.h.in", SUBST_DICT=subst_dict)


base_env.handle_targets("Library")
base_env.handle_targets("Program")


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

### INSTALL SCRIPTS
def scan_scripts(name):
	dir = "scripts/" + name
	if os.path.isdir(dir):
		for d in os.listdir(dir):
			newf = dir+"/"+d
			if os.path.isfile(newf):
				base_env.add_script(name, newf)

scan_scripts("startup.d")
scan_scripts("shutdown.d")

### INSTALL MANUAL PAGES!

base_env.add_manpage(1, 'doc/xmms2.1')
base_env.add_manpage(8, 'doc/xmms2d.8')

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

base_env.add_shared("mind.in.a.box-lament_snipplet.ogg")
base_env.Alias('install', base_env.install_targets)

