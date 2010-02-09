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
# Script to compile rkfs kernel module.
#
# NOTE: For the compilation to succeed the fs.h file in 
# linux kernel source has to be patched. Please do the
# following steps to patch fs.h:
#
# 1. cd /usr/src/linux/include/linux
# 
# 2. cp fs.h fs.h.orig
#
# 3. Edit fs.h
#
# 4. Search for jffs2_fs_i.h and add the following line next to that:
#    #include <linux/rkfs_i.h>
#
# 5. Search for jffs2_inode_info and add the following line next to that:
#    struct rkfs_inode_info         rkfs_i;
#
# 6. Search for jffs2_fs_sb.h and add the following line next to that:
#    #include <linux/rkfs_sb.h>
#
# 7. Search for cramfs_sb_info and add the following line next to that:
#    struct rkfs_sb_info    rkfs_sb;
#
#

if [ -f ./clean.sh ]; then
    ./clean.sh
fi

############################################# 
# Enable/disable debug
#############################################
#dbg="-D__RKFS_DEBUG__ -D__RKFS_DUMP_DEBUG__"
dbg=""

#############################################
# Location of kernel include files
#############################################
inc="/usr/src/linux-2.4.18-24.8.0/include"
if [ ! -d $inc ]; then
    echo -e "ERROR: The include directory: $inc does not exist."
    exit 1
fi

#############################################
# Check whether fs.h is patched
############################################
pat="rkfs_i.h rkfs_i rkfs_sb.h rkfs_sb"
for p in $pat
do
    grep -i -q -w "$p" $inc/linux/fs.h
    if [ $? != 0 ]; then
        echo -e "ERROR: $inc/linux/fs.h is NOT patched."
        exit 1
    fi
done
echo -e "INFO: fs.h is patched."

#############################################
# Location of kernel modversion file
#############################################
mod_ver_file="-include $inc/linux/modversions.h"
if [ ! -f $inc/linux/modversions.h ]; then
    echo -e "ERROR: The modversion file: $mod_ver_file does not exist."
    exit 1
fi

#############################################
# List of source files to be compiled.
#############################################
SRC="utils bitmap super file inode balloc ialloc asops itree namei dir"

#############################################
# Copy rkfs header files to kernel include directory
#############################################
ifiles="rkfs.h rkfs_i.h rkfs_sb.h"
for f in $ifiles
do
    if [ ! -f $inc/linux/$f ]; then
        cp -v $f $inc/$f
    fi
done

#############################################
# Compile rkfs source files
#############################################
for f in $SRC
do
    echo -e "INFO: Compiling $f.c ..."
    gcc $dbg -D__KERNEL__ -I$inc -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fomit-frame-pointer -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -march=i486 -DMODULE -DMODVERSIONS $mod_ver_file -DKBUILD_BASENAME=$f  -c -o $f.o $f.c
done

#############################################
# Link the object files
#############################################
echo -e "INFO: Linking..."
ld -m elf_i386  -r -o rkfs.o `ls *.o`
if [ -e rkfs.o ]; then
    mv -v rkfs.o rkfs.obj
fi
echo -e "INFO: Removing unwanted object files..."
rm -rf *.o
if [ -e rkfs.obj ]; then
    mv -v rkfs.obj rkfs.o
fi
echo -e "INFO: Done."
echo -e "INFO: Now you can load rkfs kernel module with load.sh"

exit 0
