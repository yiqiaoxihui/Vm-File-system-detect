#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <guestfs.h>
//#include "include/ext2.h"
#include "ext2_qcow2/inode.h"
//#include <mcheck.h>

int main()
{
    //mtrace();
    guestfs_h *g=guestfs_create();

    MYSQL *my_conn;

    MYSQL_RES *res;

    MYSQL_ROW row;

    my_conn=mysql_init(NULL);

    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)) //连接detect数据库
    {
        printf("Connect Error!n");
        exit(1);
    }
    if(mysql_query(my_conn,"select baseImages.name as bn,overlays.name as oln from baseImages join overlays where overlays.id=1")) //连接baseImages表

    {
        printf("Query Error!n");
        exit(1);
    }

    //test();

    res=mysql_use_result(my_conn); //取得表中的数据并存储到res中
    char *bn,*oln;
    row=mysql_fetch_row(res);//打印结果
    //printf("%s %s\n",row[0],row[1]);
    bn=row[0];oln=row[1];
    int i;
    printf("\n base image:%s,layer:%s",bn,oln);
//    guestfs_add_drive(g,image);
//    guestfs_launch(g);
//    guestfs_mount(g,"/dev/sda1","/");
//    char **filename=guestfs_ls(g,"/");
//    for(i=0; i<sizeof(filename); i++)
//        printf("\nfilename:%s,number:%d",*(filename+i),i);
//    struct guestfs_statns *gs1=guestfs_statns(g,"/home/base/a.txt");
//    if(gs1!=NULL){
//            printf("\ninode:%ld,blocks:%ld,blksize%ld,size%ld",gs1->st_ino,gs1->st_blocks,gs1->st_blksize,gs1->st_size);
//    }
//
//    guestfs_umount(g,"/");
//    guestfs_shutdown(g);
//    guestfs_close(g);
    which_images_by_inode(bn,oln,133415);//133415
    mysql_free_result(res); //关闭结果集
    mysql_close(my_conn); //关闭与数据库的连接

    //muntrace();
    return 0;
}
