if HAVE_LIBSIDPLAY2
pkglib_LTLIBRARIES = libxmms_sid.la
endif
libxmms_sid_la_SOURCES = sid.c sidplay_wrapper.cpp
libxmms_sid_la_LDFLAGS = @PLUGIN_LDFLAGS@ -lstdc++ -L@LIBSIDPLAY2_BUILDERS@ @LIBSIDPLAY2_LDFLAGS@  -lresid-builder

INCLUDES = @XMMS_CFLAGS@ @GLIB_CFLAGS@ @LIBSIDPLAY2_CXXFLAGS@
