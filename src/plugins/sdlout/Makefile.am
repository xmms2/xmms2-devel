
pkglib_LTLIBRARIES = libxmms_sdlout.la

libxmms_sdlout_la_SOURCES = sdlout.c
libxmms_sdlout_la_LDFLAGS = @PLUGIN_LDFLAGS@ `sdl-config --libs`
INCLUDES = @XMMS_CFLAGS@ @GLIB_CFLAGS@ `sdl-config --cflags`
