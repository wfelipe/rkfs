/*
*
* dir.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#include <linux/fs.h>
#include <linux/pagemap.h>

#include <rkfs.h>

void rkfs_put_page(struct page *page)
{
	kunmap(page);
	page_cache_release(page);
}

int rkfs_commit_chunk(struct page *page, unsigned from, unsigned to)
{
	struct inode *inode = NULL;
	int err = 0;

	inode = page->mapping->host;
	page->mapping->a_ops->commit_write(NULL, page, from, to);
	if (IS_SYNC(inode))
		err = waitfor_one_page(page);

	return err;
}

struct page *rkfs_get_page(struct inode *vfs_pinode, unsigned long n)
{
	struct address_space *mapping = NULL;
	struct page *page = NULL;

	mapping = vfs_pinode->i_mapping;

	page =
	    read_cache_page(mapping, n, (filler_t *) mapping->a_ops->readpage,
			    NULL);
	if (IS_ERR(page)) {
		rkfs_debug("Can't get page (read_cache_page failed)\n");
		goto error_out;
	}

	wait_on_page(page);
	kmap(page);

	if (!Page_Uptodate(page)) {
		rkfs_debug("Can't get page (page not uptodate)\n");
		goto fail;
	}

	if (PageError(page)) {
		rkfs_debug("Can't get page (page has errors)\n");
		goto fail;
	}

	return page;

 fail:
	FAILED;
	rkfs_put_page(page);
	return ERR_PTR(-EIO);

 error_out:
	FAILED;
	return ERR_PTR(-EIO);
}

int rkfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	loff_t pos = 0;
	struct inode *inode = NULL;
	unsigned offset = 0;
	unsigned long n = 0, npages = 0;
	char *ps_addr = NULL, *pe_addr = NULL, *p_addr = NULL;
	struct rkfs_dir_entry *de = NULL;
	struct page *page = NULL;
	int over = 0;

	inode = filp->f_dentry->d_inode;
	pos = filp->f_pos;

	n = pos / PAGE_CACHE_SIZE;
	offset = pos % PAGE_CACHE_SIZE;
	npages = RKFS_DIR_PAGES(inode);

	rkfs_debug
	    ("Inode: %ld, sz: %ld, pgs= %ld, pos= %ld, n = %ld, offt= %d\n",
	     inode->i_ino, (long)inode->i_size, npages, (long)pos, n, offset);

	if (pos >= inode->i_size)
		goto done;

	while (n < npages) {
		page = rkfs_get_page(inode, n);
		if (IS_ERR(page)) {
			rkfs_debug
			    ("Got page error while reading page no: %ld\n", n);
			continue;
		}

		ps_addr = page_address(page);
		pe_addr = ps_addr + PAGE_CACHE_SIZE - RKFS_DIR_ENTRY_LEN(1);

		p_addr = ps_addr + offset;

		rkfs_debug("ps_addr= %ld\n", (ulong) ps_addr);
		rkfs_debug("p_addr= %ld\n", (ulong) p_addr);
		rkfs_debug("pe_addr= %ld\n", (ulong) pe_addr);

		while (p_addr <= pe_addr) {
			de = (struct rkfs_dir_entry *)p_addr;

			if (de->de_inode) {
				offset = ((char *)de) - ps_addr;
				over =
				    filldir(dirent, de->de_name,
					    de->de_name_len,
					    ((n * PAGE_CACHE_SIZE) + offset),
					    de->de_inode, DT_UNKNOWN);

				if (over) {
					rkfs_put_page(page);
					rkfs_debug
					    ("filldir returned over=: %d\n",
					     over);
					goto done;
				}
			}

			if (de->de_name_len) {
				p_addr =
				    p_addr +
				    RKFS_DIR_ENTRY_LEN(de->de_name_len);
			} else {
				rkfs_debug
				    ("namelen = 0, so no next entry...\n");
				break;
			}
		}

		rkfs_put_page(page);

		n++;
		offset = 0;
	}

 done:
	filp->f_pos = (n * PAGE_CACHE_SIZE) + offset;
	UPDATE_ATIME(inode);
	return 0;
}

struct rkfs_dir_entry *rkfs_find_entry(struct inode *dir,
				       struct dentry *dentry,
				       struct page **res_page)
{
	char *name = NULL;
	unsigned namelen = 0;
	unsigned long n = 0, npages = 0;
	struct page *page = NULL;
	struct rkfs_dir_entry *de = NULL;
	char *ps_addr = NULL, *pe_addr = NULL, *p_addr = NULL;

	name = (char *)dentry->d_name.name;
	namelen = dentry->d_name.len;
	npages = RKFS_DIR_PAGES(dir);
	*res_page = NULL;

	for (n = 0; n < npages; n++) {
		page = rkfs_get_page(dir, n);
		if (IS_ERR(page)) {
			rkfs_debug
			    ("Got page error while reading page no: %ld\n", n);
			continue;
		}

		p_addr = ps_addr = page_address(page);
		pe_addr = ps_addr + PAGE_CACHE_SIZE - RKFS_DIR_ENTRY_LEN(1);

		while (p_addr <= pe_addr) {
			de = (struct rkfs_dir_entry *)p_addr;

			if (!de->de_inode && !de->de_name_len)
				break;

			if (de->de_inode && !de->de_name_len) {
				rkfs_bug
				    ("Invalid dir entry found in page %ld\n",
				     n);
				rkfs_put_page(page);
				FAILED;
				goto not_found;
			}

			if (de->de_inode && (de->de_name_len == namelen) &&
			    (strncmp(de->de_name, name, de->de_name_len) == 0))
				goto found;

			p_addr = p_addr + RKFS_DIR_ENTRY_LEN(de->de_name_len);
		}

		rkfs_put_page(page);
	}

 not_found:
	*res_page = NULL;
	return NULL;

 found:
	*res_page = page;
	return de;
}

ino_t rkfs_inode_by_name(struct inode * dir, struct dentry * dentry)
{
	ino_t res = 0;
	struct rkfs_dir_entry *de = NULL;
	struct page *page = NULL;

	de = rkfs_find_entry(dir, dentry, &page);
	if (de) {
		res = de->de_inode;
		rkfs_put_page(page);
		goto out;
	}

	rkfs_debug("Can't find inode for name %s\n", dentry->d_name.name);

 out:
	return res;
}

void rkfs_set_link(struct inode *dir, struct rkfs_dir_entry *de,
		   struct page *page, struct inode *inode)
{
	char *ps_addr = NULL;
	unsigned from = 0, to = 0;
	int err = 0;

	ps_addr = page_address(page);
	from = ((char *)de) - ps_addr;
	to = from + RKFS_DIR_ENTRY_LEN(de->de_name_len);

	lock_page(page);

	err = page->mapping->a_ops->prepare_write(NULL, page, from, to);
	if (err) {
		rkfs_bug("Can't link %s to %ld (prepare write failed)\n",
			 de->de_name, inode->i_ino);
		goto error_out;
	}

	de->de_inode = inode->i_ino;

	err = rkfs_commit_chunk(page, from, to);
	if (err) {
		rkfs_bug("Can't link %s to %ld (prepare write failed)\n",
			 de->de_name, inode->i_ino);
		goto error_out;
	}

	UnlockPage(page);
	rkfs_put_page(page);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	mark_inode_dirty(dir);

	return;

 error_out:
	FAILED;
	UnlockPage(page);
	rkfs_put_page(page);
	return;
}

int rkfs_add_link(struct dentry *dentry, struct inode *inode)
{
	struct inode *dir = NULL;
	char *name = NULL;
	unsigned namelen = 0, reclen = 0, rec_len = 0;
	struct page *page = NULL;
	struct rkfs_dir_entry *de = NULL;
	unsigned long npages = 0, n = 0;
	char *ps_addr = NULL, *pe_addr = NULL, *p_addr = NULL;
	unsigned from = 0, to = 0;
	int err = 0;

	dir = dentry->d_parent->d_inode;
	name = (char *)dentry->d_name.name;
	namelen = dentry->d_name.len;
	reclen = RKFS_DIR_ENTRY_LEN(namelen);
	npages = RKFS_DIR_PAGES(dir);

	for (n = 0; n <= npages; n++) {
		page = rkfs_get_page(dir, n);
		err = PTR_ERR(page);
		if (IS_ERR(page)) {
			rkfs_debug("Can't add entry: %s (page error)\n", name);
			FAILED;
			goto out;
		}

		p_addr = ps_addr = page_address(page);
		pe_addr = ps_addr + PAGE_CACHE_SIZE - RKFS_DIR_ENTRY_LEN(1);

		while (p_addr <= pe_addr) {
			de = (struct rkfs_dir_entry *)p_addr;

			if (de->de_inode) {
				if (!de->de_name_len) {
					err = -EIO;
					rkfs_bug
					    ("Invalid dir entry found in page %ld\n",
					     n);
					goto out_page;
				}

				if ((de->de_name_len == namelen) &&
				    (strncmp(de->de_name, name, de->de_name_len)
				     == 0)) {
					err = -EEXIST;
					goto out_page;
				}
			} else {
				if (!de->de_name_len) {
					rec_len = reclen;
					rkfs_debug
					    ("Got new free dir-entry slot\n");
					goto got_it;
				}

				rec_len = RKFS_DIR_ENTRY_LEN(de->de_name_len);
				if (rec_len == reclen) {
					rkfs_debug
					    ("Got exact free dir-entry slot of size: %d\n",
					     rec_len);
					goto got_it;
				}

				if ((rec_len > reclen) &&
				    (rec_len - reclen) >=
				    RKFS_DIR_ENTRY_LEN(1)) {
					rkfs_debug
					    ("Got free bigger dir-entry slot of size: %d\n",
					     rec_len);
					goto got_it;
				}

				rkfs_debug
				    ("Skipping empty dir-entry slot of size: %d\n",
				     rec_len);
			}

			p_addr = p_addr + RKFS_DIR_ENTRY_LEN(de->de_name_len);
		}

		rkfs_put_page(page);
	}

	rkfs_debug("Can't add entry: %s (no proper vacant space)\n", name);
	return -ENOSPC;

 got_it:
	from = ((char *)de) - ps_addr;

	if (rec_len == reclen)
		to = from + reclen;
	else
		to = from + rec_len;

	lock_page(page);
	err = page->mapping->a_ops->prepare_write(NULL, page, from, to);
	if (err) {
		rkfs_debug("Can't add entry: %s (prepare write failed)\n",
			   name);
		FAILED;
		goto out_unlock;
	}

	de->de_inode = inode->i_ino;
	de->de_name_len = namelen;
	memcpy(de->de_name, name, namelen);

	if (rec_len != reclen) {
		de = (struct rkfs_dir_entry *)(((char *)de) + reclen);
		de->de_inode = 0;
		de->de_name_len = rec_len - reclen - 4;
	}

	err = rkfs_commit_chunk(page, from, to);
	if (err) {
		rkfs_debug("Can't add entry: %s (commit chunk failed)\n", name);
		FAILED;
		goto out_unlock;
	}

	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
	mark_inode_dirty(dir);

 out_unlock:
	UnlockPage(page);

 out_page:
	rkfs_put_page(page);

 out:
	return err;
}

int rkfs_delete_entry(struct rkfs_dir_entry *de, struct page *page)
{
	struct address_space *mapping = NULL;
	struct inode *inode = NULL;
	char *ps_addr = NULL;
	unsigned from = 0, to = 0;
	int err = 0;

	mapping = page->mapping;
	inode = (struct inode *)mapping->host;
	ps_addr = page_address(page);

	from = ((char *)de) - ps_addr;
	to = from + RKFS_DIR_ENTRY_LEN(de->de_name_len);

	lock_page(page);

	err = mapping->a_ops->prepare_write(NULL, page, from, to);
	if (err) {
		rkfs_bug("Can't delete the entry: %s (prepare write failed)\n",
			 de->de_name);
		goto fail;
	}

	de->de_inode = 0;

	err = rkfs_commit_chunk(page, from, to);
	if (err) {
		rkfs_bug("Can't delete the entry: %s (commit chunk failed)\n",
			 de->de_name);
		goto fail;
	}

	UnlockPage(page);
	rkfs_put_page(page);

	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	mark_inode_dirty(inode);

	return err;

 fail:
	FAILED;
	UnlockPage(page);
	rkfs_put_page(page);
	return err;
}

int rkfs_make_empty(struct inode *inode, struct inode *parent)
{
	struct address_space *mapping = NULL;
	struct page *page = NULL;
	struct rkfs_dir_entry *de = NULL;
	char *base = NULL;
	int err = 0;

	mapping = inode->i_mapping;

	err = -ENOMEM;
	page = grab_cache_page(mapping, 0);
	if (!page) {
		rkfs_debug("Not enough memory to make empty directory\n");
		goto out;
	}

	err = mapping->a_ops->prepare_write(NULL, page, 0, RKFS_BLOCK_SIZE);
	if (err) {
		rkfs_debug
		    ("Can't make empty directory (fail to prepare write)\n");
		goto fail;
	}

	base = page_address(page);
	memset(base, 0, PAGE_CACHE_SIZE);

	de = (struct rkfs_dir_entry *)base;
	de->de_inode = inode->i_ino;
	de->de_name_len = 1;
	strcpy(de->de_name, ".");

	de = (struct rkfs_dir_entry *)(base + RKFS_DIR_ENTRY_LEN(1));
	de->de_inode = parent->i_ino;
	de->de_name_len = 2;
	strcpy(de->de_name, "..");

	err = rkfs_commit_chunk(page, 0, RKFS_BLOCK_SIZE);
	if (err) {
		rkfs_debug
		    ("Can't make empty directory (fail to commit chunk)\n");
		goto fail;
	}

	UnlockPage(page);
	page_cache_release(page);

	return err;

 fail:
	UnlockPage(page);
	page_cache_release(page);

 out:
	FAILED;
	return err;
}

int rkfs_empty_dir(struct inode *inode)
{
	struct page *page = NULL;
	unsigned long n = 0, npages = 0;
	char *ps_addr = NULL, *pe_addr = NULL, *p_addr = NULL;
	struct rkfs_dir_entry *de = NULL;

	npages = RKFS_DIR_PAGES(inode);

	for (n = 0; n < npages; n++) {
		page = rkfs_get_page(inode, n);
		if (IS_ERR(page)) {
			rkfs_debug("Got page error while reading page: %ld\n",
				   n);
			continue;
		}

		p_addr = ps_addr = page_address(page);
		pe_addr = ps_addr + PAGE_CACHE_SIZE - RKFS_DIR_ENTRY_LEN(1);

		while (p_addr <= pe_addr) {
			de = (struct rkfs_dir_entry *)p_addr;

			if (de->de_inode) {
				if (!de->de_name_len) {
					rkfs_bug
					    ("Invalid dir entry found at page %ld\n",
					     n);
					goto not_empty;
				}

				if ((de->de_name_len == 1)
				    && (de->de_name[0] != '.'))
					goto not_empty;

				if ((de->de_name_len == 2)
				    && (de->de_name[0] != '.')
				    && (de->de_name[1] != '.'))
					goto not_empty;

				if (de->de_name_len > 2)
					goto not_empty;

				if (de->de_name_len < 2) {
					if (de->de_inode != inode->i_ino)
						goto not_empty;
				}
			}

			if (de->de_name_len)
				p_addr =
				    p_addr +
				    RKFS_DIR_ENTRY_LEN(de->de_name_len);
			else
				break;
		}

		rkfs_put_page(page);
	}

	return 1;

 not_empty:

	rkfs_put_page(page);
	return 0;
}

struct file_operations rkfs_dir_operations = {
 read:	generic_read_dir,
 readdir:rkfs_readdir,
 fsync:rkfs_sync_file,
};
