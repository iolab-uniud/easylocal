#!/bin/sh
touch NEWS README AUTHORS ChangeLog INSTALL COPYING
mkdir config
aclocal -I config
libtoolize --force --copy
automake --foreign --add-missing --copy
autoconf
