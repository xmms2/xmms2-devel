AC_INIT(src/xmms/main.c)
AC_CONFIG_AUX_DIR(acmacros)
AM_INIT_AUTOMAKE(xmms, 1.9.0)

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_LIBTOOL


XMMS_CFLAGS="-DPKGLIBDIR=\\\"\$(pkglibdir)\\\" -I\$(top_srcdir)/src -include config.h -Wall"
AC_SUBST(XMMS_CFLAGS)

PLUGIN_LDFLAGS="-module -avoid-version -export -symbols-regex \"xmms_plugin_get\""
AC_SUBST(PLUGIN_LDFLAGS)

AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR(Install GLib!),gmodule gthread)
AM_PATH_XML2(2.4.0,,AC_MSG_ERROR(Install libxml2!))

PKG_CHECK_MODULES(MAD, mad >= 0.14.0,,AC_MSG_ERROR(Install libmad!))
AC_SUBST(MAD_LIBS)
AC_SUBST(MAD_CFLAGS)

AC_LANG_CPLUSPLUS
dnl gives us LIBSIDPLAY2_CXXFLAGS LIBSIDPLAY2_LDFLAGS LIBSIDPLAY2_BUILDERS
SID2_FIND_LIBSIDPLAY2
SID2_TEST_BUILDER(resid,$LIBSIDPLAY2_CXXFLAGS,-L$LIBSIDPLAY2_BUILDERS $LIBSIDPLAY2_LDFLAGS,LIBSIDPLAY2_RESID_LDFLAGS)
AC_SUBST(LIBSIDPLAY2_RESID_LDFLAGS)
AM_CONDITIONAL(HAVE_LIBSIDPLAY2,test "$LIBSIDPLAY2_RESID_LDFLAGS" != "")
AC_LANG_C

AM_PATH_SDL(1.2.5,,AC_MSG_ERROR(Install LibSDL!))

AC_OUTPUT([
Makefile
src/Makefile
src/xmms/Makefile
src/plugins/Makefile
src/plugins/mad/Makefile
src/plugins/http/Makefile
src/plugins/diskwrite/Makefile
src/plugins/sdlout/Makefile
src/plugins/oss/Makefile
src/plugins/file/Makefile])
