import SCons
import xmmsenv
from marshal import dump, load;

import os;
import sys;

##
## This function will check for all libs that XMMS2
## might need. Or its plugins.
##

def checkFlags(base_env):
	##
	## Check for essensial libs
	##
	base_env.CheckAndAddFlagsToGroup("mad", "pkg-config --libs --cflags mad", fail=1)
	base_env.CheckAndAddFlagsToGroup("xml2", "xml2-config --libs --cflags", fail=1)
	base_env.CheckAndAddFlagsToGroup("glib", "pkg-config --libs --cflags gthread-2.0 glib-2.0 gmodule-2.0", fail=1)
	base_env.CheckAndAddFlagsToGroup("dbus", "pkg-config --libs --cflags dbus-1", fail=1)
	base_env.CheckAndAddFlagsToGroup("dbusglib", "pkg-config --libs --cflags dbus-1 dbus-glib-1", fail=1)

	##
	## Check for optional libs
	##
	base_env.CheckAndAddFlagsToGroup("qt", "pkg-config --libs --cflags qt")
	base_env.CheckAndAddFlagsToGroup("curl", "curl-config --libs --cflags")
	base_env.CheckAndAddFlagsToGroup("sdl", "sdl-config --libs --cflags")
	base_env.CheckAndAddFlagsToGroup("alsa","pkg-config --cflags --libs alsa")
	base_env.CheckLibAndAddFlagsToGroup("sdl-ttf","SDL_ttf","TTF_Init",depends="sdl")
	base_env.CheckLibAndAddFlagsToGroup("vorbis","vorbis","ogg_sync_init")
	base_env.CheckLibAndAddFlagsToGroup("sqlite","sqlite","sqlite_open")
	##
	## Write cache
	##
	statefile = open('scons.cache','wb+')
	dump(base_env.flag_groups, statefile);


