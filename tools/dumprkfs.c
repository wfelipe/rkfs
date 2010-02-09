/*
*
* dumprkfs.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003
* All rights reserved.
*
*/

#include "globals.h"
#include "rkfs.h"
#include "utils.h"

boolean quiet = FALSE;
boolean verbose = TRUE;

void print_usage(const char *prg_name)
{
	fprintf(stderr, "%s <device name>", prg_name);
}

int print_superblock(void *buf)
{
	struct rkfs_super_block *sb;
	register int i = 0;

	sb = (struct rkfs_super_block *)buf;

	if (sb->s_fsid != RKFS_ID) {
		print_emsg("\nNot a valid %s filesystem found.", RKFS_NAME);
		return -1;
	}

	print_msg("\nFilesystem ID: %d", sb->s_fsid);
	print_msg("\nFilesystem Ver: %d", sb->s_fsver);

	print_msg("\nUsed blocks:\n");
	for (i = 0; i < (RKFS_BLOCK_MAP_SIZE * sizeof(__u16) * 8); i++) {
		if (test_bit(i, sb->s_block_map)) {
			print_msg("%d ", i);
			if (i && !(i % 16))
				print_msg("\n");
		}
	}

	print_msg("\nUsed inodes:\n");
	for (i = 0; i < (RKFS_INODE_MAP_SIZE * sizeof(__u16) * 8); i++) {
		if (test_bit(i, sb->s_inode_map)) {
			print_msg("%d ", i);
			if (i && !(i % 16))
				print_msg("\n");
		}
	}

	print_msg("\nInode table map:");
	for (i = 0; i < RKFS_INODE_TABLES_MAP_SIZE; i++) {
		if (sb->s_itable_map[i][0] != 0)
			print_msg("\n\tIndex: %d, Block: %d, Count: %d", i,
				  sb->s_itable_map[i][0],
				  sb->s_itable_map[i][1]);
	}

	if (sb->s_state == RKFS_VALID_FS)
		print_msg("\nSuper block state: Valid");
	else {
		if (sb->s_state == RKFS_ERROR_FS)
			print_msg("\nSuper block state: Error");
		else
			print_msg("\nSuper block state: Unknown");
	}

	print_msg("\nTotal blocks: %d", sb->s_total_blocks);

	return 0;
}

int dumprkfs(const char *device)
{
	char buf[RKFS_BLOCK_SIZE];
	register int rc = 0, fd = 0;
	uint offset = 0, blkno = 0;
	ulong total_blocks = 0;

	disable_stream_buffering(stdout);
	disable_stream_buffering(stderr);

	if ((total_blocks = get_total_device_blocks(device)) == -1)
		return -1;
	print_msg("Device %s has %u blocks.", device, total_blocks);

	if ((fd = open_device(device, O_RDONLY)) < 0)
		return -1;

	offset = 0;
	do {
		memset(buf, 0, RKFS_BLOCK_SIZE);
		if (!offset)
			blkno = RKFS_SUPER_BLOCK;
		else
			blkno = offset;
		if (read_block(fd, blkno, &buf) != 0) {
			rc = -1;
			goto error_exit;
		}

		print_msg("\n\n***Superblock at offset: %d\n", offset);
		if (print_superblock(buf) != 0)
			break;

		offset += RKFS_MIN_BLOCKS;
		if (offset < (total_blocks - 1))
			continue;

		break;

	} while (1);

 error_exit:
	if (close_device(fd) != 0)
		rc = -1;

	if (rc)
		return -1;

	return 0;
}

void cleanup()
{
	printf("\n");
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	atexit(cleanup);

	if (argv[1] == NULL) {
		print_usage(argv[0]);
		exit(1);
	}

	if (dumprkfs(argv[1]) != 0)
		exit(1);

	exit(0);
}
