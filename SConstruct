import os;
import sys;
import xmmsenv;
import SCons
from marshal import dump, load;
from xmmsconf import checkFlags, showOpts;

# Ok, hope that scons versionnumbers only will contain one dot...
if int (SCons.__version__.replace (".","")) < 94 :
	print "You have too old scons version. 0.94 is required. You have:", SCons.__version__
	sys.exit ()

Help ("""XMMS2 SCons help
Build XMMS2 by running:
	
	scons

Clean up the builddir by running:

	scons -c
""")


##
## Get options
##
opts = Options(None, ARGUMENTS)
opts.Add('PYREX', 'PyREX compiler', 'pyrexc')
opts.Add('CC', 'C compiler to use', 'gcc')
opts.Add('CXX', 'C++ compiler to use', 'g++')
opts.Add('LD', 'Linker to use', 'ld')
opts.Add('LDFLAGS', 'Linker to use', '-L/sw/lib')
opts.Add('CXXFLAGS', 'C++ compilerflags', '-g -Wall -O0')
opts.Add('CCFLAGS', 'C compilerflags', '-g -Wall -O0')
opts.Add('PREFIX', 'installprefix', '/usr/local')
opts.Add('SYSCONFDIR', 'system configuration dir', '/usr/local/etc')
opts.Add('INSTALLDIR', 'runtime install dir', '')
opts.Add('MANDIR', 'manual directory', '/usr/local/man')
opts.Add('SHOWCACHE', 'show what flags that lives inside cache', 0)
opts.Add('NOCACHE', 'do not use cache', 0)
opts.Add('EXCLUDE', 'exclude this modules', '')

## setup base environment...
## ...ok, this should be a bit configurable... later.
##
## SCons-tips 42: start paths with '#' to have them change
##                correctly when we descend into subdirs
base_env = xmmsenv.XmmsEnvironment(options=opts, LINK="gcc", CPPPATH = ['#src'])

#check endian
import struct;
if struct.pack ("@h", 1)[0] == '\x01' :
	endian = "-DWORDS_LITTLEENDIAN"
else :
	endian = "-DWORDS_BIGENDIAN"

base_env['CCFLAGS'] = base_env['CCFLAGS'] + " " + endian

if base_env['NOCACHE']:
	print "We don't want any cache"
	checkFlags(base_env)
	showOpts (base_env)
	sys.exit ()
else:
	try:
		statefile = open('scons.cache','rb+')
		dict = load(statefile)
		base_env.flag_groups = dict["flag_groups"]
		base_env.optional_config = dict["optional_config"]
		statefile.close()
		print "Cachefile scons.cache found, not checking libs"
		if base_env['SHOWCACHE']:
			for x in base_env.flag_groups.keys():
				print "Module " + x + " has flags"
				if base_env['SHOWCACHE'] == "2":
					print "\t" + base_env.flag_groups[x]
			sys.exit ()
	except IOError:
		print "No cachefile"
		checkFlags(base_env)
		showOpts (base_env)
		sys.exit ()

Export('base_env')

SConscript('src/xmms/SConscript',build_dir='builddir/xmms',duplicate=0)
SConscript('src/clients/SConscript',build_dir='builddir/clients',duplicate=0)
SConscript('src/plugins/SConscript',build_dir='builddir/plugins', duplicate=0)
SConscript('src/clients/lib/python/SConscript')
SConscript('src/lib/SConscript')

#base_env.XmmsManual('doc/xmms2.1')

