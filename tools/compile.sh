#!/bin/sh

#
# compile.sh
#
# R.K.Raja
# (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
#
# (C) Copyright 2002, 2003.
# All rights reserved.
#
# Script to compile rkfs userland tools.
#

if [ -f ./clean.sh ]; then
    ./clean.sh
fi

echo -e "INFO: Compiling mkrkfs.c ..."
gcc -Wall -g -O -o mkrkfs mkrkfs.c utils.c

echo -e "INFO: Compiling dumprkfs.c ..."
gcc -Wall -g -O -o dumprkfs dumprkfs.c utils.c

echo -e "Done."
