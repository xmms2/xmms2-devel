
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@

bin_PROGRAMS=xmms2 medialibtool

sysconf_DATA = xmms2.conf

xmms2_SOURCES=main.c \
	config_xmms.c config_xmms.h \
	plugin.c plugin.h \
	object.c object.h \
	ringbuf.c ringbuf.h \
	transport.c transport.h \
	decoder.c decoder.h \
	output.c output.h \
	playlist.c playlist.h \
	magic.c magic.h \
	medialib.c medialib.h \
	unixsignal.c unixsignal.h

medialibtool_SOURCES=medialibtool.c \
	config_xmms.c config_xmms.h \
	magic.c magic.h \
	object.c object.h \
	plugin.c plugin.h \
        ringbuf.c ringbuf.h \
        transport.c transport.h \
        decoder.c decoder.h \
        output.c output.h \
        playlist.c playlist.h \
	medialib.c medialib.h


xmms2_LDADD = @GLIB_LIBS@ @XML_LIBS@

medialibtool_LDADD = @GLIB_LIBS@ @XML_LIBS@


