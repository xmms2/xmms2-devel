
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@ @DBUS_CFLAGS@

bin_PROGRAMS=xmms2d medialibtool testpl

sysconf_DATA = xmms2.conf

xmmscore_sources=config_xmms.c config_xmms.h \
	plugin.c plugin.h \
	object.c object.h \
	ringbuf.c ringbuf.h \
	transport.c transport.h \
	decoder.c decoder.h \
	output.c output.h \
	playlist.c playlist.h \
	magic.c magic.h \
	medialib.c medialib.h \
	core.c core.h \
	log.c log.h 

xmms2d_LDADD = @GLIB_LIBS@ @XML_LIBS@ @DBUS_LIBS@ @DBUSGLIB_LIBS@
xmms2d_SOURCES=main.c \
	unixsignal.c unixsignal.h \
	dbus.c dbus.h \
	$(xmmscore_sources)

medialibtool_LDADD = @GLIB_LIBS@ @XML_LIBS@
medialibtool_SOURCES=medialibtool.c \
	$(xmmscore_sources)

testpl_LDADD = @GLIB_LIBS@ @XML_LIBS@
testpl_SOURCES=testpl.c \
	$(xmmscore_sources)

