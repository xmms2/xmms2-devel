
if HAVE_SQLITE
pkglib_LTLIBRARIES = libxmms_sqlite.la
endif

libxmms_sqlite_la_SOURCES = sqlite.c str.c
libxmms_sqlite_la_LDFLAGS = @PLUGIN_LDFLAGS@ -lsqlite
INCLUDES = @XMMS_CFLAGS@ @GLIB_CFLAGS@
