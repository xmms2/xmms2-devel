
import os;

## check for required libs
# xml
if WhereIs("xml2-config") == None:
	print "xms2-config not found"
	Exit(1)

# glib

# dbus

## check for optional libs
#if WhereIs("sdl-config") != None:
#	have["sdl"] = 1
#else:
#	print "sdl-config not found, skipping sdl-vis"

glib_linkflags = os.popen("pkg-config --libs-only-other gthread-2.0 glib-2.0 gmodule-2.0").read().strip()
glib_ccflags = os.popen("pkg-config --cflags-only-other gthread-2.0 glib-2.0 gmodule-2.0").read().strip()

Export('glib_linkflags','glib_ccflags')

SConscript('src/SConscript')
