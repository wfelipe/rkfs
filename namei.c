/*
*
* namei.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserve.
*
*/

#include <linux/fs.h>
#include <linux/pagemap.h>

#include <linux/rkfs.h>

void rkfs_inc_count(struct inode *inode)
{
    inode->i_nlink++;
    mark_inode_dirty(inode);
}

void rkfs_dec_count(struct inode *inode)
{
    inode->i_nlink--;
    mark_inode_dirty(inode);
}

int rkfs_add_nondir(struct dentry *dentry, struct inode *inode)
{
    int err = 0;

    err = rkfs_add_link(dentry,inode);
    if( !err ) {
        d_instantiate(dentry,inode);
        return 0;
    }

    rkfs_dec_count(inode);
    iput(inode);

    return err;
}

struct dentry *rkfs_lookup(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = NULL;
    ino_t ino = 0;

    if( dentry->d_name.len >= RKFS_MAX_FILENAME_LEN)
        return ERR_PTR(-ENAMETOOLONG);

    ino = rkfs_inode_by_name(dir,dentry);
    if( ino ) {
        inode = iget(dir->i_sb,ino);
        if( !inode )
            return ERR_PTR(-EACCES);
    }

    d_add(dentry,inode);
    return NULL;
}

int rkfs_create(struct inode *dir, struct dentry *dentry, int mode)
{
    struct inode *inode = NULL;
    int err = 0;

    err = rkfs_new_inode(dir,mode,&inode);
    if( err )
        return err;

    err = PTR_ERR(inode);
    if( !IS_ERR(inode) ) {
        inode->i_op = &rkfs_file_inode_operations;
        inode->i_fop = &rkfs_file_operations;
        inode->i_mapping->a_ops = &rkfs_aops;

        mark_inode_dirty(inode);
        err = rkfs_add_nondir(dentry,inode);
    }

    return err;
}

int rkfs_mknod(struct inode *dir, struct dentry *dentry, int mode, int rdev)
{
    struct inode *inode = NULL;
    int err = 0;

    err = rkfs_new_inode(dir,mode,&inode);
    if( err )
        return err;

    err = PTR_ERR(inode);
    if( !IS_ERR(inode) ) {
        init_special_inode(inode,mode,rdev);

        mark_inode_dirty(inode);
        err = rkfs_add_nondir(dentry,inode);
    }

    return err;
}

int rkfs_symlink(struct inode *dir, struct dentry *dentry, const char *sname)
{
    struct super_block *sb =  NULL;
    int err = -ENAMETOOLONG;
    unsigned len = 0;
    struct inode *inode = NULL;

    sb = dir->i_sb;
    len = strlen(sname) + 1;

    if( len > sb->s_blocksize )
        goto out;

    err = rkfs_new_inode(dir,S_IFLNK | S_IRWXUGO,&inode);
    if( err )
        return err;

    err = PTR_ERR(inode);
    if( IS_ERR(inode) )
        goto out;

    inode->i_op = &page_symlink_inode_operations;
    inode->i_mapping->a_ops = &rkfs_aops;
    err = block_symlink(inode,sname,len);
    if( err )
        goto out_fail;

    mark_inode_dirty(inode);
    err = rkfs_add_nondir(dentry,inode);

out:
    return err;

out_fail:
    rkfs_dec_count(inode);
    iput(inode);
    return err;
}

int rkfs_link(struct dentry *old_dentry, struct inode *dir,
               struct dentry *dentry)
{
    struct inode *inode = NULL;
    int err = 0;

    inode = old_dentry->d_inode;
    if( S_ISDIR(inode->i_mode) )
        return -EPERM;

    if( inode->i_nlink >= RKFS_LINK_MAX )
        return -EMLINK;

    inode->i_ctime = CURRENT_TIME;
    rkfs_inc_count(inode);
    atomic_inc(&inode->i_count);

    err = rkfs_add_nondir(dentry,inode);

    return err;
}

int rkfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
    struct inode *inode = NULL;
    int err = -EMLINK;

    if( dir->i_nlink >= RKFS_LINK_MAX )
        goto out;

    rkfs_inc_count(dir);

    err = rkfs_new_inode(dir,S_IFDIR | mode,&inode);
    if( err )
        goto out_dir;

    inode->i_op = &rkfs_dir_inode_operations;
    inode->i_fop = &rkfs_dir_operations;
    inode->i_mapping->a_ops = &rkfs_aops;

    rkfs_inc_count(inode);

    err = rkfs_make_empty(inode,dir);
    if( err )
        goto out_fail;

    err = rkfs_add_link(dentry,inode);
    if( err )
        goto out_fail;

    d_instantiate(dentry,inode);

out:
    return err;

out_fail:
    rkfs_dec_count(inode);
    rkfs_dec_count(inode);
    iput(inode);

out_dir:
    rkfs_dec_count(dir);
    return err;
}

int rkfs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = NULL;
    struct rkfs_dir_entry *de = NULL;
    struct page *page = NULL;
    int err = -ENOENT;

    inode = dentry->d_inode;

    de = rkfs_find_entry(dir,dentry,&page);
    if( !de )
        goto out;

    err = rkfs_delete_entry(de,page);
    if( err )
        goto out;

    inode->i_ctime = dir->i_ctime;
    rkfs_dec_count(inode);
    err = 0;

out:
    return err;
}

int rkfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = NULL;
    int err = -ENOTEMPTY;

    inode = dentry->d_inode;

    if( rkfs_empty_dir(inode) ) {
        err = rkfs_unlink(dir,dentry);
        if( !err ) {
            inode->i_size = 0;
            rkfs_dec_count(inode);
            rkfs_dec_count(dir);
        }
    }

    return err;
}

int rkfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                 struct inode *new_dir, struct dentry *new_dentry )
{
    struct inode *old_inode = NULL;
    struct inode *new_inode = NULL;
    struct page *dir_page = NULL;
    struct rkfs_dir_entry *dir_de = NULL;
    struct page *old_page = NULL;
    struct rkfs_dir_entry *old_de = NULL;
    struct page *new_page = NULL;
    struct rkfs_dir_entry *new_de = NULL;
    struct rkfs_dir_entry *de = NULL;
    int err = -ENOENT;
    unsigned rec_len = 0;
    char *kaddr = NULL;

    old_inode = old_dentry->d_inode;
    new_inode = new_dentry->d_inode;

    old_de = rkfs_find_entry(old_dir,old_dentry,&old_page);
    if( !old_de )
        goto out;

    if( S_ISDIR(old_inode->i_mode) ) {
        err = -EIO;
        dir_page = rkfs_get_page(old_inode,0);
        if( IS_ERR(dir_page) )
            goto out_old;
        kaddr = page_address(dir_page);
        de = (struct rkfs_dir_entry *) kaddr;
        if( !de->de_name_len ) {
            err = -EINVAL;
            goto out_old;
        }
        rec_len = RKFS_DIR_ENTRY_LEN(de->de_name_len);
        dir_de = (struct rkfs_dir_entry *) (((char *) de) + rec_len);
    }

    if( new_inode ) {
        err = -ENOTEMPTY;
        if( dir_de && !rkfs_empty_dir(new_inode) )
            goto out_dir;

        err = -ENOENT;
        new_de = rkfs_find_entry(new_dir,new_dentry,&new_page);
        if( !new_de )
            goto out_dir;

        rkfs_inc_count(old_inode);
        rkfs_set_link(new_dir,new_de,new_page,old_inode);

        new_inode->i_ctime = CURRENT_TIME;

        if( dir_de )
            new_inode->i_nlink--;

        rkfs_dec_count(new_inode);
    } else {
        if( dir_de ) {
            err = -EMLINK;
            if( new_dir->i_nlink >= RKFS_LINK_MAX )
                goto out_dir;
        }

        rkfs_inc_count(old_inode);

        err = rkfs_add_link(new_dentry,old_inode);
        if( err ) {
            rkfs_dec_count(old_inode);
            goto out_dir;
        }

        if( dir_de )
            rkfs_inc_count(new_dir);
    }

    rkfs_delete_entry(old_de,old_page);
    rkfs_dec_count(old_inode);

    if( dir_de ) {
        rkfs_set_link(old_inode,dir_de,dir_page,new_dir);
        rkfs_dec_count(old_dir);
    }

    return 0;

out_dir:
    if( dir_de )
        rkfs_put_page(dir_page);

out_old:
    rkfs_put_page(old_page);

out:
    return err;
}

struct inode_operations rkfs_dir_inode_operations = {
    create:  rkfs_create,
    lookup:  rkfs_lookup,
    link:    rkfs_link,
    unlink:  rkfs_unlink,
    symlink: rkfs_symlink,
    mkdir:   rkfs_mkdir,
    rmdir:   rkfs_rmdir,
    mknod:   rkfs_mknod,
    rename:  rkfs_rename,
};
