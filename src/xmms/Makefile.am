
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@ @DBUS_CFLAGS@

bin_PROGRAMS=xmms2 xmms2_client

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
	unixsignal.c unixsignal.h \
	dbus.c dbus.h \
	core.c core.h

xmms2_LDADD = @GLIB_LIBS@ @XML_LIBS@ @DBUS_LIBS@ @DBUSGLIB_LIBS@

xmms2_client_SOURCES = xmms2_client.c

xmms2_client_LDADD = @GLIB_LIBS@ @DBUS_LIBS@ @DBUSGLIB_LIBS@




