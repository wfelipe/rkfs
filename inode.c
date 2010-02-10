/*
*
* inode.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003
* All rights reserved.
*
*/

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/highuid.h>
#include <linux/quotaops.h>
#include <linux/module.h>

#include <rkfs.h>

MODULE_AUTHOR("R.K.Raja");
MODULE_DESCRIPTION("RK Floppy Filesystem");
MODULE_LICENSE("GPL");

void rkfs_read_inode(struct inode *vfs_inode)
{
	struct super_block *vfs_sb = NULL;
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;
	struct rkfs_inode *rkfs_dinode = NULL;
	char *ptr = NULL;
	unsigned short rkfs_sb_index = 0, rkfs_sb_count = 0, blkno = 0;
	unsigned short itable_index = 0, offset = 0, bit = 0;

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		goto no_inode_out;
	}

	rkfs_debug("Reading inode: %ld\n", vfs_inode->i_ino);

	if (vfs_inode->i_ino < RKFS_ROOT_INO) {
		rkfs_bug("Inode %ld is bad (< %d)\n", vfs_inode->i_ino,
			 RKFS_ROOT_INO);
		goto out;
	}

	if (!(vfs_sb = vfs_inode->i_sb)) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;
	rkfs_sb_index = vfs_inode->i_ino / RKFS_MIN_BLOCKS;
	if (rkfs_sb_index > (rkfs_sb_count - 1)) {
		rkfs_bug("Inode %ld is bad (invalid %s sb index)\n",
			 vfs_inode->i_ino, RKFS_NAME);
		goto out;
	}

	if (!(bh = vfs_sb->u.rkfs_sb.s_sbh[rkfs_sb_index])) {
		rkfs_bug("%s superblock not in memory\n", RKFS_NAME);
		goto out;
	}

	rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
	if (rkfs_dsb->s_fsid != RKFS_ID) {
		rkfs_bug("No valid %s superblock found\n", RKFS_NAME);
		goto out;
	}

	bit = vfs_inode->i_ino % RKFS_MIN_BLOCKS;
	itable_index = bit / RKFS_INODES_PER_BLOCK;
	if (itable_index > (RKFS_INODE_TABLES_MAP_SIZE - 1)) {
		rkfs_bug("Inode %ld (bit %d) is bad  (invalid itable index)\n",
			 vfs_inode->i_ino, bit);
		goto out;
	}

	if (!(blkno = rkfs_dsb->s_itable_map[itable_index][0])) {
		rkfs_bug("Inode %ld (bit %d) is bad (no inode block)\n",
			 vfs_inode->i_ino, bit);
		goto out;
	}

	if (!(bh = bread(vfs_inode->i_dev, blkno, vfs_sb->s_blocksize))) {
		rkfs_printk("Unable to read block %d from device %s\n", blkno,
			    bdevname(vfs_inode->i_dev));
		goto out;
	}

	offset = bit % RKFS_INODES_PER_BLOCK;
	ptr = (char *)bh->b_data + (offset * RKFS_INODE_SIZE);
	rkfs_dinode = (struct rkfs_inode *)ptr;

	vfs_inode->i_mode = rkfs_dinode->i_mode;
	vfs_inode->i_nlink = rkfs_dinode->i_links_count;
	vfs_inode->i_uid = rkfs_dinode->i_uid;
	vfs_inode->i_gid = rkfs_dinode->i_gid;
	vfs_inode->i_size = rkfs_dinode->i_size;
	vfs_inode->i_atime = rkfs_dinode->i_time;
	vfs_inode->i_mtime = rkfs_dinode->i_time;
	vfs_inode->i_ctime = rkfs_dinode->i_time;
	vfs_inode->i_blksize = PAGE_SIZE;
	vfs_inode->i_blocks = rkfs_dinode->i_blocks;
	for (blkno = 0; blkno < RKFS_N_BLOCKS; blkno++)
		vfs_inode->u.rkfs_i.i_block[blkno] =
		    rkfs_dinode->i_block[blkno];

	if (S_ISREG(vfs_inode->i_mode)) {
		rkfs_debug("Inode: %ld is a file\n", vfs_inode->i_ino);
		vfs_inode->i_op = &rkfs_file_inode_operations;
		vfs_inode->i_fop = &rkfs_file_operations;
		vfs_inode->i_mapping->a_ops = &rkfs_aops;
	} else if (S_ISDIR(vfs_inode->i_mode)) {
		rkfs_debug("Inode: %ld is a directory\n", vfs_inode->i_ino);
		vfs_inode->i_op = &rkfs_dir_inode_operations;
		vfs_inode->i_fop = &rkfs_dir_operations;
		vfs_inode->i_mapping->a_ops = &rkfs_aops;
	} else if (S_ISLNK(vfs_inode->i_mode)) {
		rkfs_debug("Inode: %ld is a link\n", vfs_inode->i_ino);
		vfs_inode->i_op = &page_symlink_inode_operations;
		vfs_inode->i_mapping->a_ops = &rkfs_aops;
	} else {
		rkfs_debug("Inode: %ld is a special file\n", vfs_inode->i_ino);
		init_special_inode(vfs_inode, vfs_inode->i_mode,
				   rkfs_dinode->i_block[0]);
	}

	brelse(bh);
	return;

 out:
	make_bad_inode(vfs_inode);

 no_inode_out:

	FAILED;
	return;
}

int rkfs_update_inode(struct inode *vfs_inode, int sync)
{
	struct super_block *vfs_sb = NULL;
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;
	struct rkfs_inode *rkfs_dinode = NULL;
	char *ptr = NULL;
	unsigned short rkfs_sb_index = 0, rkfs_sb_count = 0, blkno = 0;
	unsigned short itable_index = 0, offset = 0, bit = 0;
	int err = -EIO;

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		goto out;
	}

	rkfs_debug("Updating inode: %ld\n", vfs_inode->i_ino);

	if (vfs_inode->i_ino < RKFS_ROOT_INO) {
		rkfs_bug("Inode %ld is bad (< %d)\n", vfs_inode->i_ino,
			 RKFS_ROOT_INO);
		goto out;
	}

	if (!(vfs_sb = vfs_inode->i_sb)) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;
	rkfs_sb_index = vfs_inode->i_ino / RKFS_MIN_BLOCKS;
	if (rkfs_sb_index > (rkfs_sb_count - 1)) {
		rkfs_bug("Inode %ld is bad (invalid %s sb index)\n",
			 vfs_inode->i_ino, RKFS_NAME);
		goto out;
	}

	if (!(bh = vfs_sb->u.rkfs_sb.s_sbh[rkfs_sb_index])) {
		rkfs_bug("%s superblock not in memory\n", RKFS_NAME);
		goto out;
	}

	rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
	if (rkfs_dsb->s_fsid != RKFS_ID) {
		rkfs_bug("No valid %s superblock found\n", RKFS_NAME);
		goto out;
	}

	bit = vfs_inode->i_ino % RKFS_MIN_BLOCKS;
	itable_index = bit / RKFS_INODES_PER_BLOCK;
	if (itable_index > (RKFS_INODE_TABLES_MAP_SIZE - 1)) {
		rkfs_bug("Inode %ld (bit %d) is bad  (invalid itable index)\n",
			 vfs_inode->i_ino, bit);
		goto out;
	}

	if (!(blkno = rkfs_dsb->s_itable_map[itable_index][0])) {
		rkfs_bug("Inode %ld (bit %d) is bad (no inode block)\n",
			 vfs_inode->i_ino, bit);
		goto out;
	}

	if (!(bh = bread(vfs_inode->i_dev, blkno, vfs_sb->s_blocksize))) {
		rkfs_printk("Unable to read block %d from device %s\n", blkno,
			    bdevname(vfs_inode->i_dev));
		err = -EIO;
		goto out;
	}

	offset = bit % RKFS_INODES_PER_BLOCK;
	ptr = (char *)bh->b_data + (offset * RKFS_INODE_SIZE);
	rkfs_dinode = (struct rkfs_inode *)ptr;

	rkfs_dinode->i_mode = vfs_inode->i_mode;
	rkfs_dinode->i_links_count = vfs_inode->i_nlink;
	rkfs_dinode->i_uid = vfs_inode->i_uid;
	rkfs_dinode->i_gid = vfs_inode->i_gid;
	rkfs_dinode->i_size = vfs_inode->i_size;
	rkfs_dinode->i_time = vfs_inode->i_mtime;
	rkfs_dinode->i_blocks = vfs_inode->i_blocks;
	if (S_ISCHR(vfs_inode->i_mode) || S_ISBLK(vfs_inode->i_mode))
		rkfs_dinode->i_block[0] = kdev_t_to_nr(vfs_inode->i_rdev);
	else
		for (blkno = 0; blkno < RKFS_N_BLOCKS; blkno++)
			rkfs_dinode->i_block[blkno] =
			    vfs_inode->u.rkfs_i.i_block[blkno];

	/*
	   rkfs_dump_rkfs_inode(rkfs_dinode,"%s: Dumping %s inode (after)...", \
	   __FUNCTION__,RKFS_NAME);
	 */

	mark_buffer_dirty(bh);
	ll_rw_block(WRITE, 1, &bh);
	wait_on_buffer(bh);

	brelse(bh);
	return 0;

 out:
	FAILED;
	return err;
}

void rkfs_write_inode(struct inode *vfs_inode, int sync)
{
	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		return;
	}

	rkfs_debug("Writing inode: %ld\n", vfs_inode->i_ino);

	lock_kernel();
	rkfs_update_inode(vfs_inode, sync);
	unlock_kernel();
}

void rkfs_delete_inode(struct inode *vfs_inode)
{
	unsigned short rkfs_sb_count = 0, rkfs_sb_index = 0, bit = 0;
	struct super_block *vfs_sb = NULL;
	unsigned short icount = 0, iblkno = 0;

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		goto no_inode;
	}

	rkfs_debug("Deleting inode: %ld\n", vfs_inode->i_ino);

	lock_kernel();

	if (is_bad_inode(vfs_inode)) {
		rkfs_debug("Can't delete bad inode\n");
		goto no_delete;
	}

	if (!(vfs_sb = vfs_inode->i_sb)) {
		rkfs_bug("VFS superblock is NULL\n");
		goto no_delete;
	}

	rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;
	rkfs_sb_index = vfs_inode->i_ino / RKFS_MIN_BLOCKS;
	if (rkfs_sb_index > (rkfs_sb_count - 1)) {
		rkfs_bug("Inode %ld is bad (invalid %s sb index)\n",
			 vfs_inode->i_ino, RKFS_NAME);
		goto no_delete;
	}

	bit = vfs_inode->i_ino % RKFS_MIN_BLOCKS;
	if (rkfs_sb_index) {
		if (bit == 0) {
			rkfs_bug
			    ("Inode %ld (bit %d) is bad (can't free resv. inode)\n",
			     vfs_inode->i_ino, bit);
			goto no_delete;
		}
	} else {
		if (bit < RKFS_ROOT_INO) {
			rkfs_bug
			    ("Inode %ld (bit %d) is bad (can't free resv. inode)\n",
			     vfs_inode->i_ino, bit);
			goto no_delete;
		}
	}

	rkfs_debug("Delete inode: %ld (bit: %d)\n", vfs_inode->i_ino, bit);
	vfs_inode->i_size = 0;

	if (vfs_inode->i_blocks)
		rkfs_truncate(vfs_inode);

	rkfs_free_inode(vfs_inode, &icount, &iblkno);

	if (!icount)
		rkfs_free_inode_block(vfs_sb, iblkno);

	unlock_kernel();
	return;

 no_delete:
	unlock_kernel();
	clear_inode(vfs_inode);

 no_inode:
	FAILED;
}

int rkfs_sync_inode(struct inode *vfs_inode)
{
	int err = 0;

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		return -EIO;
	}

	rkfs_debug("Syncing inode: %ld\n", vfs_inode->i_ino);

	err = rkfs_update_inode(vfs_inode, 1);
	return err;
}
