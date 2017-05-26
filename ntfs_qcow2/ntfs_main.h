#ifndef NTFS_MAIN_H_INCLUDED
#define NTFS_MAIN_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <guestfs.h>
#include <sys/stat.h>
#include "../include/ntfs.h"
#include "../include/qcow2.h"
#define NTFS_OFFSET 1048576      //0x7e00(32256),win10 1048576

#define IS_MAGIC(a,b)		(*(int*)(a)==*(int*)(b))
/*ALL BIT -1*/
#define BIT_1_POS(__value, __ret) { \
  unsigned long __n = __value; \
  unsigned __lg = 0; \
  if (__n > 0x80000000) { __lg += 32; __n >>= 32; } \
  if (__n > 0x8000) { __lg += 16; __n >>= 16; } \
  if (__n > 0x80) { __lg += 8; __n >>= 8; } \
  if (__n > 8) { __lg += 4; __n >>= 4; } \
  if (__n == 8) __lg += 4; \
  else if (__n == 4) __lg += 3; \
  else if (__n == 2) __lg += 2; \
  else if (__n == 1) __lg += 1; \
  __ret = __lg-1; \
}

int ntfs_update_file_metadata(char *overlay_image_path,char base_image_path[],__U64_TYPE inodes[],int inode_count,char *overlay_id);

int ntfs_init_volume(ntfs_volume *vol,char *boot);

int ntfs_inode_in_overlay(char *overlay_image_path,struct ntfs_inode_info *ntfs_inode);

int ntfs_check_mft_record(ntfs_volume *vol, char *record, char *magic, int size);

static void ntfs_load_attributes(struct ntfs_inode_info* ino);

static int ntfs_process_runs(struct ntfs_inode_info *ino,ntfs_attribute* attr,unsigned char *data);

int ntfs_decompress_run(unsigned char **data, int *length, __U32_TYPE *cluster,int *ctype);

void ntfs_insert_run(ntfs_attribute *attr,int cnum,__U32_TYPE cluster,int len);

ntfs_attribute* ntfs_find_attr(struct ntfs_inode_info *ino,int type,char *name);

int ntfs_ua_strncmp(short int* a,char* b,int n);

int ntfs_blockInOverlay(char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits);

/**
 *author:liuyang
 *date  :2017/5/10
 *detail:
 *return
 */
int ntfs_overlay_md5(char *baseImage,char *overlay,char *overlay_id);
#endif // NTFS_MAIN_H_INCLUDED
