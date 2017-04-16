#include "base.h"
#include "include/public.h"

int main()
{
    //mtrace();
    /**关键文件检测*/
    //key_files_detect();

    {   /**for test ntfs*/
        unsigned long int inodes[2]={5625,10720};
        ntfs_update_file_metadata("/var/lib/libvirt/images/winxp_snap1.img","/var/lib/libvirt/images/winxp.img",inodes,2,11);
    }

    //allfile_md5();
    //statistics_proportion();
    return 0;
}
int key_files_detect(){
    char **image_abspath;
    char **image_id;
    int images_count=0;
    int i;
    int thread_create;
    /******指针空间必须在主函数中分配*******/
    image_abspath=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
    image_id=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
    /*********************************read the image path***************************************/
    if(read_host_image_name(image_abspath,image_id)<=0){
        printf("\n read image abspath failed or no use images!");
        return 0;
    }
    for(i=0;image_abspath[i];i++){
            printf("\n in main read image:%s",image_abspath[i]);
            images_count++;
    }
    printf("\ i:%d,image count:%d",i,images_count);
    read_image_thread=malloc(images_count*sizeof(pthread_t));
    threadVar=malloc(images_count*sizeof(struct ThreadVar));
    for(i=0;i<images_count;i++){
        //threadVar[434].image_id=image_abspath[i];
        printf("\noverlay:%s,id:%s",image_abspath[i],image_id[i]);
        threadVar[i].image_id=image_id[i];
        threadVar[i].image_path=image_abspath[i];
        thread_create=pthread_create(&read_image_thread[i],NULL,multi_read_image_file,(void *)&threadVar[i]);
        if(thread_create){
            printf(stderr,"\nError -%d pthread_create() return code: %d",i,thread_create);
            return 0;
        }
    }
    for(i=0;i<images_count;i++){
        pthread_join(read_image_thread[i],NULL);
    }

    free(image_abspath);
    free(image_id);
    free(threadVar);
    free(read_image_thread);
    return 1;
}
/*
 *author:liuyang
 *date  :2017/3/21
 *detail:多线程读取多个镜像，更新监控文件信息
 *return 1
 */
void *multi_read_image_file(void *var){
    struct ThreadVar *threadVar=(struct ThreadVar *)var;
    char *overlay_image_id;
    char *overlay_image_path;
    char base_image_path[256];
    char strsql[256];/*take care!!!*/
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    guestfs_h *g=guestfs_create();
    __U64_TYPE  *inodes;
    struct guestfs_statns *gs1;
    int count;
    int i;
    char *md5str;
    overlay_image_id=threadVar->image_id;
    overlay_image_path=threadVar->image_path;
    printf("\n\n\n\n\n\n\n\n\n\nbegin multi thread.........");
    /******************************************判断原始镜像和增量镜像中的原始镜像名是否一致************************************************/

    if(is_base_image_identical(overlay_image_id,base_image_path)<=0){
        printf("\n<<<<<<<<<in multi:overlay:%s,images are not identical,exit thread!>>>>>>>>>>\n\n\n\n\n\n\n\n\n",overlay_image_id);
        pthread_exit(0);
    }
    printf("\nidentical!overlay_image_id:%s,overlay_image_path:%s,base image path:%s",overlay_image_id,overlay_image_path,base_image_path);
    /******************************************判断原始镜像和增量镜像中的原始镜像名是否一致***end*********************************************/
/*    {//
        int all=0;
        unsigned long int allsize=0;
        blockInOverlay_error=0;
        inodeInOverlay_error=0;
        magic_error=0;
        int error_file_count1=0;
        overlay_file_count=0;
        inode_in_overlay_file_count=0;
        read_error=0;
        __U32_TYPE inode_number;
        FILE *fp,*fp1,*fp2;
        char line[256];
        struct guestfs_statns *gs1;
        char *md5str;
        fp=fopen("/home/heaven/Downloads/test.txt","r");
        //fp1=fopen("/home/heaven/Downloads/truepath.txt","w");

        //fp2=fopen("/home/heaven/Downloads/overlay_file.txt","w");
        if(fp==NULL){
            printf("\n open allpath fail!");
            exit(0);
        }
    //    fgets(line,256,fp);
    //    line[strlen(line)-1]='\0';
    //    printf("%s,%d",line,strlen(line));

        guestfs_h *g=guestfs_create();
        guestfs_add_drive(g,overlay_image_path);
        guestfs_launch(g);
        guestfs_mount_ro(g,"/dev/sda1","/");
        while(fgets(line,256,fp)){
            all++;
            line[strlen(line)-1]='\0';
            printf("\nthe file number:%d,file:%s",all,line);
            gs1=guestfs_lstatns(g,line);
            printf("\nblock:%d,blocksize:%d,size:%d",gs1->st_blocks,gs1->st_blksize,gs1->st_size);
            if(gs1==NULL){
                error_file_count1++;
                printf("\nthe file not exist:%.0f",error_file_count1);
                continue;
            }
            allsize+=gs1->st_size;
            inode_number=gs1->st_ino;
            which_images_by_inode("/var/lib/libvirt/images/base.img",overlay_image_path,inode_number,line);

        }
        for(i=0;i<overlay_file_count;i++){
            printf("\nto do md5 file:%s",overlay_filepath[i]);
            md5str=guestfs_checksum(g,"md5",overlay_filepath[i]);
            if(md5str==NULL){
                continue;
            }
            printf("\nmd5:%s|file in overlay:%s",md5str,overlay_filepath[i]);
            //fputs(overlay_filepath[i],fp2);
            //fputc('\n',fp2);
        }
        allsize=allsize/1024;
        printf("\n average file size:%d KB",allsize/all);
        printf("\nall file:%d;\n file in overlay:%d",all,overlay_file_count);
        printf("\nerror file:%.0f;\nread_error:%0.f;\next2_blockInOverlay error:%d;\ninodeInOverlay:%d;\nmagic_error:%d\n",error_file_count1,read_error,blockInOverlay_error,inodeInOverlay_error,magic_error);
        printf("\n增量文件占比:%.2f%%",(((float)overlay_file_count)/(all-error_file_count-read_error))*100);
        guestfs_free_statns(gs1);
        guestfs_umount (g, "/");
        guestfs_shutdown (g);
        guestfs_close (g);
        fclose(fp);
    }
    */

    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!");
        mysql_close(my_conn);
        pthread_exit(0);
    }
    /****************************************读取该增量镜像对应的监控文件**************************************************/
    sprintf(strsql,"select file.absPath,file.id,file.firstAddFlag from file join overlays where overlays.id=file.overlayId and file.status=1 and overlays.id=%s",
            overlay_image_id);

    if(mysql_query(my_conn,strsql)){
        printf("\nin multi: query the image file failed!");
        mysql_close(my_conn);
        pthread_exit(0);
    }
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    /**该镜像无徛控文件，无需加载*/
    if(count<=0){
        printf("\n<<<<<<<<<------in multi:no file in the image,thread exit!---->>>>>>>>>>");
        mysql_close(my_conn);
        pthread_exit(0);
    }
    /****************************************读取该增量镜像对应的监控文件***end***********************************************/

    inodes=malloc((count+1)*sizeof(__U64_TYPE));
    //printf("\n$$$$$$$$$$$$$$count:%d,sizeof(inodes):%d$$$$$$$$$$$$$$$$",count,sizeof(inodes));
    printf("\n****$$$$*****begin read image by libguestfs***$$$$*****");
    guestfs_add_drive(g,overlay_image_path);
    guestfs_launch(g);
    //gues
    guestfs_mount(g,"/dev/sda1","/");
    count=0;
    /****************************************使用guestfs读取文件inode元信息，更新文件状态**************************************************/
    while((row=mysql_fetch_row(res))){
        //printf("\nin multi:read from datebase the file name:%s",row[0]);
        //根据路径名获取inode等信息
        gs1=guestfs_statns(g,row[0]);
        if(gs1!=NULL){
            /**未新添加的文件计算hash值*/
            //printf("\nthe fisrAddFlag:%s",row[2]);
            md5str=NULL;
            if(strcmp(row[2],"0")==0){
                md5str=guestfs_checksum(g,"md5",row[0]);
                printf("\nhash is:%s,len:%d",md5str,strlen(md5str));
                if(md5str!=NULL){
                    printf("\nhash is not null:%s",md5str);
                    sprintf(strsql,"update file set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld),hash='%s',firstAddFlag=1 where file.id=%s",
                            gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,md5str,row[1]);

                }else{
                    sprintf(strsql,"update file set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld) where file.id=%s",
                            gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,row[1]);
                }
            }else{
                sprintf(strsql,"update file set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld) where file.id=%s",
                        gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,row[1]);
            }
            printf("\nstrsql:%s\nlen of strsql:%d",strsql,strlen(strsql));
            if(mysql_query(my_conn,strsql)){
                printf("\nupdate the meta data of file failed!!!");
            }
            inodes[count]=gs1->st_ino;
            printf("\nthe inodes:%ld,gs1_st_ino:%ld",inodes[count],gs1->st_ino);
            //TODO:restore file from base image
            count++;
            if(md5str!=NULL){
                free(md5str);
                md5str=NULL;
            }
        }else{/**该文件在镜像中不存在*/
            sprintf(strsql,"update file set status=0 where file.id=%s",row[1]);
            if(mysql_query(my_conn,strsql)){
                printf("\nupdate the status of file failed!!!");
            }
            printf("\nthe file don't exist in image!");
        }
        //guestfs_free_statns(gs1);
    }
    if(count==0){
        printf("\navaliable inode of file no! leaven multi thread.......\n\n\n\n\n\n");
        mysql_close(my_conn);
        free(inodes);
        pthread_exit(0);
    }
    inodes[count]=0;
    for(i=0;i<count;i++)printf("\n i:%d,inode:%ld",i,inodes[i]);
    ext2_update_file_metadata(overlay_image_path,base_image_path,inodes,count,overlay_image_id);
    free(inodes);
    //for(i=0;inodes[i];i++)printf("\n inode %d:%d",i,inodes[i]);
    /****************************************使用guestfs读取文件inode元信息，更新文件状态***end***********************************************/
    /****************************************读取数据库位于增量中的文件，计算哈希对比是否被篡改**************************************************/
    sprintf(strsql,"select file.absPath,file.hash,file.id from file join overlays where overlays.id=file.overlayId and file.dataPosition=2 and overlays.id=%s",overlay_image_id);
    if(mysql_query(my_conn,strsql)){
        printf("\nin multi: query the image overlay file failed!");
        goto check_md5_failed;
    }
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    /**该镜像无内容在增量中的文件*/
    if(count<=0){
        printf("\nin multi:no overlay file which data in overlay,thread exit!");
        goto normal;
    }
    while((row=mysql_fetch_row(res))){
        md5str=guestfs_checksum(g,"md5",row[0]);
        if(md5str==NULL){
            printf("\nguestfs cal md5 failed,maybe the file not exist!");
            continue;
        }
        printf("\nthe normal file md5:%s;\nthe file md5 now:%s",row[1],md5str);
        if(strcmp(md5str,row[1])==0){
            printf("\nthe hash consistency,file security!");
        }else{
            printf("\nthe hash not consistency,file is falsified!");
            sprintf(strsql,"update file set file.isModified=1 where file.id=%s",row[2]);
        }
    }
    /****************************************读取数据库位于增量中的文件，计算哈希对比是否被篡改***end***********************************************/
normal:
    guestfs_umount(g,"/");
    guestfs_shutdown(g);
    guestfs_close(g);
    mysql_close(my_conn);
    printf("\nleaven multi thread.......\n\n\n\n\n\n");
    pthread_exit(0);
check_md5_failed:
    guestfs_umount(g,"/");
    guestfs_shutdown(g);
    guestfs_close(g);
    mysql_close(my_conn);
    printf("\nleaven multi thread.......check md5 failed!\n\n\n\n\n\n");
}
/*
 *author:liuyang
 *date  :2017/3/28
 *detail:cal all vm file md5
 */
void allfile_md5(){
    FILE *fp;
    char line[256];
    struct guestfs_statns *gs1;
    char *md5str;
    unsigned long int md5num=0;
    fp=fopen("/home/heaven/Downloads/truepath.txt","r");
    //fp1=fopen("/home/heaven/Downloads/truepath.txt","w");
    /*only in overlay files*/
    //fp2=fopen("/home/heaven/Downloads/overlay_file.txt","w");
    if(fp==NULL){
        printf("\n open allpath fail!");
        exit(0);
    }
//    fgets(line,256,fp);
//    line[strlen(line)-1]='\0';
//    printf("%s,%d",line,strlen(line));
    guestfs_h *g=guestfs_create();
    guestfs_add_drive(g,"/var/lib/libvirt/images/snap1.img");
    guestfs_launch(g);
    guestfs_mount_ro(g,"/dev/sda1","/");
    while(!feof(fp)){
        all_file_count++;
        fgets(line,256,fp);
        line[strlen(line)-1]='\0';
        printf("\nfile path:%s",line);
        gs1=guestfs_lstatns(g,line);
        if(gs1==NULL){
            continue;
        }
        if(S_ISREG(gs1->st_mode)!=1){
            continue;
        }
        printf("\nfile type:%x",gs1->st_mode);
        md5str=guestfs_checksum(g,"md5",line);
        if(md5str==NULL){
            continue;
        }
        md5num++;
        printf("\nmd5:%s",md5str);

        //break;
    }
    printf("\nall file number in VM:%.0f,the md5 file:%ld",all_file_count,md5num);
    guestfs_free_statns(gs1);
    guestfs_umount (g, "/");
    guestfs_shutdown (g);
    guestfs_close (g);
    fclose(fp);
}
/*
 *author:liuyang
 *date  :2017/3/28
 *detail:统计虚拟机全部文件和增量文件占比
 */
void statistics_proportion(){
    int i;
    blockInOverlay_error=0;
    inodeInOverlay_error=0;
    magic_error=0;

    all_file_count=0;
    error_file_count=0;
    overlay_file_count=0;
    inode_in_overlay_file_count=0;
    read_error=0;
    __U32_TYPE inode_number;
    FILE *fp,*fp1,*fp2;
    char line[256];
    struct guestfs_statns *gs1;
    char *md5str;
    fp=fopen("/home/heaven/Downloads/truepath.txt","r");
    //fp1=fopen("/home/heaven/Downloads/truepath.txt","w");
    /*only in overlay files*/
    //fp2=fopen("/home/heaven/Downloads/overlay_file.txt","w");
    if(fp==NULL){
        printf("\n open allpath fail!");
        exit(0);
    }
//    fgets(line,256,fp);
//    line[strlen(line)-1]='\0';
//    printf("%s,%d",line,strlen(line));
    guestfs_h *g=guestfs_create();
    guestfs_add_drive(g,"/var/lib/libvirt/images/snap1.img");
    guestfs_launch(g);
    guestfs_mount_ro(g,"/dev/sda1","/");
    while(fgets(line,256,fp)){
        all_file_count++;
        line[strlen(line)-1]='\0';
        gs1=guestfs_lstatns(g,line);
        if(gs1==NULL){
            printf("\nthe file not exist:%.0f",error_file_count);
            error_file_count++;
            continue;
        }
//        line[strlen(line)]='\n';
//        fputs(line,fp1);
        inode_number=gs1->st_ino;

        printf("\nall file number in VM:%.0f",all_file_count);

        which_images_by_inode("/var/lib/libvirt/images/base.img","/var/lib/libvirt/images/snap1.img",inode_number,line);
        guestfs_free_stat(gs1);
        //break;

    }
    for(i=0;i<overlay_file_count;i++){
        md5str=guestfs_checksum(g,"md5",overlay_filepath[i]);
        if(md5str==NULL){
            continue;
        }
        free(md5str);
        printf("\nmd5:%s|file in overlay:%s",md5str,overlay_filepath[i]);
        //fputs(overlay_filepath[i],fp2);
        //fputc('\n',fp2);
    }
    printf("\nall file:%.0f;\nfile in overlay:%d;\ninode in overlay:%0.f;\nerror file:%.0f;\n",all_file_count,overlay_file_count,inode_in_overlay_file_count,error_file_count);
    printf("\nread_error:%0.f;\next2_blockInOverlay error:%d;\ninodeInOverlay:%d;\nmagic_error:%d\n",read_error,blockInOverlay_error,inodeInOverlay_error,magic_error);
    printf("\n增量文件占比:%.5f\%",(((float)overlay_file_count)/(all_file_count-error_file_count-read_error))*100);
    guestfs_free_statns(gs1);
    guestfs_umount (g, "/");
    guestfs_shutdown (g);
    guestfs_close (g);
    fclose(fp);
    //fclose(fp2);
    //fclose(fp1);
    free(md5str);
}
