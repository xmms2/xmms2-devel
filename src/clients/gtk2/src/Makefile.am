## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I/home/tru/xmms2/src/clients/lib/xmmsclient

bin_PROGRAMS = xmms2-gtk2

xmms2_gtk2_SOURCES = \
	main.c \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h

xmms2_gtk2_LDADD = @PACKAGE_LIBS@ /usr/local/lib/xmms/libxmmsclient.so

