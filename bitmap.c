/*
*
* bitmap.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003
* All rights reserved.
*
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/rkfs.h>

unsigned short rkfs_count_free(void *map, unsigned short offset,
			       unsigned short total_blocks)
{
	unsigned short sum = 0, i = 0, blkno = 0;

	if (!map) {
		rkfs_bug("NULL map specified\n");
		FAILED;
		goto out;
	}

	for (i = 0; i < RKFS_MIN_BLOCKS; i++) {
		blkno = offset + i;
		if (blkno > (total_blocks - 1))
			return sum;
		if (!rkfs_test_bit(i, map))
			sum += 1;
	}

 out:
	return sum;
}
