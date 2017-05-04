#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>             //add the -lpthread

//#include <guestfs.h>            //-lguestfs

#include "sql/sqlread.h"
//#include <mcheck.h>
#include <mysql/mysql.h>        //-lmysqlclient

#include "ext2_qcow2/ext2_main.h"
#include "ntfs_qcow2/ntfs_main.h"

#define MAX_OVERLAY_IMAGES 20

void *multi_read_image_file(void *path);

int file_is_modified_by_md5(guestfs_h *g,MYSQL *my_conn,MYSQL_RES *res,MYSQL_ROW row,char strsql[],char *overlay_image_id);

void update_file_info(guestfs_h *g,struct guestfs_statns *gs1,MYSQL *my_conn,char strsql[],MYSQL_ROW row);

void allfile_md5();

void statistics_proportion();

void system_test();
#endif // BASE_H_INCLUDED
