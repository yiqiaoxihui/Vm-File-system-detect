#ifndef BASE_H_INCLUDED
#define BASE_H_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>             //add the -lpthread

//#include <guestfs.h>            //-lguestfs
#include "ext2_qcow2/inode.h"
#include "sql/sqlread.h"
//#include <mcheck.h>
#include <mysql/mysql.h>        //-lmysqlclient


#define MAX_OVERLAY_IMAGES 20



#endif // BASE_H_INCLUDED
