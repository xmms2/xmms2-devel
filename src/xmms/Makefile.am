
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@

bin_PROGRAMS=xmms2

xmms2_SOURCES=main.c \
	config.c config.h \
	plugin.c plugin.h \
	object.c object.h \
	ringbuf.c ringbuf.h \
	transport.c transport.h \
	decoder.c decoder.h \
	playlist.c playlist.h

xmms2_LDADD = @GLIB_LIBS@ @XML_LIBS@
