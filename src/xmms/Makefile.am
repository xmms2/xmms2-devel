
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@

bin_PROGRAMS=xmms2

xmms2_SOURCES=main.c \
	config.c config.h \
	transport.c transport.h \
	playlist.c playlist.h
	#plugin.c plugin.h

xmms2_LDADD = @GLIB_LIBS@ @XML_LIBS@
