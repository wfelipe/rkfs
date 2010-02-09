/*
*
* balloc.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/quotaops.h>

#include <rkfs.h>

int rkfs__free_blocks(struct super_block *vfs_sb,
		      struct inode *vfs_inode,
		      unsigned short blkno, unsigned short count)
{
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;
	unsigned short i = 0, tb = 0, nblkno = 0, group_last_blkno = 0;
	unsigned short rkfs_sb_count = 0, rkfs_sb_index = 0, bit = 0;
	int err = -EIO;

	rkfs_debug("Block: %d, Count: %d\n", blkno, count);

	if (!vfs_sb) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;
	rkfs_sb_index = blkno / RKFS_MIN_BLOCKS;
	if (rkfs_sb_index > (rkfs_sb_count - 1)) {
		rkfs_bug("Block %d is bad (invalid %s sb index)\n", blkno,
			 RKFS_NAME);
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

	tb = rkfs_dsb->s_total_blocks;
	group_last_blkno = (rkfs_sb_index + 1) * RKFS_MIN_BLOCKS;
	if (((blkno + count) > tb) || ((blkno + count) > group_last_blkno)) {
		rkfs_bug("Block %d not in valid range\n", blkno);
		goto out;
	}

	for (i = 0; i < count; i++) {
		nblkno = blkno + i;
		bit = nblkno - (rkfs_sb_index * RKFS_MIN_BLOCKS);
		if (rkfs_sb_index) {
			if (bit == 0) {
				rkfs_bug
				    ("Block %d (bit %d) is bad (can't free metadata)\n",
				     nblkno, bit);
				goto out;
			}
		} else {
			if (bit < RKFS_FIRST_BLOCK) {
				rkfs_bug
				    ("Block %d (bit %d) is bad (can't free metadata)\n",
				     nblkno, bit);
				goto out;
			}
		}

		rkfs_debug("Freeing block: %d (bit: %d)\n", nblkno, bit);
		if (!rkfs_clear_bit(bit, rkfs_dsb->s_block_map)) {
			rkfs_bug("Block %d (bit %d) is already free\n", nblkno,
				 bit);
			goto out;
		}

		if (vfs_inode)
			DQUOT_FREE_BLOCK(vfs_inode, 1);
	}

	mark_buffer_dirty(bh);

	if (vfs_sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block(WRITE, 1, &bh);
		wait_on_buffer(bh);
	}

	rkfs_debug("Freed blocks from %d to %d\n", blkno, (blkno + count - 1));
	return 0;

 out:
	FAILED;
	return err;
}

int rkfs_free_blocks(struct inode *vfs_inode, unsigned short blkno,
		     unsigned short count)
{
	struct super_block *vfs_sb = NULL;
	int err = -EIO;

	rkfs_debug("Block: %d, Count: %d\n", blkno, count);

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		goto out;
	}

	if (!(vfs_sb = vfs_inode->i_sb)) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	lock_super(vfs_sb);
	err = rkfs__free_blocks(vfs_sb, vfs_inode, blkno, count);
	unlock_super(vfs_sb);

 out:
	return err;
}

int rkfs_free_inode_block(struct super_block *vfs_sb, unsigned short iblkno)
{
	int err = -EIO;

	rkfs_debug("Inode block: %d\n", iblkno);

	if (!vfs_sb) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	lock_super(vfs_sb);
	err = rkfs__free_blocks(vfs_sb, NULL, iblkno, 1);
	unlock_super(vfs_sb);
	return err;

 out:
	return err;
}

int rkfs__new_block(struct super_block *vfs_sb,
		    struct inode *vfs_inode, unsigned short *res_blkno)
{
	struct buffer_head *bh = NULL;
	struct rkfs_super_block *rkfs_dsb = NULL;
	unsigned short i = 0, rkfs_sb_count = 0;
	unsigned short tb = 0, bit = 0, blkno = 0;
	unsigned short fb_found = 0;
	int err = -EIO;

	rkfs_debug("New block requested...\n");

	*res_blkno = 0;
	if (!vfs_sb) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;

	for (i = 0; i < rkfs_sb_count; i++) {
		if (!(bh = vfs_sb->u.rkfs_sb.s_sbh[i])) {
			rkfs_bug("%s superblock not in memory\n", RKFS_NAME);
			goto out;
		}

		rkfs_dsb = (struct rkfs_super_block *)((char *)bh->b_data);
		if (rkfs_dsb->s_fsid != RKFS_ID) {
			rkfs_bug("No valid %s superblock found\n", RKFS_NAME);
			goto out;
		}

		tb = rkfs_dsb->s_total_blocks;
		bit = rkfs_find_first_zero_bit(rkfs_dsb->s_block_map,
					       RKFS_MIN_BLOCKS);

		if (i) {
			if (bit == 0 || bit >= RKFS_MIN_BLOCKS)
				continue;
		} else {
			if (bit < RKFS_FIRST_BLOCK || bit >= RKFS_MIN_BLOCKS)
				continue;
		}

		blkno = bit + (i * RKFS_MIN_BLOCKS);
		if (blkno >= tb)
			continue;

		if (rkfs_set_bit(bit, rkfs_dsb->s_block_map)) {
			rkfs_bug("Block %d (bit: %d) already allocated\n",
				 blkno, bit);
			goto out;
		}

		if (vfs_inode)
			DQUOT_ALLOC_BLOCK(vfs_inode, 1);

		fb_found = 1;
		break;
	}

	if (!fb_found) {
		rkfs_debug("No free blocks found in the device %s\n",
			   bdevname(vfs_sb->s_dev));
		err = -ENOSPC;
		goto out;
	}

	*res_blkno = blkno;
	mark_buffer_dirty(bh);
	if (vfs_sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block(WRITE, 1, &bh);
		wait_on_buffer(bh);
	}

	rkfs_debug("Found free block: %d (bit: %d)\n", *res_blkno, bit);
	return 0;

 out:
	FAILED;
	return err;
}

int rkfs_new_block(struct inode *vfs_inode, unsigned short *res_blkno)
{
	struct super_block *vfs_sb = NULL;
	int err = -EIO;

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		goto out;
	}

	if (!(vfs_sb = vfs_inode->i_sb)) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	lock_super(vfs_sb);
	err = rkfs__new_block(vfs_sb, vfs_inode, res_blkno);
	unlock_super(vfs_sb);

	return err;

 out:
	FAILED;
	return err;

}

int rkfs_new_inode_block(struct inode *vfs_inode, unsigned short *res_blkno)
{
	struct super_block *vfs_sb = NULL;
	int err = -EIO;

	if (!vfs_inode) {
		rkfs_bug("VFS inode is NULL\n");
		goto out;
	}

	if (!(vfs_sb = vfs_inode->i_sb)) {
		rkfs_bug("VFS superblock is NULL\n");
		goto out;
	}

	err = rkfs__new_block(vfs_sb, NULL, res_blkno);
	return err;

 out:
	FAILED;
	return err;
}
