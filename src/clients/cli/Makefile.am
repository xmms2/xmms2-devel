
INCLUDES=@XMMS_CFLAGS@ @GLIB_CFLAGS@ @XML_CFLAGS@ -I../lib/xmmsclient/

bin_PROGRAMS=xmms2 

xmms2_SOURCES = xmms2_client.c

xmms2_LDADD = @GLIB_LIBS@ -L../lib/xmmsclient/ -lxmmsclient
