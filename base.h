#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>             //add the -lpthread

//#include <guestfs.h>            //-lguestfs
#include "ext2_qcow2/ext2_main.h"
#include "sql/sqlread.h"
//#include <mcheck.h>
#include <mysql/mysql.h>        //-lmysqlclient
#include "ntfs_qcow2/ntfs_main.h"

#define MAX_OVERLAY_IMAGES 20

void *multi_read_image_file(void *path);

void allfile_md5();

void statistics_proportion();

void system_test();
#endif // BASE_H_INCLUDED
