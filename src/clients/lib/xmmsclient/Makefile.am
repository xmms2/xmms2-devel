xmmsclientincludedir = $(includedir)/xmms

pkglib_LTLIBRARIES = libxmmsclient.la

xmmsclientinclude_HEADERS = xmmsclient.h
libxmmsclient_la_SOURCES = xmmsclient.c xmmsclient.h
libxmmsclient_la_LDFLAGS = @GLIB_LDFLAGS@ @DBUS_LIBS@ @DBUSGLIB_LIBS@
INCLUDES = @XMMS_CFLAGS@ @GLIB_CFLAGS@ @DBUS_CFLAGS@ -DG_LOG_DOMAIN=\"xmmsc\"
