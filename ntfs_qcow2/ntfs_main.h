#ifndef NTFS_MAIN_H_INCLUDED
#define NTFS_MAIN_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <guestfs.h>
#include "../include/ntfs.h"

int ntfs_update_file_metadata(char *overlay_image_path,char base_image_path[],__U64_TYPE inodes[],int inode_count,char *overlay_id);

int ntfs_init_volume(ntfs_volume *vol,char *boot);
#endif // NTFS_MAIN_H_INCLUDED
