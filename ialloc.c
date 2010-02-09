/*
*
* ialloc.c
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

#include <linux/rkfs.h>

int rkfs_free_inode(struct inode *vfs_inode,
                     unsigned short *res_icount, unsigned short *res_iblkno)
{
    struct super_block *vfs_sb = NULL;
    struct buffer_head *bh = NULL;
    struct rkfs_super_block *rkfs_dsb = NULL;
    unsigned short rkfs_sb_index = 0, rkfs_sb_count = 0;
    unsigned short bit = 0, blkno = 0, itable_index = 0;
    int err = -EIO;

    *res_icount = 0;
    *res_iblkno = 0;
    if( !vfs_inode ) {
        rkfs_bug("VFS inode is NULL\n");
        goto out;
    }

    rkfs_debug("Inode %ld\n",vfs_inode->i_ino);

    if( !(vfs_sb=vfs_inode->i_sb) ) {
        rkfs_bug("VFS superblock is NULL\n");
        goto out;
    }

    lock_super(vfs_sb);
    clear_inode(vfs_inode);

    if( is_bad_inode(vfs_inode) ) {
        rkfs_bug("Cannot free bad inode\n");
        goto unlock_and_out;
    }

    rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;
    rkfs_sb_index = vfs_inode->i_ino / RKFS_MIN_BLOCKS;

    if( rkfs_sb_index > (rkfs_sb_count - 1) ) {
        rkfs_bug("Inode %ld is bad (invalid %s sb index)\n",
                  vfs_inode->i_ino,RKFS_NAME);
        goto unlock_and_out;
    }

    if( !(bh=vfs_sb->u.rkfs_sb.s_sbh[rkfs_sb_index]) ) {
        rkfs_bug("%s superblock not in memory\n",RKFS_NAME);
        goto unlock_and_out;
    }

    rkfs_dsb = (struct rkfs_super_block *) ((char *) bh->b_data);
    if( rkfs_dsb->s_fsid != RKFS_ID ) {
        rkfs_bug("No valid %s superblock found\n",RKFS_NAME);
        goto unlock_and_out;
    }

    bit = vfs_inode->i_ino % RKFS_MIN_BLOCKS;
    if( rkfs_sb_index ) {
        if( bit == 0 ) {
            rkfs_bug("Inode %ld (bit %d) is bad (can't free resv. inode)\n",
                      vfs_inode->i_ino,bit);
            goto unlock_and_out;
        }
    } else {
        if( bit < (RKFS_FIRST_INODE - 1) ) {
            rkfs_bug("Inode %ld (bit %d) is bad (can't free resv. inode)\n",
                      vfs_inode->i_ino,bit);
            goto unlock_and_out;
        }
    }

    itable_index = bit / RKFS_INODES_PER_BLOCK;
    if( !(blkno=rkfs_dsb->s_itable_map[itable_index][0]) ) {
        rkfs_bug("Inode %ld is bad (no inode block)\n",vfs_inode->i_ino);
        goto unlock_and_out;
    }

    rkfs_debug("Freeing inode %ld (bit %d)\n",vfs_inode->i_ino,bit);
    if( !(rkfs_clear_bit(bit,rkfs_dsb->s_inode_map)) ) {
        rkfs_bug("Inode %ld (bit %d) is already free\n",vfs_inode->i_ino,bit);
        goto unlock_and_out;
    }

    rkfs_dsb->s_itable_map[itable_index][1] -= 1;
    rkfs_debug("Inode count at index: %d is %d\n",itable_index,
                rkfs_dsb->s_itable_map[itable_index][1]);

    *res_icount = rkfs_dsb->s_itable_map[itable_index][1];
    *res_iblkno = blkno;

    if( *res_icount == 0 )
        rkfs_dsb->s_itable_map[itable_index][0] = 0;

    mark_buffer_dirty(bh);
    if( vfs_sb->s_flags & MS_SYNCHRONOUS ) {
        ll_rw_block(WRITE,1,&bh);
        wait_on_buffer(bh);
    }

    unlock_super(vfs_sb);
    rkfs_debug("Inode %ld (bit %d) freed\n",vfs_inode->i_ino,bit);
    return 0;

unlock_and_out:
    unlock_super(vfs_sb);

out:
    FAILED;
    return err;
}

int rkfs_new_inode(struct inode *vfs_pinode, int mode,
                           struct inode **vfs_cinode)
{
    struct super_block *vfs_sb = NULL;
    struct buffer_head *bh = NULL;
    struct rkfs_super_block *rkfs_dsb = NULL;
    unsigned short i = 0, blkno = 0, bit = 0, itable_index = 0;
    unsigned short rkfs_sb_count = 0, rkfs_sb_index = 0;
    unsigned short fi_found = 0;
    unsigned long cino = 0;
    int err = -EIO;

    rkfs_debug("New inode requested...\n");

    if( !vfs_pinode ) {
        rkfs_bug("VFS inode is NULL\n");
        goto out;
    }

    *vfs_cinode = NULL;
    if( !(vfs_sb=vfs_pinode->i_sb) ) {
        rkfs_bug("VFS superblock is NULL\n");
        goto out;
    }

    lock_super(vfs_sb);

    if( !(*vfs_cinode=new_inode(vfs_sb)) ) {
        rkfs_debug("Can't create new empty VFS inode\n");
        err = -ENOSPC;
        goto unlock_and_out;
    }

    err = -EIO;
    rkfs_sb_count = vfs_sb->u.rkfs_sb.s_sb_count;
    rkfs_sb_index = vfs_pinode->i_ino / RKFS_MIN_BLOCKS;
    if( rkfs_sb_index > (rkfs_sb_count - 1) ) {
        rkfs_bug("Parent inode %ld is bad (invalid %s sb index)\n",
                  vfs_pinode->i_ino,RKFS_NAME);
        goto put_unlock_and_out;
    }

    while(1) {
        if( !(bh=vfs_sb->u.rkfs_sb.s_sbh[rkfs_sb_index]) ) {
            rkfs_bug("%s superblock not in memory\n",RKFS_NAME);
            goto put_unlock_and_out;
        }

        rkfs_dsb = (struct rkfs_super_block *) ((char *) bh->b_data);
        if( rkfs_dsb->s_fsid != RKFS_ID ) {
            rkfs_bug("No valid %s superblock found\n",RKFS_NAME);
            goto put_unlock_and_out;
        }

        if( !i ) {
            bit = vfs_pinode->i_ino % RKFS_MIN_BLOCKS;
            itable_index = bit / RKFS_INODES_PER_BLOCK;
            if( !(blkno=rkfs_dsb->s_itable_map[itable_index][0]) ) {
                rkfs_bug("Parent inode %ld (bit %d) is bad (no inode blk)\n",
                          vfs_pinode->i_ino,bit);
                goto put_unlock_and_out;
            }
        }

        bit = rkfs_find_first_zero_bit(rkfs_dsb->s_inode_map,
                                        RKFS_MIN_BLOCKS);

        if( rkfs_sb_index && bit > 0 && bit < RKFS_MIN_BLOCKS ) {
            fi_found = 1;
            break;
        }

        if( !rkfs_sb_index && bit >= RKFS_FIRST_INODE &&
                               bit < RKFS_MIN_BLOCKS ) {
            fi_found = 1;
            break;
        }

        if( !i ) {
            rkfs_sb_index = 0;
            i++;
        } else
            rkfs_sb_index++;

        if( rkfs_sb_index >= rkfs_sb_count )
            break;
    }

    if( !fi_found ) {
        rkfs_debug("No free inode found in the device %s\n",
                    bdevname(vfs_pinode->i_dev));
        err = -ENOSPC;
        goto put_unlock_and_out;
    }

    itable_index = bit / RKFS_INODES_PER_BLOCK;
    if( !(blkno=rkfs_dsb->s_itable_map[itable_index][0]) ) {
        rkfs_debug("Allocating new inode block...\n");
        err = rkfs_new_inode_block(vfs_pinode,&blkno);
        if( err ) {
            rkfs_debug("Can't get new inode block\n");
            goto put_unlock_and_out;
        }
        rkfs_dsb->s_itable_map[itable_index][0] = blkno;
    }

    cino = bit + (rkfs_sb_index * RKFS_MIN_BLOCKS);
    if( rkfs_set_bit(bit,rkfs_dsb->s_inode_map) ) {
        rkfs_bug("New inode %ld (bit %d) is already in use\n",cino,bit);
        err = -EIO;
        goto put_unlock_and_out;
    }

    rkfs_dsb->s_itable_map[itable_index][1] += 1;
    rkfs_debug("Inode count at index: %d is %d\n",itable_index,
                rkfs_dsb->s_itable_map[itable_index][1]);

    mark_buffer_dirty(bh);
    if( vfs_sb->s_flags & MS_SYNCHRONOUS ) {
        ll_rw_block(WRITE,1,&bh);
        wait_on_buffer(bh);
    }

    (*vfs_cinode)->i_ino = cino;
    (*vfs_cinode)->i_uid = current->fsuid;
    (*vfs_cinode)->i_gid = (vfs_pinode->i_mode & S_ISGID) ?
                           vfs_pinode->i_gid : current->fsgid;
    (*vfs_cinode)->i_mode = mode;
    (*vfs_cinode)->i_mtime = CURRENT_TIME;
    (*vfs_cinode)->i_ctime = CURRENT_TIME;
    (*vfs_cinode)->i_atime = CURRENT_TIME;
    (*vfs_cinode)->i_size = 0;
    (*vfs_cinode)->i_blocks = 0;
    (*vfs_cinode)->i_blksize = PAGE_SIZE;

    for( i=0; i<RKFS_N_BLOCKS; i++ )
        (*vfs_cinode)->u.rkfs_i.i_block[i] = 0;

    insert_inode_hash(*vfs_cinode);
    mark_inode_dirty(*vfs_cinode);

    unlock_super(vfs_sb);
    rkfs_debug("New inode is: %ld\n",(*vfs_cinode)->i_ino);
    return 0;

put_unlock_and_out:
    iput(*vfs_cinode);

unlock_and_out:
    unlock_super(vfs_sb);

out:
    FAILED;
    return err;
}
