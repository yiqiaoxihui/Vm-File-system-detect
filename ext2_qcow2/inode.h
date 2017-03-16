#ifndef __INODE_H_INCLUDED
#define __INODE_H_INCLUDED

#include "../include/ext2.h"
#include "../include/qcow2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define Kilo 1024//1K bytes
#define Meg 1048576//1M bytes

int which_images_by_inode(char *baseImage,char *qcow2Image,unsigned int inode);

int test(char *baseImage,char *qcow2Image,unsigned int inode);

int blockInOverlay(char *baseImage,char *qcow2Image,unsigned int block,__U16_TYPE block_size);

//int inodeInOverlay(char *baseImage,char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits,unsigned int bytes_offset_into_block,struct ext2_inode *inode);

#endif // __INODE_H_INCLUDED
