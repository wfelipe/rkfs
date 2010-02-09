/*
*
* utils.h
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#ifndef __UTILS_H__
#define __UTILS_H__

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include <linux/rkfs.h>

void rkfs_dump_super_block(const struct super_block *sb,
                            const char *frmt, ...);
void rkfs_dump_inode(const struct inode *_inode, const char *frmt, ...);
void rkfs_dump_buffer_head(const struct buffer_head *bh,
                            const char *frmt, ...);
void rkfs_dump_rkfs_super_block(const struct rkfs_super_block *rkfs_sb,
                                  const char *frmt, ...);
void rkfs_dump_rkfs_inode(const struct rkfs_inode *_rkfs_inode,
                            const char *frmt, ...);
void rkfs_dump_rkfs_dir_entry(const struct rkfs_dir_entry *rkfs_de,
                                const char *frmt, ...);


#endif
