import SCons
import xmmsenv
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
	base_env.CheckAndAddFlagsToGroup("glib", "pkg-config --libs --cflags glib-2.0", fail=1)
	base_env.CheckAndAddFlagsToGroup("glib-thread", "pkg-config --libs --cflags gthread-2.0", fail=1)
	base_env.CheckAndAddFlagsToGroup("glib-module", "pkg-config --libs --cflags gmodule-2.0", fail=1)
	base_env.CheckLibAndAddFlagsToGroup("sqlite","sqlite","sqlite_open", fail=1)
	if base_env.HasGroup("sqlite"):
		base_env.AddFlagsToGroup("sqlite", " -DHAVE_SQLITE");

	##
	## Check for optional libs
	##
	base_env.CheckAndAddFlagsToGroup("ecore", "ecore-config --libs --cflags")
	if base_env.HasGroup("ecore") :
		base_env.AddFlagsToGroup("ecore", " -DHAVE_ECORE")

	base_env.CheckAndAddFlagsToGroup("qt", "pkg-config --libs --cflags qt")
	base_env.CheckAndAddFlagsToGroup("shout", "pkg-config --libs --cflags shout")
	base_env.CheckAndAddFlagsToGroup("curl", "curl-config --libs --cflags")
	if base_env.HasGroup("curl"):
		base_env.AddFlagsToGroup("curl", " -DHAVE_CURL");
	base_env.CheckAndAddFlagsToGroup("sdl", "sdl-config --libs --cflags")
	base_env.CheckAndAddFlagsToGroup("alsa","pkg-config --cflags --libs alsa")
	base_env.CheckLibAndAddFlagsToGroup("sdl-ttf","SDL_ttf","TTF_Init",depends="sdl")
	base_env.CheckLibAndAddFlagsToGroup("ogg","ogg","ogg_sync_init")
	base_env.CheckLibAndAddFlagsToGroup("vorbis","vorbis","vorbis_synthesis_init")
	base_env.CheckLibAndAddFlagsToGroup("vorbisenc","vorbisenc","vorbis_encode_ctl",depends="vorbis")
	base_env.CheckLibAndAddFlagsToGroup("vorbisfile","vorbisfile","ov_open_callbacks",depends="vorbis")
	base_env.CheckLibAndAddFlagsToGroup("math","m","cos")
	base_env.CheckLibAndAddFlagsToGroup("flac", "FLAC", "FLAC__seekable_stream_decoder_get_state")
	base_env.CheckLibAndAddFlagsToGroup("speex", "speex", "speex_bits_init")
	base_env.CheckAndAddFlagsToGroupFromLibTool("resid", "libresid-builder.la")
	base_env.CheckAndAddFlagsToGroupFromLibTool("sid", "libsidplay2.la")
	base_env.CheckLibAndAddFlagsToGroup("samba","libsmbclient","smbc_init")
	base_env.CheckAndAddFlagsToGroup("jack", "pkg-config --libs --cflags jack")
	base_env.CheckAndAddFlagsToGroup("modplug", "pkg-config --libs --cflags libmodplug")
	base_env.CheckAndAddFlagsToGroup("valgrind", "pkg-config --libs --cflags valgrind")
	if base_env.HasGroup("valgrind"):
		base_env.AddFlagsToGroup("valgrind", " -DHAVE_VALGRIND");
	base_env.CheckAndAddFlagsToGroup("gnomevfs", "pkg-config --libs --cflags gnome-vfs-2.0")

	if base_env.sys == 'Darwin':
		base_env.AddFlagsToGroup("CoreAudio", "-framework CoreAudio")
		base_env.AddFlagsToGroup("CoreAudio", " -framework AudioUnit")
		base_env.AddFlagsToGroup("CoreAudio", " -framework CoreServices")
		base_env.AddFlagsToGroup("Carbon", "-framework Carbon")
		base_env.AddFlagsToGroup("Cocoa", "-ObjC -framework Cocoa")
	   	base_env.AddFlagsToGroup("CoreFoundation", "-framework CoreFoundation")

	if base_env.CheckProgramAndAddFlagsToGroup ("pyrex", "pyrexc --version") :
		print "PyREX compiler found!"
	else:
		print "PyREX not found, no cookie for you!"

	if base_env.CheckProgram ("ruby --version") :
		cmd = "ruby -rmkmf -e 'print Config::CONFIG[\"libdir\"]'"
		rubylibdir = os.popen(cmd).read().strip()

		cmd = "ruby -rmkmf -e 'print Config::CONFIG[\"archdir\"]'"
		rubydir = os.popen(cmd).read().strip()

		cmd = "ruby -rmkmf -e 'print Config::CONFIG[\"sitearchdir\"]'"
		base_env.optional_config["rubysitedir"] = os.popen(cmd).read().strip()

		flags = " -I" + rubydir + " -L" + rubydir
		base_env.AddFlagsToGroup ("ruby", flags)

		if base_env.CheckCHeader ("ruby.h", rubydir) == 0 :
			base_env.RemoveGroup ("ruby")

	try:
		import distutils
		import distutils.sysconfig
		base_env.AddFlagsToGroup ("python", "-I"+distutils.sysconfig.get_python_inc ())
		base_env.optional_config["pythonlibdir"] = distutils.sysconfig.get_python_lib()
	except:
		pass
	##
	## Write cache
	##
	statefile = open('scons.cache','wb+')
	dump({"flag_groups":base_env.flag_groups,
	      "optional_config":base_env.optional_config},
	     statefile)


