
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ -I../lib/xmmsclient/ @SDL_CFLAGS@

bin_PROGRAMS=sdl-vis

sdl_vis_SOURCES = sdl-vis.c
sdl_vis_LDADD = @GLIB_LIBS@ @SDL_LIBS@ -L../lib/xmmsclient/ -lxmmsclient
