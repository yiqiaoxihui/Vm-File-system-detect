#ifndef CSQL_H_INCLUDED
#define CSQL_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

int read_host_image_name(char **image_abspath,char **image_id);


int sql_update_file_metadata(char *overlay_id,__U64_TYPE inode_number,int inodePosition,int dataPosition);

int sql_get_filesystem_type(char *overlayid,char **type);

int sql_get_backup_root(char **backupRoot);

int sql_file_restore_result(char *file_id,int restoreType,int result);
/**
 *author:liuyang
 *date  :2017/5/18
 *detail:read all overlay which need be scan
 *return int
 */
int sql_read_scan_overlay_name(char **image_abspath,char **image_id);

/**
 *author:liuyang
 *date  :2017/5/18
 *detail:update all file scan info
 *return int
 */
int sql_update_scan_info(char *overlayid,__U32_TYPE all_files,__U32_TYPE overlay_files,__U32_TYPE scan_time);
#endif // CSQL_H_INCLUDED
