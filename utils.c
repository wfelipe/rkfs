/*
*
* utils.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include <rkfs.h>

void rkfs_dump_vfs_super_block(const struct super_block *sb,
			       const char *frmt, ...)
{
	va_list vptr;
	char buf[RKFS_BIG_BUF_SIZE];

	memset(buf, 0, RKFS_BIG_BUF_SIZE);
	va_start(vptr, frmt);
	vsprintf(buf, frmt, vptr);
	va_end(vptr);

	printk("\n%s\n", buf);
	printk("VFS superblock: Device: %s\n", bdevname(sb->s_dev));
	printk("VFS superblock: Block size: %ld\n", sb->s_blocksize);
	printk("VFS superblock: Block size bits: %u\n", sb->s_blocksize_bits);
	printk("VFS superblock: Dirt: %u\n", sb->s_dirt);
	printk("VFS superblock: Max filesize: %ld Kb\n",
	       (long)(sb->s_maxbytes / 1024));
	printk("VFS superblock: Magic: %ld\n", sb->s_magic);
}

void rkfs_dump_rkfs_super_block(const struct rkfs_super_block *rkfs_sb,
				const char *frmt, ...)
{
	va_list vptr;
	char buf[RKFS_BIG_BUF_SIZE];
	int i = 0, cnt = 0;

	memset(buf, 0, RKFS_BIG_BUF_SIZE);
	va_start(vptr, frmt);
	vsprintf(buf, frmt, vptr);
	va_end(vptr);

	printk("\n%s\n", buf);
	printk("rkfs superblock: Filesystem ID: %d\n", rkfs_sb->s_fsid);
	printk("rkfs superblock: Filesystem ver: %d\n", rkfs_sb->s_fsver);
	printk("rkfs superblock: Used blocks:\n");
	for (i = 0; i < RKFS_MIN_BLOCKS; i++) {
		if (rkfs_test_bit(i, (volatile void *)rkfs_sb->s_block_map)) {
			printk("%d ", i);
			cnt++;
			if (cnt > 15) {
				printk("\n");
				cnt = 0;
			}
		}
	}
	cnt = 0;
	printk("\nrkfs superblock: Used inodes:\n");
	for (i = 0; i < RKFS_MIN_BLOCKS; i++) {
		if (rkfs_test_bit(i, (volatile void *)rkfs_sb->s_inode_map)) {
			printk("%d ", i);
			cnt++;
			if (cnt > 15) {
				printk("\n");
				cnt = 0;
			}
		}
	}
	printk("\nrkfs superblock: Inode table map:\n");
	for (i = 0; i < RKFS_INODE_TABLES_MAP_SIZE; i++)
		if (rkfs_sb->s_itable_map[i][0] != 0)
			printk("Index: %d, Block: %d, Count: %d\n", i,
			       rkfs_sb->s_itable_map[i][0],
			       rkfs_sb->s_itable_map[i][1]);
	printk("rkfs superblock: Filesystem state: %d\n", rkfs_sb->s_state);
	printk("rkfs superblock: Total blocks: %d\n", rkfs_sb->s_total_blocks);
}

void rkfs_dump_rkfs_inode(const struct rkfs_inode *rkfs_dinode,
			  const char *frmt, ...)
{
	va_list vptr;
	char buf[RKFS_BIG_BUF_SIZE];
	int i = 0;

	memset(buf, 0, RKFS_BIG_BUF_SIZE);
	va_start(vptr, frmt);
	vsprintf(buf, frmt, vptr);
	va_end(vptr);

	printk("\n%s\n", buf);
	printk("rkfs inode: User ID: %d\n", rkfs_dinode->i_uid);
	printk("rkfs inode: Group ID: %d\n", rkfs_dinode->i_gid);
	printk("rkfs inode: Mode: %o\n", rkfs_dinode->i_mode);
	printk("rkfs inode: Links count: %d\n", rkfs_dinode->i_links_count);
	printk("rkfs inode: Time: %d\n", rkfs_dinode->i_time);
	printk("rkfs inode: Size: %d\n", rkfs_dinode->i_size);
	printk("rkfs inode: Blocks: %d\n", rkfs_dinode->i_blocks);
	printk("rkfs inode: Data blocks:\n");
	for (i = 0; i < RKFS_N_BLOCKS; i++)
		if (rkfs_dinode->i_block[i])
			printk("Index: %d, Block %d\n", i,
			       rkfs_dinode->i_block[i]);
}

void rkfs_dump_vfs_inode(const struct inode *vfs_inode, const char *frmt, ...)
{
	va_list vptr;
	char buf[RKFS_BIG_BUF_SIZE];

	memset(buf, 0, RKFS_BIG_BUF_SIZE);
	va_start(vptr, frmt);
	vsprintf(buf, frmt, vptr);
	va_end(vptr);

	printk("\n%s", buf);
	printk("VFS inode: Inode number: %ld\n", vfs_inode->i_ino);
	printk("VFS inode: Device: %s\n", bdevname(vfs_inode->i_dev));
	printk("VFS inode: Mode: %o\n", vfs_inode->i_mode);
	printk("VFS inode: Link count: %d\n", vfs_inode->i_nlink);
	printk("VFS inode: User ID: %d\n", vfs_inode->i_uid);
	printk("VFS inode: Group ID: %d\n", vfs_inode->i_gid);
	printk("VFS inode: Size: %ld\n", (long)vfs_inode->i_size);
	printk("VFS inode: Block bits: %d\n", vfs_inode->i_blkbits);
	printk("VFS inode: Block size: %ld\n", vfs_inode->i_blksize);
	printk("VFS inode: Blocks: %ld\n", vfs_inode->i_blocks);
	printk("VFS inode: Access time: %ld\n", (ulong) vfs_inode->i_atime);
	printk("VFS inode: Modification time: %ld\n",
	       (ulong) vfs_inode->i_mtime);
	printk("VFS inode: Change time: %ld\n", (ulong) vfs_inode->i_ctime);
}

/*
void rkfs_dump_buffer_head(const struct buffer_head *bh,
                            const char *frmt, ...)
{
    va_list vptr;
    char buf[RKFS_BIG_BUF_SIZE];

    if( !rkfs_debug )
        return;

    memset(buf,0,RKFS_BIG_BUF_SIZE);
    va_start(vptr,frmt);
    vsprintf(buf,frmt,vptr);
    va_end(vptr);

    printk("Dumping buffer head:\n%s\n",buf);
    printk("Block number: %ld\n",bh->b_blocknr);
    printk("Block size: %d\n",bh->b_size);
    printk("Device:  %s\n",bdevname(bh->b_dev));
}
*/

void rkfs_dump_rkfs_dir_entry(const struct rkfs_dir_entry *rkfs_de,
			      const char *frmt, ...)
{
	va_list vptr;
	char buf[RKFS_BIG_BUF_SIZE];

	memset(buf, 0, RKFS_BIG_BUF_SIZE);
	va_start(vptr, frmt);
	vsprintf(buf, frmt, vptr);
	va_end(vptr);

	printk("\n%s\n", buf);
	printk("Inode number: %d\n", rkfs_de->de_inode);
	printk("Name: %s\n", rkfs_de->de_name);
	printk("Name len: %d\n", rkfs_de->de_name_len);
}
