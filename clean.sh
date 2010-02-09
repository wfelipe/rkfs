#!/bin/sh

#
# clean.sh
#
# R.K.Raja
# (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
#
# (C) Copyright 2002, 2003.
# All rights reserved.
#
# Script to cleanup the rkfs kernel module object files.
#


echo -e "INFO: Cleaning..."
rm -rf *.o
rm -rf *.s
echo -e "INFO: Done."
