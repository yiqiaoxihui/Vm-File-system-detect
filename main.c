#include "base.h"
#include "include/public.h"

int main()
{
    //mtrace();
    /**关键文件检测*/
    //key_files_detect();
    {   /**for test ntfs*/
        //unsigned long int inodes[2]={5625,10720};
        //ntfs_update_file_metadata("/var/lib/libvirt/images/winxp_snap1.img","/var/lib/libvirt/images/winxp.img",10720,1,11);
    }

    //which_images_by_inode("/var/lib/libvirt/images/base.img","/var/lib/libvirt/images/snap1.img",133301,"/home/base/Desktop/a.txt");
    ext2_overlay_md5("/var/lib/libvirt/images/base.img","/var/lib/libvirt/images/snap1.img");
    //allfile_md5();
    //statistics_proportion();
    return 0;
}
/**
 *author:liuyang
 *date  :2017/3/21
 *detail:拉取该节点增量镜像，开启多线程处理
 *return 1
 */
int key_files_detect(){
    char **base_abspath;
    char **image_abspath;
    char **image_id;
    int images_count;
    int i;
    int thread_create;
    base_abspath=malloc((MAX_BASE_IMAGES+1)*sizeof(char *));
    if(base_abspath==NULL){
        printf("\nmalloc base image path failed!");
    }
    if(sql_get_base_image_path(base_abspath,&images_count)<=0){
        printf("\nread base image for restore file failed!");
    }

    global_biga.baseImageGuestfs=malloc(images_count*sizeof(struct BaseImageGuestfs));
    global_biga.len=images_count;
    for(i=0;i<images_count;i++){
        //printf("\ni:%d,base image:%s",i,base_abspath[i]);
        global_biga.baseImageGuestfs[i].g=NULL;
        global_biga.baseImageGuestfs[i].base_image_name=base_abspath[i];
    }

    /******指针空间必须在主函数中分配??*******/
    image_abspath=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
    image_id=malloc((MAX_OVERLAY_IMAGES+1)*sizeof(char *));
    /*********************************read the image path***************************************/
    if(read_host_image_name(image_abspath,image_id)<=0){
        printf("\n read image abspath failed or no used images!");
        return 0;
    }
    images_count=0;
    for(i=0;image_abspath[i];i++){
            printf("\n back to  key_files_detect, read image:%s",image_abspath[i]);
            images_count++;
    }
    //printf("\nimage count:%d",images_count);
    read_image_thread=malloc(images_count*sizeof(pthread_t));
    threadVar=malloc(images_count*sizeof(struct ThreadVar));
    for(i=0;i<images_count;i++){
        //threadVar[434].image_id=image_abspath[i];
        //printf("\noverlay:%s,id:%s",image_abspath[i],image_id[i]);
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
    /**关闭原始镜像guestfs_h句柄*/
    for(i=0;i<global_biga.len;i++){
        //printf("\ni:%d,base image:%s",i,base_abspath[i]);
        if(global_biga.baseImageGuestfs[i].g!=NULL){
            printf("\n%s guestfs_h have been closed now!",global_biga.baseImageGuestfs[i].base_image_name);
            guestfs_umount(global_biga.baseImageGuestfs[i].g,"/");
            guestfs_shutdown(global_biga.baseImageGuestfs[i].g);
            guestfs_close(global_biga.baseImageGuestfs[i].g);
        }
    }
    free(base_abspath);
    free(image_abspath);
    free(image_id);
    free(threadVar);
    free(read_image_thread);
    return 1;
}
/**
 *author:liuyang
 *date  :2017/3/21
 *detail:多线程读取多个镜像，更新监控文件信息
 *return 1
 */
 //   /home/Desktop/a.txtff
void *multi_read_image_file(void *var){
    struct ThreadVar *threadVar=(struct ThreadVar *)var;
    char *overlay_image_id;
    char *overlay_image_path;
    char base_image_path[256];
    char strsql[512];/*take care!!!*/
    char *type;
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

    if(guestfs_add_drive(g,overlay_image_path)!=0){
        mysql_close(my_conn);
        pthread_exit(0);
    }
    if(is_base_image_identical(overlay_image_id,base_image_path)<=0){
        printf("\nnot identical!exit thread!\n\n\n\n\n\n\n\n\n",overlay_image_id);
        pthread_exit(0);
    }
    printf("\nidentical!\noverlay_image_id:%s,overlay_image_path:%s;\nbase image path:%s",overlay_image_id,overlay_image_path,base_image_path);

    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!");
        mysql_close(my_conn);
        pthread_exit(0);
    }
    /****************************************读取该增量镜像对应的监控文件**************************************************/
    sprintf(strsql,"select files.absPath,files.id,files.firstAddFlag from files join overlays where overlays.id=files.overlayId and files.status=1 and overlays.id=%s",
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
        printf("\nin multi:no file in the image,thread exit!\n\n\n\n\n\n\n");
        mysql_close(my_conn);
        pthread_exit(0);
    }

    inodes=malloc((count+1)*sizeof(__U64_TYPE));
    guestfs_launch(g);
    guestfs_mount(g,"/dev/sda1","/");
    printf("\n\n\n\nall the file number:%d",count);
    count=0;
    /****************************************使用guestfs读取文件inode元信息，更新文件状态**************************************************/
    while((row=mysql_fetch_row(res))){
        //printf("\nin multi:read from datebase the file name:%s",row[0]);
        //根据路径名获取inode等信息

        gs1=guestfs_statns(g,row[0]);
        if(gs1!=NULL){
            printf("\nin muti:the file name:%s",row[0]);
            /**未新添加的文件计算hash值*/
            /*更新文件元信息，计算md5*/
            update_file_info(g,gs1,my_conn,strsql,row);
            inodes[count]=gs1->st_ino;
            count++;
        }else{/**该文件在镜像中不存在,status=-1*/
            sprintf(strsql,"update files set lost=1 where files.id=%s",row[1]);
            if(mysql_query(my_conn,strsql)){
                printf("\nin muti:update the status of file failed!!!");
            }
            printf("\nin muti:the file don't exist in image!");
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
    type=malloc(5);
    get_filesystem_type(overlay_image_id,&type);
    printf("\ntype:%s",type);
    if(type!=NULL && strcmp(type,"EXT2")==0){
        printf("\nin muti: EXT2!!!");
        ext2_update_file_metadata(overlay_image_path,base_image_path,inodes,count,overlay_image_id);
    }else if(type!=NULL && strcmp(type,"NTFS")==0){
        printf("\nin muti: NTFS!!!");
        ntfs_update_file_metadata(overlay_image_path,base_image_path,inodes,count,overlay_image_id);
    }else{
        printf("\nin muti:error:unknown file system!");
    }
    free(type);

    //for(i=0;inodes[i];i++)printf("\n inode %d:%d",i,inodes[i]);
    free(inodes);
    /****************************************检查是否有需要还原的文件，开始尝试还原**************************************************/
    file_restore(g,my_conn,res,row,strsql,overlay_image_id,base_image_path);
    /****************************************读取数据库位于增量中的文件，计算哈希对比是否被篡改******************************************/
    file_is_modified_by_md5(g,my_conn,res,row,strsql,overlay_image_id);
normal:
    guestfs_umount(g,"/");
    guestfs_shutdown(g);
    guestfs_close(g);
    mysql_close(my_conn);
    printf("\nleaven multi thread.......\n\n\n\n\n\n");
    pthread_exit(0);
}
/**
 *author:liuyang
 *date  :2017/5/5
 *detail:get the base image guestfs_h,if not exist,create it
 *return guestfs_h*
 */
guestfs_h* get_baseimage_guestfs_h(char *base_image_path){
    int i;

    for(i=0;i<global_biga.len;i++){
        if(strcmp(global_biga.baseImageGuestfs[i].base_image_name,base_image_path)==0){
            if(global_biga.baseImageGuestfs[i].g==NULL){

                global_biga.baseImageGuestfs[i].g=guestfs_create();
                if(guestfs_add_drive(global_biga.baseImageGuestfs[i].g,base_image_path)!=0){
                    printf("\nguestfs_add_drive failed!");
                    return NULL;
                }
                guestfs_launch(global_biga.baseImageGuestfs[i].g);
                guestfs_mount(global_biga.baseImageGuestfs[i].g,"/dev/sda1","/");
                printf("\n%s guestfs_h builds successfully,return it!\n\n\n\n\n\n",base_image_path);
                return global_biga.baseImageGuestfs[i].g;
            }else{
                printf("\n%s guestfs_h have builded,return it!\n\n\n\n\n",base_image_path);
                return global_biga.baseImageGuestfs[i].g;
            }
        }
    }

fail:
    return NULL;
}
/**
 *author:liuyang
 *date  :2017/5/4
 *detail:begin file restore for the file need to restore
 *return void
 */
int file_restore(guestfs_h *g,MYSQL *my_conn,MYSQL_RES *res,MYSQL_ROW row,char strsql[],char *overlay_image_id,char *base_image_path){
    //printf("\n\n\n\n\nbegin file restore!");
    int baseHas=0;
    int flag=1;
    char dir_name[128]={NULL};
    char *backupRoot=NULL;
    char *filename;
    guestfs_h *gb;
    /*获取并判断备份根目录是否存在*/
    sql_get_backup_root(&backupRoot);
    printf("\nbackupRoot:%s",backupRoot);
    if(backupRoot!=NULL){
        if(opendir(backupRoot)==NULL){
            printf("\nthe backup root not exist,creating......");
            mkdir(backupRoot,S_IREAD|S_IWUSR|S_IXUSR);
        }
    }else{
        printf("\nget server host backup root failed!");
        return -1;
    }
    sprintf(strsql,"select files.absPath,files.baseHas,overlays.backupPath,files.status,files.id from files join overlays where  overlays.id=%s and files.overlayId=overlays.id and files.restore=1",overlay_image_id);
    if(mysql_query(my_conn,strsql)){
        printf("\nquery file need restore failed!");
        return -1;
    }
    res=mysql_store_result(my_conn);
    if(mysql_num_rows(res)<=0){
        printf("\nin multi:no file need to restore,return........\n\n\n\n\n\n\n");
        return 0;
    }
    printf("\n\n\n\n\nbegin file restore.....");

    //printf("\nbase image:%s",base_image_path);
    /**
     *多线程共享变量需要互斥锁
     *否则会造成句柄创建分配混乱
     */
    pthread_mutex_lock(&global_biga_mutex);
    gb=get_baseimage_guestfs_h(base_image_path);
    pthread_mutex_unlock(&global_biga_mutex);

    while(row=mysql_fetch_row(res)){
        /*创建增量镜像备份文件夹*/
        strcpy(dir_name,backupRoot);
        strcat(dir_name,"/");
        strcat(dir_name,row[2]);
        printf("\nthe hidden overlay dir in host:%s",dir_name);
        if(opendir(dir_name)==NULL){
            printf("\nhas not created overlay dir,begin create.....");
            mkdir(dir_name,S_IREAD|S_IWUSR|S_IXUSR);
            strcat(dir_name,"/temp");
            mkdir(dir_name,S_IREAD|S_IWUSR|S_IXUSR);
        }
        strcat(dir_name,"/temp");
        baseHas=atoi(row[2]);
        filename=strrchr(row[0], '/');
        strcat(dir_name,filename);

        printf("\nfile path :%s",dir_name);

        pthread_mutex_lock(&global_biga_mutex);
        guestfs_download(gb,row[0],dir_name);
        pthread_mutex_unlock(&global_biga_mutex);
        if(access(dir_name,0)!=0){
            printf("\noverlay id:%s;file %s not in base image,need restore by host backup!",overlay_image_id,row[0]);
            //TODO restore from local host backup
        }else{
            printf("\n\n\n\n\n\noverlay id:%s;file %s in base image,have download!",overlay_image_id,row[0]);
            if(atoi(row[3])==-1){
                printf("\nfile %s lost,upload directly!",row[0]);
                if(guestfs_upload(g,dir_name,row[0])==0){
                    printf("\nfile upload successful!!!");
                    /**更新还原信息*/
                    sql_file_restore_result(row[4],1,1);
                }else{
                    sql_file_restore_result(row[4],1,-1);
                }
                //fileid,restoreType

            }else{
                printf("\nfile %s is modified,delete firstly,then upload!",row[0]);
                if(guestfs_rm(g,row[0])==0){
                    printf("\nfile delete successfully!");
                }
                if(guestfs_upload(g,dir_name,row[0])==0){
                    printf("\nfile upload successfully!!!");
                    /**更新还原信息*/
                    sql_file_restore_result(row[4],1,1);
                }else{
                    sql_file_restore_result(row[4],1,-1);
                }
            }

        }
        dir_name[0]=NULL;
    }

}
/**
 *author:liuyang
 *date  :2017/5/4
 *detail:judge file modified by md5,just for file in overlay
 *return void
 */
int file_is_modified_by_md5(guestfs_h *g,MYSQL *my_conn,MYSQL_RES *res,MYSQL_ROW row,char strsql[],char *overlay_image_id){
    char *md5str=NULL;
    sprintf(strsql,"select files.absPath,files.hash,files.id from files join overlays where overlays.id=files.overlayId and files.dataPosition=2 and files.status=1 and files.restore=0 and overlays.id=%s",overlay_image_id);
    if(mysql_query(my_conn,strsql)){
        printf("\nin multi: query the file in overlay failed!");
        return -1;
    }
    res=mysql_store_result(my_conn);
    /**该镜像无内容在增量中的文件*/
    if(mysql_num_rows(res)<=0){
        printf("\nfile_is_modified_by_md5:no overlay file to be judge!");
        return 0;
    }
    while(row=mysql_fetch_row(res)){
        md5str=guestfs_checksum(g,"md5",row[0]);
        if(md5str==NULL){
            printf("\nguestfs cal md5 failed,maybe the file not exist!");
            continue;
        }
        //printf("file id:%s",row[2]);
        printf("\n\n\n\nfile:%s;\nthe normal file md5:%s;\nthe file md5 now:%s",row[0],row[1],md5str);
        if(strcmp(md5str,row[1])==0){
            free(md5str);
            printf("\nthe hash consistency,file security!");
            sprintf(strsql,"update files set files.isModified=0 where files.id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("\nfile_is_modified_by_md5: modify file isModified=0 failed!");
                continue;
            }
        }else{
            printf("\nfile:%s,the hash not consistency,file is modified!",row[0]);
            sprintf(strsql,"update files set files.isModified=1 where files.id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("\nin multi: modify file isModified=1 failed!");

            }
        }
    }

}
/**
 *author:liuyang
 *date  :2017/5/4
 *detail:update file info,espacially
 *return void
 */
void update_file_info(guestfs_h *g,struct guestfs_statns *gs1,MYSQL *my_conn,char strsql[],MYSQL_ROW row){
    char *md5str=NULL;
    if(strcmp(row[2],"0")==0){
        md5str=guestfs_checksum(g,"md5",row[0]);
        //printf("\nhash is:%s,len:%d",md5str,strlen(md5str));
        if(md5str!=NULL){
            printf("\nin muti:new detected file,hash:%s",md5str);
            sprintf(strsql,"update files set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld),mode=%d,hash='%s' where files.id=%s",
                    gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,gs1->st_mode,md5str,row[1]);
        }else{
            sprintf(strsql,"update files set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld),mode=%d where files.id=%s",
                    gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,gs1->st_mode,row[1]);
        }
    }else{
        sprintf(strsql,"update files set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld),mode=%d where files.id=%s",
                gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,gs1->st_mode,row[1]);
    }
    //printf("\nstrsql:%s\nlen of strsql:%d",strsql,strlen(strsql));
    if(mysql_query(my_conn,strsql)){
        printf("\nin muti:update the meta data of file failed!!!");
    }
    if(md5str!=NULL){
        free(md5str);
        md5str=NULL;
    }
}

/*
 *author:liuyang
 *date  :2017/3/28
 *detail:cal all vm file md5
 *return void
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
 *detail:统计虚拟机全部文件和增量文件占比,并计算在增量中文件的md5
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
    char *baseImage="/var/lib/libvirt/images/base.img";
    char *overlay="/var/lib/libvirt/images/snap1.img";
    char *filepath="/home/heaven/Downloads/truepath.txt";
    fp=fopen(filepath,"r");
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

        which_images_by_inode(baseImage,overlay,inode_number,line);
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
void system_test(){
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
        //fp1=fopen("/home/heaven/Downloads/truepath.txt","w");所属增量镜像

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
}
