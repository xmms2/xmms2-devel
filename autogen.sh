#!/bin/sh

libtoolize --automake -c -f
aclocal
autoheader
automake -a -c
autoconf

./configure $@
