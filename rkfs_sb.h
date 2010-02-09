/*
*
* rkfs_sb.h
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2002, 2003
* All rights reserved.
*
*/

#ifndef __RKFS_SB_H__
#define __RKFS_SB_H__

struct rkfs_sb_info {
    unsigned short s_sb_count;
    struct buffer_head **s_sbh;
};

#endif
