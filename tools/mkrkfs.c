/*
*
* mkrkfs.c
*
* R.K.Raja
* (rajkanna_hcl@yahoo.com, rajark_hcl@yahoo.co.in)
*
* (C) Copyright 2003, 2003.
* All rights reserved.
*
*/

#include "globals.h"
#include "rkfs.h"
#include "utils.h"
#include "mkrkfs.h"

/*
* Function to print banner.
*/
void print_version()
{
    fprintf(stdout,"%s (%d.%d), for intel based linux, %s\n",
            RKFS_NAME,RKFS_MAJOR_VER,RKFS_MINOR_VER,RKFS_DOB);
}


/*
* Function to print program usage.
*/
void print_usage(const char *prg_name)
{
    fprintf(stderr,"Usage: %s <options> <device>",prg_name);
    fprintf(stderr,"\nOptions:");
    fprintf(stderr,"\n'-v'   - Verbose");
    fprintf(stderr,"\n'-q'   - Quiet");
    fprintf(stderr,"\n'-s'   - Skip badblocks");
    fprintf(stderr,"\n'-V'   - Version\n");

    exit(1);
}


/*
* Argument parser...
*/
char *parse_args(int argc, char *argv[])
{
    register int c = 0;
    const char *options = "vqsV";
    extern int optind, opterr;
    static char device[255];

    opterr = 0;
    while( (c=getopt(argc,argv,options)) != -1 ) {
        switch(c) {
        case 'v':
            if( quiet ) {
                fprintf(stderr,"Options -v and -q are mutually exclusive.\n");
                print_usage(argv[0]);
            }
            verbose = TRUE;
            break;
        case 'q':
            if( verbose ) {
                fprintf(stderr,"Options -q and -v are mutually exclusive.\n");
                print_usage(argv[0]);
            }
            quiet = TRUE;
            break;
        case 's':
            skip_badblocks = TRUE;
            break;
        case 'V':
            if( quiet ) {
                fprintf(stderr,"Options -q and -V are mutually exclusive.\n");
                print_usage(argv[0]);
            }
            version = TRUE;
            break;
        default:
            fprintf(stderr,"'%c' is not a valid option.\n",optopt);
            print_usage(argv[0]);
        }
    }

    if( version ) {
        if( quiet ) {
            fprintf(stderr,"Options -q and -V are mutually exclusive.\n");
            print_usage(argv[0]);
        }
        if( argv[optind] != NULL )
            print_usage(argv[0]);
        print_version();
        exit(0);
    }

    if( optind == argc )
        print_usage(argv[0]);

    if( argv[optind] == NULL )
        print_usage(argv[0]);

    memset(device,0,(sizeof(device)/sizeof(device[0])));
    strcpy(device,argv[optind]);
    return device;
}

/*
* Function to create new directory block template.
*/
char *create_new_dir_block_template(ushort pinode, ushort cinode)
{
    static char buf[RKFS_BLOCK_SIZE];
    struct rkfs_dir_entry *dir_entry;

    memset(buf,0,RKFS_BLOCK_SIZE);

    dir_entry = (struct rkfs_dir_entry *) buf;
    dir_entry->de_inode = cinode;
    dir_entry->de_name_len = 1;
    strcat(dir_entry->de_name,".");

    dir_entry = (struct rkfs_dir_entry *) (buf + RKFS_DIR_ENTRY_LEN(1));
    dir_entry->de_inode = pinode;
    dir_entry->de_name_len = 2;
    strcat(dir_entry->de_name,"..");

    return buf;
}

/*
* Function to write root inode
*/
int write_inode(register int fd, struct rkfs_super_block *sb,
                ushort ino, struct rkfs_inode *inode)
{
    const char *this = "write_inode:";
    char blk_buf[RKFS_BLOCK_SIZE];
    ushort blkno = 0, offset = 0, itable_index = 0;
    char *ptr = NULL;

    itable_index = ino / RKFS_INODES_PER_BLOCK;
    offset = ino % RKFS_INODES_PER_BLOCK;
    blkno = sb->s_itable_map[itable_index][0];

    if( !blkno ) {
        print_emsg("\n%s No valid inode block found for inode %d.",this,ino);
        return -1;
    }

    memset(blk_buf,0,RKFS_BLOCK_SIZE);
    if( read_block(fd,blkno,&blk_buf) < 0 ) {
        print_emsg("\n%s Error while reading the block %d.",this,blkno);
        return -1;
    }

    ptr = (char *) (blk_buf + (offset * RKFS_INODE_SIZE));
    memcpy((struct rkfs_inode *)ptr,inode,RKFS_INODE_SIZE);

    if( write_block(fd,blkno,&blk_buf) < 0 ) {
        print_emsg("\n%s Error while writing the block %d.",this,blkno);
        return -1;
    }

    return 0;
}

/*
* Function to create '/' directory
*/
int create_root_directory(register int fd, struct rkfs_super_block *sb)
{
    const char *this = "create_root_directory:";
    struct rkfs_inode pinode;
    char *blk_ptr = NULL;

    blk_ptr = create_new_dir_block_template(RKFS_ROOT_INO,RKFS_ROOT_INO);

    memset(&pinode,0,RKFS_INODE_SIZE);
    pinode.i_uid = getuid();
    pinode.i_gid = getgid();
    pinode.i_mode = S_IFDIR | 0755;
    pinode.i_time = time(NULL);
    pinode.i_links_count = 2;
    pinode.i_size = RKFS_BLOCK_SIZE;
    pinode.i_blocks = RKFS_BLOCK_SIZE / 512;
    pinode.i_block[0] = RKFS_ROOT_DIR_BLOCK;

    if( write_block(fd,RKFS_ROOT_DIR_BLOCK,blk_ptr) < 0 ) {
        print_emsg("\n%s Error while writing the block %d.",this,
                   RKFS_ROOT_DIR_BLOCK);
        return -1;
    }

    if( write_inode(fd,sb,RKFS_ROOT_INO,&pinode) < 0 ) {
        print_emsg("\n%s Error while writing the inode %d.",this,
                   RKFS_ROOT_INO);
        return -1;
    }

    set_bit(RKFS_ROOT_DIR_BLOCK,sb->s_block_map);

    return 0;
}


/*
* Function to mark the bad blocks as used.
*/
int mark_badblocks(register int fd, struct rkfs_super_block *sb,
                   ushort total_blocks, uint offset, ushort *total_bad_blocks)
{
    char blk_buf[RKFS_BLOCK_SIZE];
    register int i = 0;
    ushort blkno = 0;

    if( !skip_badblocks )
        return 0;

    print_msg("\n");
    memset(blk_buf,0,RKFS_BLOCK_SIZE);
    for( i=0; i<RKFS_MIN_BLOCKS; i++ ) {
        blkno = offset + i;
        if( blkno > (total_blocks - 1) )
            break;
        if( read_block(fd,blkno,blk_buf) < 0 )  {
            print_msg("\n*** Block %d is bad. Marking it as used...\n",blkno);
            set_bit(i,sb->s_block_map);
            *total_bad_blocks += 1;
        }
    }

    return 0;
}


/*
* Core function that actually creates the filesystem.
*/
int create_rkfs(const char *device, ushort total_blocks)
{
    struct rkfs_super_block sb;
    register int rc = 0, fd = 0;
    ushort blkno = 0, j = 0, offset = 0, total_bad_blocks = 0;
    char blk_buf[RKFS_BLOCK_SIZE];

    print_msg("Creating %s filesystem on the device %s",RKFS_NAME,device);
    if( skip_badblocks )
        print_msg("\nThis may take few minutes, please wait...");
    print_msg("\nDevice %s has %d blocks.",device,total_blocks);

    if( (fd=open_device(device,O_RDWR)) < 0 ) {
        print_emsg("\nError while opening the device %s.",device);
        return -1;
    }

    offset = 0;
    do {
        memset(&sb,0,sizeof(struct rkfs_super_block));
        sb.s_fsid = RKFS_ID;
        sb.s_fsver = RKFS_VER;
        sb.s_state = RKFS_VALID_FS;
        sb.s_total_blocks = total_blocks;
        if( !offset ) {
            sb.s_itable_map[0][0] = RKFS_FIRST_INODE_TABLE_BLOCK;
            sb.s_itable_map[0][1] = 4;
        } else
            sb.s_itable_map[0][1] = 0;

        if( mark_badblocks(fd,&sb,total_blocks,offset,&total_bad_blocks) < 0 ) {
            rc = -1;
            goto error_exit;
        }

        if( total_bad_blocks > ((total_blocks * 75)/100) ) {
            print_emsg("\nDevice %s has too many bad blocks (%d).",device,
                       total_bad_blocks);
            rc = -1;
            goto error_exit;
        }

        if( !offset ) {
            for( j=0; j<RKFS_ROOT_DIR_BLOCK; j++ ) {
                if( test_bit(j,&sb.s_block_map) ) {
                    print_emsg("\nBlock %d is marked as bad.",j);
                    rc = -1;
                    goto error_exit;
                }
                set_bit(j,&sb.s_block_map);
            }

            for( j=0; j<RKFS_FIRST_INODE; j++ )
                set_bit(j,&sb.s_inode_map);
        } else {
            for( j=0; j<1; j++ ) {
                if( test_bit(j,sb.s_block_map) ) {
                    print_emsg("\nBlock %d is marked as bad.",(offset + j));
                    rc = -1;
                    goto error_exit;
                }
                set_bit(j,sb.s_block_map);
            }

            for( j=0; j<1; j++ )
                set_bit(j,&sb.s_inode_map);
        }

        if( !offset ) {
            if( create_root_directory(fd,&sb) < 0 ) {
                print_emsg("\nError while creating the root directory.");
                rc = -1;
                goto error_exit;
            }
        }

        if( !offset )
            blkno = RKFS_SUPER_BLOCK;
        else
            blkno = offset;

        memset(blk_buf,0,RKFS_BLOCK_SIZE);
        memcpy(blk_buf,&sb,sizeof(struct rkfs_super_block));
        if( write_block(fd,blkno,blk_buf) != 0 ) {
            print_emsg("\nError while writing the superblock.");
            rc = -1;
            goto error_exit;
        }

        offset += RKFS_MIN_BLOCKS;

        if( offset > (total_blocks - 1) )
            break;

        if( (total_blocks - offset) < RKFS_MIN_BLOCKS_PER_GROUP )
            break;
    } while(1);

error_exit:
    if( close_device(fd) != 0 )
        rc = -1;

    if( rc )
        return -1;

    print_msg("\nFilesystem created.\n");

    return 0;
}


int main(int argc, char *argv[])
{
    char *device = NULL;
    ulong tb = 0;

    disable_stream_buffering(stdout);
    disable_stream_buffering(stderr);

    device = parse_args(argc,argv);

    if( verbose )
        print_version();

    if( !is_valid_block_device(device) ) {
        print_emsg("\nDevice %s does'nt exist or not a valid block device.",
                   device);
        goto error_exit;
    }

    if( is_device_mounted(device) ) {
        print_emsg("\nDevice %s is mounted.",device);
        goto error_exit;
    }

    if( (tb=get_total_device_blocks(device)) == 0 ) {
        print_emsg("\nFailed to determine total blocks in the device %s.",
                   device);
        goto error_exit;
    }

    if( tb < RKFS_MIN_BLOCKS ) {
        print_emsg("\nDevice %s has too few blocks (%u).",device,tb);
        goto error_exit;
    }

    if( tb > RKFS_MAX_BLOCKS ) {
        print_emsg("\nDevice %s has too many blocks (%u).",device,tb);
        goto error_exit;
    }

    if( create_rkfs(device,(ushort)tb) != 0 )
        goto error_exit;

    exit(0);

error_exit:
    print_emsg("\nCannot create filesystem on this device.\n");
    exit(1);
}
