/*
*
* super.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>

#include "rkfs.h"

static const struct super_operations rkfs_sops = {
	.put_super = rkfs_put_super,
	.write_super = rkfs_write_super,
	.statfs = rkfs_statfs,
	.read_inode = rkfs_read_inode,
	.write_inode = rkfs_write_inode,
	.delete_inode = rkfs_delete_inode,
};

void rkfs_write_super(struct super_block *vfs_sb)
{
	unsigned short i = 0, rkfs_sb_count = 0;
	struct rkfs_sb_info *rkfs_sbi = NULL;
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;

	if (vfs_sb == NULL) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	rkfs_sbi = vfs_sb->s_fs_info;
	rkfs_sb_count = rkfs_sbi->s_sb_count;
	rkfs_debug("Number of %s superblocks to write: %d\n", RKFS_NAME,
		   rkfs_sb_count);

	for (i = 0; i < rkfs_sb_count; i++) {
		if (!(bh = rkfs_sbi->s_sbh[i])) {
			rkfs_bug("No %s superblock in memory\n", RKFS_NAME);
			goto out;
		}

		rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
		if (rkfs_dsb->s_fsid != RKFS_ID) {
			rkfs_bug("No valid %s superblock found\n", RKFS_NAME);
			goto out;
		}

		mark_buffer_dirty(bh);
		ll_rw_block(WRITE, 1, &bh);
		wait_on_buffer(bh);
	}

	vfs_sb->s_dirt = 0;
	return;

 out:
	FAILED;
	return;
}

void rkfs_put_super(struct super_block *vfs_sb)
{
	unsigned short i = 0, rkfs_sb_count = 0;
	struct buffer_head *bh = NULL;
	struct rkfs_sb_info *rkfs_sbi = NULL;

	if (vfs_sb == NULL) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	rkfs_sbi = vfs_sb->s_fs_info;
	rkfs_sb_count = rkfs_sbi->s_sb_count;
	rkfs_debug("Number of %s superblocks to put: %d\n", RKFS_NAME,
		   rkfs_sb_count);

	for (i = 0; i < rkfs_sb_count; i++) {
		if (!(bh = rkfs_sbi->s_sbh[i])) {
			rkfs_bug("No %s superblock in memory\n", RKFS_NAME);
			goto out;
		}

		brelse(bh);
	}

	kfree(rkfs_sbi->s_sbh);
	rkfs_sbi->s_sb_count = 0;
	return;

 out:
	FAILED;
	return;
}

int rkfs_statfs(struct super_block *vfs_sb, struct statfs *sbuf)
{
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;
	unsigned short i = 0, rkfs_sb_count = 0;
	unsigned short offset = 0, tb = 0, fb = 0, fi = 0;
	int err = -EIO;
	struct rkfs_sb_info *rkfs_sbi = NULL;

	if (vfs_sb == NULL) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	if (sbuf == NULL) {
		rkfs_bug("statfs result holder is NULL\n");
		goto out;
	}

	sbuf->f_type = RKFS_ID;
	sbuf->f_bsize = vfs_sb->s_blocksize;
	sbuf->f_namelen = RKFS_MAX_FILENAME_LEN;

	rkfs_sbi = vfs_sb->s_fs_info;
	rkfs_sb_count = rkfs_sbi->s_sb_count;
	for (i = 0; i < rkfs_sb_count; i++) {
		if (!(bh = rkfs_sbi->s_sbh[i])) {
			rkfs_bug("No %s superblock in memory\n", RKFS_NAME);
			goto out;
		}

		rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
		if (rkfs_dsb->s_fsid != RKFS_ID) {
			rkfs_bug("No valid %s superblock found\n", RKFS_NAME);
			goto out;
		}

		tb = rkfs_dsb->s_total_blocks;
		fb += rkfs_count_free(rkfs_dsb->s_block_map, offset, tb);
		fi += rkfs_count_free(rkfs_dsb->s_inode_map, offset, tb);
		offset += RKFS_MIN_BLOCKS;
	}

	sbuf->f_blocks = tb;
	sbuf->f_bfree = sbuf->f_bavail = fb;

	if (fi > fb) {
		rkfs_debug("Too many free inodes (%d) (> avail free blocks)\n",
			   fi);
		fi = fb;
	}
	sbuf->f_ffree = fi;

	rkfs_debug("Filesystem type: %ld\n", sbuf->f_type);
	rkfs_debug("Optimal transfer block size: %ld\n", sbuf->f_bsize);
	rkfs_debug("Maximum filename length: %ld\n", sbuf->f_namelen);
	rkfs_debug("Total blocks in fs: %ld\n", sbuf->f_blocks);
	rkfs_debug("Free blocks in fs: %d\n", fb);
	rkfs_debug("Free inodes in fs: %d\n", fi);

	return 0;

 out:
	FAILED;
	return err;
}

static int rkfs_fill_super(struct super_block *vfs_sb, void *data, int silent)
{
	int blk_size = 0;
	dev_t dev;
	unsigned short offset = 0, rkfs_sb_count = 0, tb = 0;
	unsigned short i = 0, j = 0, loaded_sb = 0;
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;
	struct inode *vfs_root_inode = NULL;
	struct rkfs_sb_info *rkfs_sbi = NULL;
	int ret = -EINVAL;
	char b[BDEVNAME_SIZE];

	if (vfs_sb == NULL) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	dev = vfs_sb->s_dev;
	blk_size = vfs_sb->s_blocksize;
	if (blk_size > RKFS_BLOCK_SIZE) {
		rkfs_printk("Block size (%d) too big for %s\n", blk_size,
			    RKFS_NAME);
		goto out;
	}

	blk_size = RKFS_BLOCK_SIZE;
	if (sb_set_blocksize(vfs_sb, blk_size) < 0) {
		rkfs_printk("Unable to set blocksize for device %s to %d\n",
			    __bdevname(dev, b), blk_size);
		goto out;
	}

	if (!(bh = sb_bread(vfs_sb, blk_size))) {
		rkfs_printk("Unable to read the %s superblock at offset %d\n",
			    RKFS_NAME, RKFS_SUPER_BLOCK);
		goto out;
	}

	if (bh->b_size != blk_size) {
		rkfs_printk
		    ("Conflict in blocksize between buffer cache (%ld) and %s\n",
		     bh->b_size, RKFS_NAME);
		goto release_and_out;
	}

	rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
	if (rkfs_dsb->s_fsid != RKFS_ID) {
		rkfs_printk("Can't find valid %s filesystem on device %s\n",
			    RKFS_NAME, __bdevname(dev, b));
		goto release_and_out;
	}

	if (rkfs_dsb->s_state != RKFS_VALID_FS)
		rkfs_printk("Mounting unchecked file system\n");

	if (rkfs_dsb->s_state == RKFS_ERROR_FS)
		rkfs_printk("Mounting filesystem with errors\n");

	tb = rkfs_dsb->s_total_blocks;
	rkfs_sb_count = (tb + RKFS_MIN_BLOCKS - 1) / RKFS_MIN_BLOCKS;
	rkfs_sbi = vfs_sb->s_fs_info;
	rkfs_sbi->s_sb_count = rkfs_sb_count;
	rkfs_debug("Total superblocks in filesystem: %d\n", rkfs_sb_count);

	vfs_sb->s_blocksize_bits = 10;
	vfs_sb->s_blocksize = RKFS_BLOCK_SIZE;
	vfs_sb->s_magic = rkfs_dsb->s_fsid;
	vfs_sb->s_maxbytes = rkfs_max_file_size(tb, rkfs_sb_count);
	vfs_sb->s_op = &rkfs_sops;

	rkfs_sbi->s_sbh = kmalloc(rkfs_sb_count *
				  sizeof(struct buffer_head *), GFP_KERNEL);
	if (rkfs_sbi->s_sbh == NULL) {
		rkfs_printk("Not enough memory to load %s superblocks\n",
			    RKFS_NAME);
		goto release_and_out;
	}

	rkfs_sbi->s_sbh[0] = bh;
	offset += RKFS_MIN_BLOCKS;
	for (i = 1; i < rkfs_sb_count; i++) {
		loaded_sb = i;
		if (!(bh = sb_bread(vfs_sb, blk_size))) {
			rkfs_printk
			    ("Unable to read %s superblock at offset %d\n",
			     RKFS_NAME, offset);
			goto cleanup_loaded_sb;
		}

		rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
		if (rkfs_dsb->s_fsid != RKFS_ID) {
			rkfs_printk
			    ("Invalid %s superblock found at offset %d\n",
			     RKFS_NAME, offset);
			goto cleanup_loaded_sb;
		}

		rkfs_sbi->s_sbh[i] = bh;
		offset += RKFS_MIN_BLOCKS;
	}

	if (!(vfs_root_inode = iget(vfs_sb, RKFS_ROOT_INO))) {
		rkfs_printk("Unable to get root inode\n");
		goto cleanup_loaded_sb;
	}

	if (!(vfs_sb->s_root = d_alloc_root(vfs_root_inode))) {
		rkfs_printk("Root inode corrupted\n");
		iput(vfs_root_inode);
		goto cleanup_loaded_sb;
	}

	if (!S_ISDIR(vfs_sb->s_root->d_inode->i_mode) ||
	    !vfs_sb->s_root->d_inode->i_blocks ||
	    !vfs_sb->s_root->d_inode->i_size) {
		dput(vfs_sb->s_root);
		vfs_sb->s_root = NULL;
		rkfs_printk("Root inode corrupted\n");
		goto cleanup_loaded_sb;
	}

	return 0;

 cleanup_loaded_sb:
	for (j = 0; j < loaded_sb; j++)
		brelse(rkfs_sbi->s_sbh[j]);
	kfree(rkfs_sbi->s_sbh);
	goto out;

 release_and_out:
	brelse(bh);

 out:
	FAILED;
	return ret;
}

static int rkfs_get_sb(struct file_system_type *fs_type,
		       int flags, const char *dev_name, void *data,
		       struct vfsmount *mnt)
{
	return get_sb_bdev(fs_type, flags, dev_name, data, rkfs_fill_super,
			   mnt);
}

static struct file_system_type rkfs_type = {
	.owner = THIS_MODULE,
	.name = "rkfs",
	.get_sb = rkfs_get_sb,
	.kill_sb = kill_block_super,
	.fs_flags = FS_REQUIRES_DEV,
};

static int __init init_rkfs(void)
{
	rkfs_debug("========================\n");
	rkfs_debug("Registering %s ...\n", RKFS_NAME);
	rkfs_debug("========================\n");

	return register_filesystem(&rkfs_type);
}

static void __exit exit_rkfs(void)
{
	rkfs_debug("Unregistering %s ...\n", RKFS_NAME);

	unregister_filesystem(&rkfs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_rkfs)
    module_exit(exit_rkfs)
