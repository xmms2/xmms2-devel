AC_INIT(src/xmms/main.c)
AM_INIT_AUTOMAKE(xmms, 1.9.0)

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AM_CONFIG_HEADER(config.h)

XMMS_CFLAGS="-DPKGLIBDIR=\\\"\$(pkglibdir)\\\" -I\$(top_srcdir)/src -include config.h -Wall"
AC_SUBST(XMMS_CFLAGS)

PLUGIN_LDFLAGS="-module -avoid-version -export -symbols-regex \"xmms_plugin_get\""
AC_SUBST(PLUGIN_LDFLAGS)

AM_PATH_GLIB_2_0(1.0.0,,AC_MSG_ERROR(Install GLib!),gmodule gthread)
AM_PATH_XML2(2.4.0,,AC_MSG_ERROR(Install libxml2!))

AC_OUTPUT([
Makefile
src/Makefile
src/xmms/Makefile
src/plugins/Makefile
src/plugins/mad/Makefile
src/plugins/file/Makefile])
