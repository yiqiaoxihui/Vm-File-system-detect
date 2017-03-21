#include "base.h"

int main()
{
    MYSQL *my_conn;

    MYSQL_RES *res;

    MYSQL_ROW row;
    //mtrace();
    char **image_abspath;
    char **image_id;
    int i;
    int thread_create;
    /******指针空间必须在主函数中分配*******/
    image_abspath=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
    image_id=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
    guestfs_h *g=guestfs_create();

    my_conn=mysql_init(NULL);

    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)) //连接detect数据库
    {
        printf("Connect Error!n");
        exit(1);
    }

    /*********************************read the image path***************************************/
    if(read_host_image_name(image_abspath,image_id)<=0){
        printf("\n read image abspath failed!");
        return 0;
    }

    //printf("\n in main the image path:%s",image_abspath[0]);
    for(i=0;image_abspath[i];i++){
        //printf("\n in main the image path:%s",image_abspath[i]);
    }
    pthread_t read_image_thread[i];
    struct ThreadVar threadVar[i];
    for(i=0;image_id[i];i++){
        printf("\n in main the image path:%s,id:%s",image_abspath[i],image_id[i]);
        threadVar[i].image_id=image_id[i];
        threadVar[i].con=my_conn;
        thread_create=pthread_create(&read_image_thread,NULL,multi_read_image_file,(void *)&threadVar[i]);
        if(thread_create){
            printf(stderr,"Error - pthread_create() return code: %d\n",thread_create);
            return 0;
        }
    }

    if(mysql_query(my_conn,"select baseImages.absPath as bn,overlays.absPath as oln from baseImages join overlays where overlays.id=1")) //连接baseImages表

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
    which_images_by_inode(bn,oln,140863);//133415
    mysql_free_result(res); //关闭结果集
    mysql_close(my_conn); //关闭与数据库的连接

    //muntrace();
    return 0;
}

