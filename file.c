/*
*
* file.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#include <linux/fs.h>
#include <linux/sched.h>

#include <rkfs.h>

int rkfs_sync_file(struct file *file, struct dentry *dentry, int datasync)
{
	struct inode *vfs_inode = dentry->d_inode;
	int err = 0;

	err = fsync_inode_buffers(vfs_inode);
	err |= fsync_inode_data_buffers(vfs_inode);

	if (!(vfs_inode->i_state & I_DIRTY))
		goto out;

	if (datasync && !(vfs_inode->i_state & I_DIRTY_DATASYNC))
		goto out;

	err |= rkfs_sync_inode(vfs_inode);

 out:
	if (err)
		FAILED;

	return err ? -EIO : 0;
}

struct file_operations rkfs_file_operations = {
 llseek:generic_file_llseek,
 read:	generic_file_read,
 write:generic_file_write,
 mmap:	generic_file_mmap,
 open:	generic_file_open,
 fsync:rkfs_sync_file,
};

struct inode_operations rkfs_file_inode_operations = {
 truncate:rkfs_truncate,
};
