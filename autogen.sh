#!/bin/sh

libtoolize --automake -c -f
aclocal -I acmacros
autoheader
automake -a -c
autoconf

./configure $@
