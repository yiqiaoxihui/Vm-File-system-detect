#ifndef __INODE_H_INCLUDED
#define __INODE_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <mysql/mysql.h>
#include <guestfs.h>

#include <pthread.h>

#include "../include/public.h"
#include "../include/ext2.h"
#include "../include/qcow2.h"
#include "../sql/sqlread.h"

#define Kilo 1024//1K bytes
#define Meg 1048576//1M bytes

#define EXT2_FILESYSTEM_OFFSET 1048576

#define EXT2_SUPERBLOCK_OFFSET 1049600

#define _ext2_group_desc_offset(block_size) (EXT2_FILESYSTEM_OFFSET+block_size)

int which_images_by_inode(char *baseImage,char *qcow2Image,unsigned int inode,char *filepath);

int test(char *baseImage,char *qcow2Image,unsigned int inode);

int ext2_blockInOverlay(char *qcow2Image,unsigned int block,__U16_TYPE block_size);

//void *multi_read_image_file(void *path);

int inodeInOverlay(char *qcow2Image,unsigned int block_offset,unsigned int bytes_offset_into_block,__U16_TYPE block_bits,struct ext2_inode *inode);

int ext2_inodes_in_overlay(char *baseImage,char *qcow2Image,__U32_TYPE *block_offset,__U32_TYPE *bytes_offset_into_block,__U16_TYPE block_bits,struct ext2_inode *inode,int inode_count);

int is_base_image_identical(char *overlay_image_id,char base_image_path[]);

int ext2_update_file_metadata(char *overlay_image_path,char base_image_path[],__U64_TYPE inodes[],int inode_count,char *overlay_id);
/**
 *author:liuyang
 *date  :2017/5/9
 *detail:cal overlay vm file md5
 *return void
 */
int ext2_overlay_md5(char *baseImage,char *overlay,char *overlay_id);

//void statistics_proportion();
//
//void allfile_md5();
#endif // __INODE_H_INCLUDED
