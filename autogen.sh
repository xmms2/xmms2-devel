#!/bin/sh

(cd src/clients/gtk2 ; ./autogen.sh)

libtoolize --automake -c -f
aclocal -I acmacros
autoheader
automake -a -c
autoconf

./configure $@
