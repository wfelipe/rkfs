/*
*
* rkfs.h
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#ifndef _RKFS_H_
#define _RKFS_H_

#include <linux/statfs.h>
#include "rkfs_sb.h"

/*
* Following are inode related constants.
*/

/*
* Data blocks (Direct + Single indirect + Double indirect)
*
* If the value is changed here, then also change the value in
* 'rkfs_i.h' & 'itree.c'
*/
#define RKFS_N_BLOCKS        41

#define RKFS_INODE_SIZE      100
#define RKFS_LINK_MAX        32000

/*
* Structure of rkfs Inode (disk version)
*/
struct rkfs_inode {
	__u16 i_uid;		//User id
	__u16 i_gid;		//Group id
	__u16 i_mode;		//Type/access rights
	__u16 i_links_count;	//Number of childs
	__u32 i_time;		//Create/access/modification/del time
	__u32 i_size;		//Size in bytes
	__u16 i_blocks;		//Number of blocks
	__u16 i_block[RKFS_N_BLOCKS];	//Data blocks
};

/*
* Super Block / Inode related constants
*/
#define RKFS_ID        1811	//R=18 & K=11 :-)
#define RKFS_NAME      "rkfs"
#define RKFS_MAJOR_VER 1
#define RKFS_MINOR_VER 00
#define RKFS_VER       100	//Combination of major & minor ver
#define RKFS_DOB       "30/01/2002"	//rkfs date of birth

#define RKFS_MIN_BLOCKS 1440	//1.4MB
#define RKFS_MAX_BLOCKS 65535	//64MB

#define RKFS_BLOCK_SIZE 1024

#define RKFS_BLOCK_MAP_SIZE ((RKFS_MIN_BLOCKS/8)/2)
#define RKFS_INODE_MAP_SIZE RKFS_BLOCK_MAP_SIZE

#define RKFS_INODES_PER_BLOCK (RKFS_BLOCK_SIZE/RKFS_INODE_SIZE)

#define RKFS_INODE_TABLES_MAP_SIZE (RKFS_MIN_BLOCKS/RKFS_INODES_PER_BLOCK)

#define RKFS_VALID_FS  0x0001
#define RKFS_ERROR_FS  0x0002

#define RKFS_BOOT_SECTOR_BLOCK       0
#define RKFS_SUPER_BLOCK             1
#define RKFS_FIRST_INODE_TABLE_BLOCK 2
#define RKFS_ROOT_DIR_BLOCK          3	//Block for /. & /..
#define RKFS_FIRST_BLOCK             4
#define RKFS_BOOT_SECT_INO           0	//Boot sector block
#define RKFS_SB_INO                  1	//Superblock
#define RKFS_ITABLE_BLOCK_INO        2	//Inode table block
#define RKFS_ROOT_INO                3
#define RKFS_FIRST_INODE             (RKFS_ROOT_INO + 1)
#define RKFS_MIN_BLOCKS_PER_GROUP    3

/*
* Structure of rkfs Super Block (disk version)
*/
struct rkfs_super_block {
	__u16 s_fsid;		//Filesystem ID
	__u16 s_fsver;		//Filesystem version
	__u16 s_block_map[RKFS_BLOCK_MAP_SIZE];	//Block bitmap
	__u16 s_inode_map[RKFS_INODE_MAP_SIZE];	//Inode bitmap
	__u16 s_itable_map[RKFS_INODE_TABLES_MAP_SIZE][2];	//Inode table bitmap
	__u16 s_state;		//Filesystem state
	__u16 s_total_blocks;	//Total blocks
};

/*
* Directory entry related constants
*/
#define RKFS_MAX_FILENAME_LEN 252
#define RKFS_DIR_ENTRY_LEN(nlen) (nlen + 4)

#define RKFS_FT_UNKNOWN         0
#define RKFS_FT_REG_FILE        1
#define RKFS_FT_DIR             2
#define RKFS_FT_CHRDEV          3
#define RKFS_FT_BLKDEV          4
#define RKFS_FT_FIFO            5
#define RKFS_FT_SOCK            6
#define RKFS_FT_SYMLINK         7

#define RKFS_DIR_ENTRY_SIZE     256

/*
* Structure of rkfs directory entry (disk version)
*/
struct rkfs_dir_entry {
	__u16 de_inode;		//File inode number
	__u16 de_name_len;	//File name len
	char de_name[RKFS_MAX_FILENAME_LEN];	//File name
};

#define RKFS_DIR_ENTRY_PER_BLOCK (RKFS_BLOCK_SIZE/RKFS_DIR_ENTRY_SIZE)

#ifdef __KERNEL__
/*
* Following are required for rkfs in linux kernel.
* rkfs (user programs) nothing to do here. You have been warned.
*/

/*
* Temp buffer size.
*/
#define RKFS_BUF_SIZE     256
#define RKFS_BIG_BUF_SIZE 1024

/*
* Bit operations.
* In conventions these macros are defined in asm/bitops.h
*/
#define rkfs_set_bit                 __test_and_set_bit
#define rkfs_clear_bit               __test_and_clear_bit
#define rkfs_test_bit                test_bit
#define rkfs_find_first_zero_bit     find_first_zero_bit

/*
* Some useful macros....
*/
#define rkfs_max_file_size(tb,sb_count) (((tb - (sb_count * 2)) - 2) * 1024)

#define rkfs_printk(f,a...) \
        do { \
            printk("%s: %d: %s: ",__FILE__,__LINE__,__FUNCTION__); \
            printk(f,## a); \
        } while(0)

#ifdef __RKFS_DEBUG__
#define rkfs_debug(f,a...) \
        do { \
            printk("%s: %d: %s: ",__FILE__,__LINE__,__FUNCTION__); \
            printk(f,## a); \
        } while(0)
#else
#define rkfs_debug(f,a...)	//
#endif

#define rkfs_bug(f,a...) \
        do { \
            printk("\n\n>>>\n"); \
            printk("*** === *** === rkfs bug === *** === ***\n"); \
            printk("at: %s: %d: %s\n",__FILE__,__LINE__, \
                   __FUNCTION__); \
            printk(f,## a); \
            printk("Please report this bug to the author.\n"); \
            printk("To report, just copy this message\n"); \
            printk("from >>> to <<< and send it as an\n"); \
            printk("email to rajkanna_hcl@yahoo.com or rajark_hcl@yahoo.co.in\n"); \
            printk("*** === *** === rkfs bug === *** === ***\n"); \
            printk("<<<\n\n"); \
        } while(0)

#if 0
#define SUCCESS rkfs_debug("*** Success\n");
#endif
#define FAILED rkfs_debug("*** Failed\n");

/*
* rkf/utils.c
*/
void rkfs_dump_vfs_super_block(const struct super_block *sb,
			       const char *frmt, ...);
void rkfs_dump_rkfs_super_block(const struct rkfs_super_block *rkfs_sb,
				const char *frmt, ...);
void rkfs_dump_rkfs_inode(const struct rkfs_inode *rkfs_dinode,
			  const char *frmt, ...);
void rkfs_dump_vfs_inode(const struct inode *vfs_inode, const char *frmt, ...);
void rkfs_dump_rkfs_dir_entry(const struct rkfs_dir_entry *rkfs_de,
			      const char *frmt, ...);

/*
* rkf/bitmap.c
*/
unsigned short rkfs_count_free(void *map, unsigned short offset,
			       unsigned short total_blocks);

/*
* rkf/super.c
*/
extern struct super_operations rkfs_sops;
void rkfs_write_super(struct super_block *vfs_sb);
void rkfs_put_super(struct super_block *vfs_sb);
int rkfs_statfs(struct super_block *vfs_sb, struct statfs *sbuf);
struct super_block *rkfs_read_super(struct super_block *vfs_sb,
				    void *data, int silent);

/*
* rkf/inode.c
*/
void rkfs_read_inode(struct inode *vfs_inode);
int rkfs_update_inode(struct inode *vfs_inode, int sync);
void rkfs_write_inode(struct inode *vfs_inode, int sync);
void rkfs_put_inode(struct inode *vfs_inode);
void rkfs_delete_inode(struct inode *vfs_inode);
int rkfs_sync_inode(struct inode *vfs_inode);
void rkfs_clear_inode(struct inode *vfs_inode);

/*
* rkf/balloc.c
*/
int rkfs__free_blocks(struct super_block *vfs_sb,
		      struct inode *vfs_inode,
		      unsigned short blkno, unsigned short count);
int rkfs_free_blocks(struct inode *vfs_inode, unsigned short blkno,
		     unsigned short count);
int rkfs_free_inode_block(struct super_block *vfs_sb, unsigned short iblkno);
int rkfs__new_block(struct super_block *vfs_sb,
		    struct inode *vfs_inode, unsigned short *res_blkno);
int rkfs_new_block(struct inode *vfs_inode, unsigned short *res_blkno);
int rkfs_new_inode_block(struct inode *vfs_inode, unsigned short *res_blkno);

/*
* rkf/ialloc.c
*/
int rkfs_free_inode(struct inode *vfs_inode,
		    unsigned short *res_icount, unsigned short *res_iblkno);
int rkfs_new_inode(struct inode *vfs_pinode, int mode,
		   struct inode **vfs_cinode);

/*
* rkf/asops.c
*/
extern struct address_space_operations rkfs_aops;

/*
* rkf/itree.c
*/
extern int rkfs_get_block(struct inode *vfs_inode, long blkno,
			  struct buffer_head *bh_result, int create);
extern void rkfs_truncate(struct inode *vfs_inode);

/*
* rkf/file.c
*/
extern struct file_operations rkfs_file_operations;
extern struct inode_operations rkfs_file_inode_operations;
extern int rkfs_sync_file(struct file *file, struct dentry *dentry,
			  int datasync);

/*
* rkf/namei.c
*/
extern struct inode_operations rkfs_dir_inode_operations;
struct dentry *rkfs_lookup(struct inode *dir, struct dentry *dentry);
int rkfs_create(struct inode *dir, struct dentry *dentry, int mode);
int rkfs_mknod(struct inode *dir, struct dentry *dentry, int mode, int rdev);
int rkfs_symlink(struct inode *dir, struct dentry *dentry, const char *sname);
int rkfs_link(struct dentry *old_dentry, struct inode *dir,
	      struct dentry *dentry);
int rkfs_mkdir(struct inode *dir, struct dentry *dentry, int mode);
int rkfs_unlink(struct inode *dir, struct dentry *dentry);
int rkfs_rmdir(struct inode *dir, struct dentry *dentry);
int rkfs_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry);

/*
* rkf/dir.c
*/
#define RKFS_DIR_PAGES(inode) ((inode->i_size + PAGE_CACHE_SIZE - 1) \
                               / PAGE_CACHE_SIZE)
extern struct file_operations rkfs_dir_operations;
void rkfs_put_page(struct page *page);
int rkfs_commit_chunk(struct page *page, unsigned from, unsigned to);
struct page *rkfs_get_page(struct inode *vfs_pinode, unsigned long n);
int rkfs_readdir(struct file *filp, void *dirent, filldir_t filldir);
struct rkfs_dir_entry *rkfs_find_entry(struct inode *dir,
				       struct dentry *dentry,
				       struct page **res_page);
ino_t rkfs_inode_by_name(struct inode *dir, struct dentry *dentry);
void rkfs_set_link(struct inode *dir, struct rkfs_dir_entry *de,
		   struct page *page, struct inode *inode);
int rkfs_add_link(struct dentry *dentry, struct inode *inode);
int rkfs_delete_entry(struct rkfs_dir_entry *de, struct page *page);
int rkfs_make_empty(struct inode *inode, struct inode *parent);
int rkfs_empty_dir(struct inode *inode);

#endif				//__KERNEL__

#endif
