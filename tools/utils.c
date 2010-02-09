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

#include "globals.h"
#include "rkfs.h"
#include "utils.h"


void print_msg(const char *frmt, ...)
{
    va_list vptr;

    if( !verbose )
        return;

    va_start(vptr,frmt);
    vfprintf(stdout,frmt,vptr);
    fflush(stdout);
    va_end(vptr);
}


void print_emsg(const char *frmt, ...)
{
    va_list vptr;

    if( quiet )
        return;

    va_start(vptr,frmt);
    vfprintf(stderr,frmt,vptr);
    fflush(stderr);
    va_end(vptr);
}


void disable_stream_buffering(FILE *fptr)
{
    setbuf(fptr,NULL);
}


boolean is_valid_block_device(const char *device)
{
    const char *this = "is_valid_block_device:";
    struct stat sbuf;

    if( stat(device,&sbuf) != 0 ) {
        print_emsg("\n%s 'stat' error: %s.",this,strerror(errno));
        return FALSE;
    }

    if( !S_ISBLK(sbuf.st_mode) )
        return FALSE;

    return TRUE;
}


int exec_cmd(const char *frmt, ...)
{
    char cmd_buf[BIG_BUF_SIZE];
    va_list vptr;
    register int rc = 0;

    memset(cmd_buf,0,BIG_BUF_SIZE);
    va_start(vptr,frmt);
    vsprintf(cmd_buf,frmt,vptr);
    va_end(vptr);

    rc = system(cmd_buf);
    if( WIFEXITED(rc) )
        rc = WEXITSTATUS(rc);

    return rc;
}


boolean is_device_mounted(const char *device)
{
    const char *red = "> /dev/null";
    register int rc = 0;

    rc = exec_cmd("%s | %s %s %s %s",MOUNT_CMD,GREP_CMD,
                  "-w",device,red);

    if( rc == 0 )
        return TRUE;

    return FALSE;
}


ulong get_total_device_blocks(const char *device)
{
    const char *this = "get_total_device_blocks:";
    register int fd = 0;
    ulong size = 0, res = 0;

    if( (fd=open(device,O_RDONLY)) < 0 ) {
        print_emsg("\n%s 'open' error: %s.",this,strerror(errno));
        return 0;
    }

    if( ioctl(fd,BLKGETSIZE,&size) < 0 ) {
        close(fd);
        print_emsg("\n%s 'ioctl' error: %s.",this,strerror(errno));
        return 0;
    }

    close(fd);
    res = (ushort) (size / ( 1024 / 512 ));

    return res;
}


int open_device(const char *device, register int flags)
{
    const char *this = "open_device:";
    register int fd = 0;

    if( (fd=open(device,flags)) < 0 ) {
        print_emsg("\n%s 'open' error: %s.",this,strerror(errno));
        return -1;
    }

    return fd;
}


int close_device(register int fd)
{
    const char *this = "close_device:";

    if( close(fd) != 0 ) {
        print_emsg("\n%s 'close' error: %s.",this,strerror(errno));
        return -1;
    }

    return 0;
}


int set_bit(ushort bit_nr, void *addr)
{
    int mask, retval;
    unsigned char *ADDR = (unsigned char *) addr;

    ADDR += bit_nr >> 3;
    mask = 1 << (bit_nr & 0x07);
    retval = (mask & *ADDR) != 0;
    *ADDR |= mask;

    return retval;
}


int clear_bit(ushort bit_nr, void *addr)
{
    int mask, retval;
    unsigned char *ADDR = (unsigned char *) addr;

    ADDR += bit_nr >> 3;
    mask = 1 << (bit_nr & 0x07);
    retval = (mask & *ADDR) != 0;
    *ADDR &= ~mask;

    return retval;
}

int test_bit(ushort bit_nr, void *addr)
{
    int mask;
    const unsigned char *ADDR = (const unsigned char *) addr;

    ADDR += bit_nr >> 3;
    mask = 1 << (bit_nr & 0x07);

    return ((mask & *ADDR) != 0);
}

int write_block(register int fd, ushort blkno, const void *blk_buf)
{
    const char *this = "write_block:";
    off_t noffset = 0;
    ssize_t wrote = 0;

    if( lseek(fd,(off_t) 0,SEEK_SET) != 0 ) {
        print_emsg("\n%s 'lseek' error: %s.",this,strerror(errno));
        return -1;
    }

    noffset = blkno * RKFS_BLOCK_SIZE;
    if( lseek(fd,noffset,SEEK_SET) != noffset ) {
        print_emsg("\n%s 'lseek' error: %s.",this,strerror(errno));
        return -1;
    }

    wrote = write(fd,blk_buf,RKFS_BLOCK_SIZE);
    if( wrote != RKFS_BLOCK_SIZE ) {
        print_emsg("\n%s 'write' error: %s.",this,strerror(errno));
        return -1;
    }

    return 0;
}

int read_block(register int fd, ushort blkno, void *blk_buf)
{
    const char *this = "read_block:";
    off_t noffset = 0;
    ssize_t actual = 0;

    if( lseek(fd,(off_t) 0,SEEK_SET) != 0 ) {
        print_emsg("\n%s 'lseek' error: %s.",this,strerror(errno));
        return -1;
    }

    noffset = blkno * RKFS_BLOCK_SIZE;
    if( lseek(fd,noffset,SEEK_SET) != noffset ) {
        print_emsg("\n%s 'lseek' error: %s.",this,strerror(errno));
        return -1;
    }

    actual = read(fd,blk_buf,RKFS_BLOCK_SIZE);
    if( actual != RKFS_BLOCK_SIZE ) {
        print_emsg("\n%s 'read' error: %s.",this,strerror(errno));
        return -1;
    }

    return 0;
}

#ifdef __UTILS_TEST__
int main(int argc, char *argv[])
{
    return 0;
}

#endif
