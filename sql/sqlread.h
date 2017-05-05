#ifndef CSQL_H_INCLUDED
#define CSQL_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>

int read_host_image_name(char **image_abspath,char **image_id);


int sql_update_file_metadata(char *overlay_id,__U64_TYPE inode_number,int inodePosition,int dataPosition);

int get_filesystem_type(char *overlayid,char **type);

int sql_get_backup_root(char **backupRoot);

int sql_file_restore_success(char *file_id,int restoreType);
#endif // CSQL_H_INCLUDED
