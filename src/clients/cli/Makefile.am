
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@ -I../lib/xmmsclient/

bin_PROGRAMS=xmms2 l44ttest

xmms2_SOURCES = xmms2_client.c

xmms2_LDADD = @GLIB_LIBS@ -L../lib/xmmsclient/ -lxmmsclient

l44ttest_SOURCES = l44t.c
l44ttest_LDADD = @XML_LIBS@ @GLIB_LIBS@
