/*
*
* itree.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>

#include <linux/rkfs.h>


/*
* A confusing data-structure for inode tree used by rkfs_get_block,
* which is parent for address space operations :-)
*/
typedef __u16 block_t;

typedef struct Indirect {
    block_t *p;
    block_t key;
    struct buffer_head *bh;
} Indirect;

#define DEPTH 3
#define DIRECT 39


inline void rkfs_add_chain(Indirect *p, struct buffer_head *bh,
                                   block_t *v)
{
    p->key = *(p->p = v);
    p->bh = bh;
}

inline int rkfs_verify_chain(Indirect *from, Indirect *to)
{
    while( (from <= to) && (from->key == *from->p) )
        from++;

    return( (from > to) );
}

inline Indirect *rkfs_get_branch(struct inode *vfs_inode,
					 int depth,
					 int *offsets,
					 Indirect chain[DEPTH],
					 int *err)
{
    kdev_t dev = vfs_inode->i_dev;
    Indirect *p = chain;
    struct buffer_head *bh;

    *err = 0;

    rkfs_add_chain(chain,NULL,vfs_inode->u.rkfs_i.i_block+(*offsets));
    if( !p->key )
        goto no_block;

    while( --depth ) {
        if( !(bh=bread(dev,p->key,RKFS_BLOCK_SIZE)) ) {
            rkfs_debug("Failed to read block %d (bread failed)\n",p->key);
            goto failure;
        }

        if( !rkfs_verify_chain(chain, p) )
            goto changed;

        rkfs_add_chain(++p,bh,(block_t *)bh->b_data+(*++offsets));
        if( !p->key )
            goto no_block;
    }

    return NULL;

changed:
    *err = -EAGAIN;
    goto no_block;

failure:
    *err = -EIO;

no_block:
    return p;
}

int rkfs_alloc_branch(struct inode *vfs_inode,
			      int num,
			      int *offsets,
			      Indirect *branch)
{
    int n = 0, i = 0, err = 0;
    unsigned short parent = 0, nr = 0;
    struct buffer_head *bh = NULL;

    err = rkfs_new_block(vfs_inode,&parent);
    if( err )
        return err;

    branch[0].key = parent;
    for( n=1; n<num; n++ ) {
        err = rkfs_new_block(vfs_inode,&nr);
        if( err )
            break;

        branch[n].key = nr;

        bh = getblk(vfs_inode->i_dev,parent,RKFS_BLOCK_SIZE);

        lock_buffer(bh);
        memset(bh->b_data,0,RKFS_BLOCK_SIZE);
        branch[n].bh = bh;
        branch[n].p = (block_t*) bh->b_data + offsets[n];
        *branch[n].p = branch[n].key;
        mark_buffer_uptodate(bh,1);
        unlock_buffer(bh);

        mark_buffer_dirty_inode(bh,vfs_inode);
        parent = nr;
    }

    if( n == num )
        return 0;

    /*
    * Allocation failed, free what we already allocated.
    */
    for( i=1; i<n; i++ )
        bforget(branch[i].bh);

    for( i=0; i<n; i++ )
        rkfs_free_blocks(vfs_inode,branch[i].key,1);

    return err;
}

inline int rkfs_splice_branch(struct inode *vfs_inode,
                               Indirect chain[DEPTH],
                               Indirect *where,
                               int num)
{
    int i = 0;

    if( !rkfs_verify_chain(chain,where-1) || *where->p )
        goto changed;

    *where->p = where->key;

    vfs_inode->i_ctime = CURRENT_TIME;

    if( where->bh )
        mark_buffer_dirty_inode(where->bh,vfs_inode);

    mark_inode_dirty(vfs_inode);
    return 0;

changed:
    for( i=1; i<num; i++ )
        bforget(where[i].bh);

    for( i=0; i<num; i++ )
        rkfs_free_blocks(vfs_inode,where[i].key,1);

    return -EAGAIN;
}

int rkfs_block_to_path(struct inode *vfs_inode, long blkno,
                        int offsets[DEPTH])
{
    int n = 0;
    unsigned short tb = 0;
    struct super_block *vfs_sb = NULL;
    struct buffer_head *bh = NULL;
    struct rkfs_super_block *rkfs_dsb = NULL;

    if( !vfs_inode ) {
        rkfs_bug("VFS inode is NULL\n");
        return n;
    }

    if( !(vfs_sb=vfs_inode->i_sb) ) {
        rkfs_bug("VFS superblock is NULL\n");
        return n;
    }

    if( !(bh=vfs_sb->u.rkfs_sb.s_sbh[0]) ) {
        rkfs_bug("No %s superblock in memory\n",RKFS_NAME);
        return n;
    }

    rkfs_dsb = (struct rkfs_super_block *) ((char *)bh->b_data);
    if( rkfs_dsb->s_fsid != RKFS_ID ) {
        rkfs_bug("No valid %s superblock found\n",RKFS_NAME);
        return n;
    }

    tb = rkfs_dsb->s_total_blocks;

    if( blkno < 0 ) {
        rkfs_debug("Block %ld is bad (<0)\n",blkno);
    } else if( blkno >= tb ) {
        rkfs_debug("Block %ld is bad (>=%d)\n",blkno,tb);
    } else if( blkno < (RKFS_N_BLOCKS - 2) ) {
        offsets[n++] = blkno;
    } else if( (blkno -= (RKFS_N_BLOCKS - 2)) < (RKFS_BLOCK_SIZE / 2) ) {
        offsets[n++] = RKFS_N_BLOCKS - 2;
        offsets[n++] = blkno;
    } else {
        blkno -= RKFS_BLOCK_SIZE / 2;
        offsets[n++] = RKFS_N_BLOCKS - 1;
        offsets[n++] = blkno >> 9;
        offsets[n++] = blkno & ((RKFS_BLOCK_SIZE / 2) - 1);
    }

    return n;
}

inline int rkfs_get_block(struct inode *vfs_inode, long blkno,
                           struct buffer_head *bh_result, int create)
{
    int err = -EIO;
    int offsets[DEPTH];
    Indirect chain[DEPTH];
    Indirect *partial = NULL;
    int left = 0, depth = 0;

    rkfs_debug("Inode: %ld, Block: %ld, Create: %d\n",vfs_inode->i_ino,blkno,
                create);

    depth = rkfs_block_to_path(vfs_inode,blkno,offsets);
    if( depth == 0 )
        goto out;

    lock_kernel();
reread:
    partial = rkfs_get_branch(vfs_inode,depth,offsets,chain,&err);

    /*
    * Simplest case - block found, no allocation needed
    */
    if( !partial ) {
got_it:
        bh_result->b_dev = vfs_inode->i_dev;
        bh_result->b_blocknr = chain[depth-1].key;
        bh_result->b_state |= (1UL << BH_Mapped);

        rkfs_debug("Result block: %ld\n",bh_result->b_blocknr);

        /*
        * Clean up and exit
        */
        partial = chain + depth - 1;
        goto cleanup;
     }

     /*
     * Next simple case - plain lookup or failed read of indirect block
     */
     if( !create || err == -EIO ) {
cleanup:
         while( partial > chain ) {
             brelse(partial->bh);
             partial--;
         }
         unlock_kernel();
out:
         return err;
     }

     /*
     * Indirect block might be removed by truncate while we were
     * reading it. Handling of that case (forget what we've got and
     * reread) is taken out of the main path.
     */
     if( err == -EAGAIN )
         goto changed;

     left = (chain + depth) - partial;
     err = rkfs_alloc_branch(vfs_inode,left,offsets + (partial - chain),
                              partial);
     if( err )
         goto cleanup;

     if( rkfs_splice_branch(vfs_inode,chain,partial,left) < 0 )
         goto changed;

     bh_result->b_state |= (1UL << BH_New);
     goto got_it;

changed:
     while( partial > chain ) {
         brelse(partial->bh);
         partial--;
     }
     goto reread;
}

int rkfs_all_zeroes(block_t *p, block_t *q)
{
    while( p < q )
        if( *p++ )
            return 0;

    return 1;
}

Indirect *rkfs_find_shared(struct inode *inode, int depth, int offsets[DEPTH],
                            Indirect chain[DEPTH], block_t *top)
{
    Indirect *partial = NULL, *p = NULL;
    int k = 0, err = 0;

    *top = 0;
    for( k=depth; (k>1 && !offsets[k-1]); k--);

    partial = rkfs_get_branch(inode,k,offsets,chain,&err);
    if( !partial )
        partial = chain + k - 1;

    if( !partial->key && *partial->p )
        goto no_top;

    for( p=partial;
         (p>chain && rkfs_all_zeroes((block_t*)p->bh->b_data,p->p));
         p-- );

    if( (p == chain + k - 1) && (p > chain) ) {
        p->p--;
    } else {
        *top = *p->p;
        *p->p = 0;
    }

    while( partial > p )
    {
        brelse(partial->bh);
        partial--;
    }

no_top:
    return partial;
}

void rkfs_free_data(struct inode *inode, block_t *p, block_t *q)
{
    unsigned short blk_to_free = 0, count = 0, blkno = 0;

    while( p < q ) {
        blkno = *p;

        if( blkno ) {
            *p = 0;

            if( count == 0 ) {
                blk_to_free = blkno;
                count++;
                p++;
                continue;
            }

            if( blkno == blk_to_free + count ) {
                count++;
                p++;
                continue;
            }

            mark_inode_dirty(inode);
            rkfs_free_blocks(inode,blk_to_free,count);
            blk_to_free = blkno;
            count = 1;
        }
        p++;
    }

    if( count > 0 ) {
        mark_inode_dirty(inode);
        rkfs_free_blocks(inode,blk_to_free,count);
    }
}

void rkfs_free_branches(struct inode *inode, block_t *p, block_t *q,
                         int depth)
{
    struct buffer_head *bh = NULL;
    unsigned short blkno = 0;

    if( depth-- ) {
        for( ; p<q ; p++ ) {
            blkno = *p;
            if( !blkno )
                continue;

            *p = 0;
            if( !(bh=bread(inode->i_dev,blkno,BLOCK_SIZE)) ) {
                rkfs_printk("Failed to read block %d from the device %s\n",blkno,
                             bdevname(inode->i_dev));
                continue;
            }

            rkfs_free_branches(inode,(block_t *)bh->b_data,
                              (block_t *)(bh->b_data + RKFS_BLOCK_SIZE),
                               depth);

            bforget(bh);
            rkfs_free_blocks(inode,blkno,1);
            mark_inode_dirty(inode);
        }
    } else
        rkfs_free_data(inode,p,q);
}

void rkfs_truncate(struct inode *inode)
{
    block_t *idata = inode->u.rkfs_i.i_block;
    int offsets[DEPTH];
    Indirect chain[DEPTH];
    Indirect *partial = NULL;
    block_t nr = 0;
    int n = 0, first_whole = 0, i = 0;
    long iblock = 0;

    rkfs_debug("Truncating inode: %ld (Size: %ld)\n",inode->i_ino,
                (ulong) inode->i_size);

    if( !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
        S_ISLNK(inode->i_mode)) ) {
        rkfs_debug("Inode: %ld is not file or dir or link (so no truncate)\n",
                    inode->i_ino);
        return;
    }

    for( i=0; i<DEPTH; i++ )
        offsets[i] = 0;

    memset(chain,0,(sizeof(chain)/sizeof(chain[0])));

    iblock = (inode->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    block_truncate_page(inode->i_mapping,inode->i_size,rkfs_get_block);

    n = rkfs_block_to_path(inode, iblock, offsets);
    if( !n )
        return;

    if( n == 1 ) {
        rkfs_free_data(inode,(idata + offsets[0]),(idata + DIRECT));
        first_whole = 0;
        goto do_indirects;
    }

    first_whole = offsets[0] + 1 - DIRECT;
    partial = rkfs_find_shared(inode,n,offsets,chain,&nr);
    if( nr ) {
        if( partial == chain )
            mark_inode_dirty(inode);
        else
            mark_buffer_dirty_inode(partial->bh,inode);

        rkfs_free_branches(inode,&nr,&nr+1,((chain + n - 1) - partial));
    }

    while( partial > chain ) {
        rkfs_free_branches(inode,(partial->p + 1),
            (block_t *) (partial->bh->b_data + RKFS_BLOCK_SIZE),
            ((chain + n - 1) - partial));

        mark_buffer_dirty_inode(partial->bh,inode);

        if( IS_SYNC(inode) ) {
            ll_rw_block(WRITE,1,&partial->bh);
            wait_on_buffer(partial->bh);
        }

        brelse(partial->bh);
        partial--;
    }

do_indirects:
    while( first_whole < (DEPTH-1) ) {
        nr = idata[DIRECT + first_whole];
        if( nr ) {
            idata[DIRECT + first_whole] = 0;
            mark_inode_dirty(inode);
            rkfs_free_branches(inode,&nr,&nr+1,(first_whole + 1));
        }
        first_whole++;
    }

    inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    mark_inode_dirty(inode);
}
