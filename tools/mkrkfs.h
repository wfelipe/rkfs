/*
*
* mkrkfs.h
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#ifndef __MKRKFS_H__
#define __MKRKFS_H__

boolean verbose = FALSE;
boolean quiet = FALSE;
boolean skip_badblocks = FALSE;
boolean version = FALSE;

void print_version();
void print_usage(const char *prg_name);
char *parse_args(int argc, char *argv[]);
char *create_new_dir_block_template(ushort pinode, ushort cinode);
int write_inode(register int fd, struct rkfs_super_block *sb,
		ushort ino, struct rkfs_inode *inode);
int create_root_directory(register int fd, struct rkfs_super_block *sb);
int mark_badblocks(register int fd, struct rkfs_super_block *sb,
		   ushort total_blocks, uint offset, ushort * total_bad_blocks);
int create_rkfs(const char *device, ushort total_blocks);

#endif
