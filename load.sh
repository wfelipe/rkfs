#!/bin/sh

#
# load.sh
#
# R.K.Raja
# (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
#
# (C) Copyright 2002, 2003.
# All rights reserved.
#
# Script to load the rkfs linux kernel module.
#

echo -e "INFO: Loading rkfs..."
sync
insmod -v  rkfs.o
lsmod | grep -i -q rkfs
if [ $? -eq 0 ]; then
    echo -e "INFO: You can unload rkfs using unload.sh"
    exit 0
fi
exit 1


