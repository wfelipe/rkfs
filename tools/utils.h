/*
*
* utils.h
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003.
* All rights reserved.
*
*/

#ifndef _UTILS_H_
#define _UTILS_H_

extern boolean quiet;
extern boolean verbose;

void print_msg(const char *frmt, ...);
void print_emsg(const char *frmt, ...);
void disable_stream_buffering(FILE * fptr);
boolean is_valid_block_device(const char *device);
int exec_cmd(const char *frmt, ...);
boolean is_device_mounted(const char *device);
ulong get_total_device_blocks(const char *device);
int open_device(const char *device, int flags);
int close_device(register int fd);
int set_bit(ushort bit_nr, void *addr);
int clear_bit(ushort bit_nr, void *addr);
int test_bit(ushort bit_nr, void *addr);
int write_block(register int fd, ushort blkno, const void *blk_buf);
int read_block(register int fd, ushort blkno, void *blk_buf);

#endif
