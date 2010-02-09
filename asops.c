/*
*
* asops.c
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
#include <linux/blkdev.h>

#include <linux/rkfs.h>

int rkfs_readpage(struct file *file, struct page *page)
{
    int rc = 0;

    if( (rc=block_read_full_page(page,rkfs_get_block)) )
        FAILED;

    return rc;
}

int rkfs_writepage(struct page *page)
{
    int rc = 0;

    if( (rc=block_write_full_page(page,rkfs_get_block)) )
        FAILED;

    return rc;
}

int rkfs_prepare_write(struct file *file, struct page *page,
                               unsigned from, unsigned to)
{
    int rc = 0;

    if( (rc=block_prepare_write(page,from,to,rkfs_get_block)) )
        FAILED;

    return rc;
}

int rkfs_bmap(struct address_space *mapping, long blkno)
{
    int rc = 0;

    if( (rc=generic_block_bmap(mapping,blkno,rkfs_get_block)) )
        FAILED;

    return rc;

}

struct address_space_operations rkfs_aops = {
    readpage:       rkfs_readpage,
    writepage:      rkfs_writepage,
    sync_page:      block_sync_page,
    prepare_write:  rkfs_prepare_write,
    commit_write:   generic_commit_write,
    bmap:           rkfs_bmap
};
