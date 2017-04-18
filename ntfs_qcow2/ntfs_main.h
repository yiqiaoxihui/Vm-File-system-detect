#ifndef NTFS_MAIN_H_INCLUDED
#define NTFS_MAIN_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <guestfs.h>
#include "../include/ntfs.h"
#include "../include/qcow2.h"
#define NTFS_OFFSET 32256      //0x7e00

#define IS_MAGIC(a,b)		(*(int*)(a)==*(int*)(b))

int ntfs_update_file_metadata(char *overlay_image_path,char base_image_path[],__U64_TYPE inodes,int inode_count,char *overlay_id);

int ntfs_init_volume(ntfs_volume *vol,char *boot);

int ntfs_inode_in_overlay(char *overlay_image_path,struct ntfs_inode_info *ntfs_inode);

int ntfs_check_mft_record(ntfs_volume *vol, char *record, char *magic, int size);

static void ntfs_load_attributes(struct ntfs_inode_info* ino);

static int ntfs_process_runs(struct ntfs_inode_info *ino,ntfs_attribute* attr,unsigned char *data);

int ntfs_decompress_run(unsigned char **data, int *length, __U32_TYPE *cluster,int *ctype);

void ntfs_insert_run(ntfs_attribute *attr,int cnum,__U32_TYPE cluster,int len);

ntfs_attribute* ntfs_find_attr(struct ntfs_inode_info *ino,int type,char *name);

int ntfs_ua_strncmp(short int* a,char* b,int n);
#endif // NTFS_MAIN_H_INCLUDED
