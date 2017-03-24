#ifndef __INODE_H_INCLUDED
#define __INODE_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <guestfs.h>
#include <pthread.h>

#include "../include/public.h"
#include "../include/ext2.h"
#include "../include/qcow2.h"

#define Kilo 1024//1K bytes
#define Meg 1048576//1M bytes

int which_images_by_inode(char *baseImage,char *qcow2Image,unsigned int inode);

int test(char *baseImage,char *qcow2Image,unsigned int inode);

int blockInOverlay(char *baseImage,char *qcow2Image,unsigned int block,__U16_TYPE block_size);

void *multi_read_image_file(void *path);

int inodeInOverlay(char *baseImage,char *qcow2Image,unsigned int block_offset,unsigned int bytes_offset_into_block,__U16_TYPE block_bits,struct ext2_inode *inode);

int is_base_image_identical(char *overlay_image_id,char **base_image_path);

int update_file_metadata(char *overlay_image_path,char *base_image_path,int inodes[],int count);
#endif // __INODE_H_INCLUDED
