#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>             //add the -lpthread
#include <mysql/mysql.h>        //-lmysqlclient
#include <guestfs.h>            //-lguestfs
//#include "include/ext2.h"
#include "ext2_qcow2/inode.h"
#include "sql/csql.h"
//#include <mcheck.h>

#define MAX_OVERLAY_IMAGES 10

/*多线程读取镜像文件所需参数*/
struct ThreadVar{
    char *image_id;
    MYSQL *con;
};

#endif // BASE_H_INCLUDED
