import SCons
import xmmsenv
import distutils
import distutils.sysconfig
from marshal import dump, load;

import os;
import sys;


def showOpts(base_env):
	print "\n* Configuration options\n"
	print "C-Compiler: ", base_env["CC"]
	print "C++-Compiler: ", base_env["CXX"]
	print "C-Flags: ", base_env["CCFLAGS"]
	print "Installation prefix: ", base_env["PREFIX"]
	print "\nModules: "
	for x in base_env.flag_groups.keys():
		print "Module " + x + ": " + base_env.flag_groups[x]
	print "\n* Now run scons again to compile...\n"

##
## This function will check for all libs that XMMS2
## might need. Or its plugins.
##

def checkFlags(base_env):
	##
	## Check for essensial libs
	##
	base_env.CheckAndAddFlagsToGroup("mad", "pkg-config --libs --cflags mad", fail=1)
	base_env.CheckAndAddFlagsToGroup("glib", "pkg-config --libs --cflags gthread-2.0 glib-2.0 gmodule-2.0", fail=1)
	base_env.CheckAndAddFlagsToGroup("dbus", "pkg-config --libs --cflags dbus-1", fail=1)
	base_env.CheckAndAddFlagsToGroup("dbusglib", "pkg-config --libs --cflags dbus-1 dbus-glib-1", fail=1)

	##
	## Check for optional libs
	##
	base_env.CheckAndAddFlagsToGroup("ecore", "ecore-config --libs --cflags")
	base_env.CheckAndAddFlagsToGroup("qt", "pkg-config --libs --cflags qt")
	base_env.CheckAndAddFlagsToGroup("shout", "pkg-config --libs --cflags shout")
	base_env.CheckAndAddFlagsToGroup("curl", "curl-config --libs --cflags")
	if base_env.HasGroup("curl"):
		base_env.AddFlagsToGroup("curl", " -DHAVE_CURL");
	base_env.CheckAndAddFlagsToGroup("sdl", "sdl-config --libs --cflags")
	base_env.CheckAndAddFlagsToGroup("alsa","pkg-config --cflags --libs alsa")
	base_env.CheckLibAndAddFlagsToGroup("sdl-ttf","SDL_ttf","TTF_Init",depends="sdl")
	base_env.CheckLibAndAddFlagsToGroup("vorbis","vorbis","ogg_sync_init")
	base_env.CheckLibAndAddFlagsToGroup("vorbisenc","vorbisenc","vorbis_encode_ctl",depends="vorbis")
	base_env.CheckLibAndAddFlagsToGroup("vorbisfile","vorbisfile","ov_open_callbacks",depends="vorbis")
	base_env.CheckLibAndAddFlagsToGroup("sqlite","sqlite","sqlite_open")
	if base_env.HasGroup("sqlite"):
		base_env.AddFlagsToGroup("sqlite", " -DHAVE_SQLITE");
	base_env.CheckLibAndAddFlagsToGroup("math","m","cos")

	if base_env.sys == 'Darwin':
		base_env.AddFlagsToGroup("CoreAudio", "-framework CoreAudio")
	
	if base_env.CheckProgramAndAddFlagsToGroup ("pyrex", "pyrexc") :
		print "PyREX compiler found!"
	else :
		print "PyREX not found, no cookie for you!"

	base_env.AddFlagsToGroup ("python", "-I"+distutils.sysconfig.get_python_inc ())

	##
	## Write cache
	##
	statefile = open('scons.cache','wb+')
	dump(base_env.flag_groups, statefile);


