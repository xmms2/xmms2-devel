
import xmmsenv;
import os;

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
opts.Add('CC', 'C compiler to use', 'gcc')
opts.Add('CCFLAGS', 'compilerflags', '-g -Wall -O0')
opts.Add('PREFIX', 'installprefix', '/usr/local')

## setup base environment...
## ...ok, this should be a bit configurable... later.
##
## SCons-tips 42: start paths with '#' to have them change
##                correctly when we descend into subdirs
base_env = xmmsenv.XmmsEnvironment(options=opts, LINK="gcc", CPPPATH = ['#src'])


##
## Check for essensial libs
##
base_env.CheckAndAddFlagsToGroup("mad", "pkg-config --libs --cflags mad", fail=1)
base_env.CheckAndAddFlagsToGroup("xml2", "xml2-config --libs --cflags", fail=1)
base_env.CheckAndAddFlagsToGroup("glib", "pkg-config --libs --cflags gthread-2.0 glib-2.0 gmodule-2.0", fail=1)
base_env.CheckAndAddFlagsToGroup("dbus", "pkg-config --libs --cflags dbus-1 dbus-glib-1", fail=1)

##
## Check for optional libs
##
base_env.CheckAndAddFlagsToGroup("sdl", "sdl-config --libs --cflags")
base_env.CheckLibAndAddFlagsToGroup("sdl-ttf","SDL_ttf","TTF_Init",depends="sdl")
base_env.CheckLibAndAddFlagsToGroup("vorbis","vorbis","ogg_sync_init")
#sid!
base_env.CheckLibAndAddFlagsToGroup("sqlite","sqlite","sqlite_open")
#enable gtk2 gui again, someday.
#base_env.CheckAndAddFlagsToGroup("gtk2", "pkg-config --libs --cflags gtk+-x11-2.0")

Export('base_env')

SConscript('src/xmms/SConscript')
SConscript('src/clients/SConscript')
SConscript('src/plugins/SConscript')

