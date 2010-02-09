#!/bin/sh

#
# unload.sh
#
# R.K.Raja
# (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
#
# (C) Copyright 2002, 2003.
# All rights reserved.
#
# Script to unload the rkfs kernel module.
#

echo -e "INFO: Unloading rkfs kernel module..."
sync
rmmod -v rkfs
lsmod | grep -i -q rkfs
if [ $? -ne 0 ]; then
    echo -e "INFO: You can load rkfs kernel module using load.sh"
    exit 0
fi
exit 1

