#include "sqlread.h"
/*
 *author:liuyang
 *date  :2017/5/5
 *detail:file restore successful,update file info,modified=0,status=1
 *return void
 */
int sql_file_restore_success(char *file_id,int restoreType){
    printf("\n\n\n\n\nbegin sql file restore success........");
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char strsql[256];
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!n");
        exit(1);
    }
    sprintf(strsql,"update files set isModified=0,status=1,restore=0 where files.id=%s",file_id);
    if(mysql_query(my_conn,strsql)) //连接baseImages表
    {
        printf("\nQuery Error,update file restore successful info failed!");
        goto fail;
    }
    sprintf(strsql,"select restoreReason from fileRestore where fileId=%s",file_id);
    if(mysql_query(my_conn,strsql)) //连接baseImages表
    {
        printf("\nquery Error,query file restore failed!");
        goto fail;
    }
    res=mysql_store_result(my_conn); //取得表中的数据并存储到res中,mysql_use_result
    if(mysql_num_rows(res)<=0){//
        printf("\nnot find the restore file!");
        goto fail;
    }else{
        row=mysql_fetch_row(res);//打印结果
        sprintf(strsql,"insert into fileRestoreRecord (fileId,restoreReason,restoreType)values(%s,%s,%d)",file_id,row[0],restoreType);
        if(mysql_query(my_conn,strsql)) //连接baseImages表
        {
            printf("\ninsert fileRestoreRecord failed!");
            goto fail;
        }
    }
    sprintf(strsql,"delete from fileRestore where fileId=%s",file_id);
    if(mysql_query(my_conn,strsql)) //连接baseImages表
    {
        printf("\ndelete fileRestore failed!");
        goto fail;
    }
    mysql_close(my_conn);
    return 1;
fail:
    mysql_close(my_conn);
    return -1;
}
/*
 *author:liuyang
 *date  :2017/5/4
 *detail:get server host backup root
 *return void
 */
int sql_get_backup_root(char **backupRoot){
    char hostName[32];
    long int hostId;
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!");
        return -1;
    }
    //read server host name and host id
    if(gethostname(hostName,sizeof(hostName))){
        perror("gethostname!");
        goto fail;
    }
    hostId=gethostid();
    //查询数据库中是否有该服务器
    char strsql[256];
    sprintf(strsql,"select servers.backupRoot from servers where serverNumber=%d and name='%s'",hostId,hostName);
    //printf("%s",strsql);
    if(mysql_query(my_conn,strsql)) //连接baseImages表
    {
        printf("Query Error,query server backup root!");
        goto fail;
    }
    res=mysql_store_result(my_conn); //取得表中的数据并存储到res中,mysql_use_result
    if(mysql_num_rows(res)<=0){//
        printf("\nerror:the server not exist!");
        goto fail;
    }else{
        row=mysql_fetch_row(res);//打印结果
        *backupRoot=malloc(strlen(row[0])+1);
        strcpy(*backupRoot,row[0]);
    }
    mysql_close(my_conn);
    return 1;
fail:
    mysql_close(my_conn);
    return -1;
}
/*
 *author:liuyang
 *date  :2017/5/5
 *detail:get base image path and count
 *return 1:success,<=0:failed
 */
int sql_get_base_image_path(char **base_image_path,int *image_count){
    //host name
    char hostName[32];
    long int hostId;
    unsigned long count;
    int i;
    int serverId;
    //char *baseImages;
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    FILE *fp;
    char strsql[256];
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("Connect Error!n");
        exit(1);
    }
    /***********************************check whether the host exist in database***********************************/
    //read server host name and host id
    if(gethostname(hostName,sizeof(hostName))){
        perror("get host name failed!");
        goto fail0;
    }
    printf("\nread server info,get the image........");
    hostId=gethostid();
    //查询数据库中是否有该服务器

    sprintf(strsql,"select servers.id from servers where serverNumber=%d and name='%s'",hostId,hostName);
    //printf("%s",strsql);
    if(mysql_query(my_conn,strsql)) //连接baseImages表
    {
        printf("Query Error,query server failed!n");
        goto fail0;
    }
    res=mysql_store_result(my_conn); //取得表中的数据并存储到res中,mysql_use_result
    count=mysql_num_rows(res);
    if(count<=0){//
        printf("\n error:the server not exist!");
        goto fail0;
    }else if(count>1){
        printf("\n error:the server id repeat!");
        goto fail0;
    }else{
        row=mysql_fetch_row(res);//打印结果
        serverId=atoi(row[0]);
        //printf("\n host_status:%d,serverId:%d",host_status,serverId);
    }
    /************************************find the base images*****************************************/
    //select the baseImage by host id
    sprintf(strsql,"select baseImages.absPath,baseImages.status,baseImages.id\
            from servers join baseImages      \
            where servers.id=%d and servers.id=baseImages.server_id",serverId);
    //printf("******************strsql***************:%s",strsql);
    if(mysql_query(my_conn,strsql)){
        printf("Query Error,query server images failed!");
        goto fail0;
    }
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    //printf("\n baseImages count:%d",count);
    if(count<=0){
        printf("\n no images in the host!");
        goto fail0;
    }
    count=0;
    while((row=mysql_fetch_row(res))){
        /**first,judge the base image is exist or not!*/
        fp=fopen(row[0],"r");
        if(fp==NULL){
            //printf("\nthe base image:%s not exit!",row[0]);
            sprintf(strsql,"update baseImages set status=-1 where id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("Query Error,update server images status=-1 failed!");
            }
            continue;
        }else{
            base_image_path[count]=malloc(strlen(row[0])+1);
            strcpy(base_image_path[count],row[0]);
            count++;
            fclose(fp);
        }
    }
    *image_count=count;
    mysql_close(my_conn);
    return 1;
fail0:
    *image_count=0;
    mysql_close(my_conn);
    return -1;
}
/*
 *author:liuyang
 *date  :2017/3/19
 *detail:begin to read host image name
 *return 1:success,0:failed
 */
 int read_host_image_name(char **image_abspath,char **image_id){
    //host name
    char hostName[32];
    long int hostId;
    unsigned long long count;
    int i;
    int serverId;
    int baseImage_status,overlayImage_status;
    //char *baseImages;
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    FILE *fp;
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("Connect Error!n");
        exit(1);
    }
    /**************************************************check whether the host exist in database****************************************************/
    //read server host name and host id
    if(gethostname(hostName,sizeof(hostName))){
        perror("gethostname!");
        goto fail0;
    }
    printf("\nread server info,get the image........");
    hostId=gethostid();
    //查询数据库中是否有该服务器
    char strsql[256];
    sprintf(strsql,"select servers.id as sid,servers.IP as sip,servers.status as sstatus from servers where serverNumber=%d and name='%s'",hostId,hostName);
    //printf("%s",strsql);
    if(mysql_query(my_conn,strsql)) //连接baseImages表
    {
        printf("Query Error,query server failed!n");
        goto fail0;
    }
    res=mysql_store_result(my_conn); //取得表中的数据并存储到res中,mysql_use_result
    count=mysql_num_rows(res);
    if(count<=0){//
        printf("\n error:the server not exist!");
        goto fail0;
    }else if(count>1){
        printf("\n error:the server id repeat!");
        goto fail0;
    }else{
        row=mysql_fetch_row(res);//打印结果
        int host_status=atoi(row[2]);
        //host status
        if(host_status!=1){
            printf("\n the host stop detecting!");
            goto fail0;
        }
        serverId=atoi(row[0]);
        //printf("\n host_status:%d,serverId:%d",host_status,serverId);
    }
    /**************************************************check whether the host exist in database***end*************************************************/
    /**************************************************find the base images*******************************************************/
    //select the baseImage by host id
    sprintf(strsql,"select baseImages.absPath,baseImages.status,baseImages.id\
            from servers join baseImages      \
            where servers.id=%d and servers.id=baseImages.server_id",serverId);
    //printf("******************strsql***************:%s",strsql);
    if(mysql_query(my_conn,strsql)){
        printf("Query Error,query server images failed!");
        goto fail0;
    }
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    //printf("\n baseImages count:%d",count);
    if(count<=0){
        printf("\n no images in the host!");
        goto fail0;
    }
    //char *baseImages[count];
    //baseImages=malloc(count * sizeof(char));
    count=0;
    char str_baseImages_id[128]={NULL};
    while((row=mysql_fetch_row(res))){
        baseImage_status=atoi(row[1]);
        /**first,judge the base image is exist or not!*/
        fp=fopen(row[0],"r");
        if(fp==NULL){
            //printf("\nthe base image:%s not exit!",row[0]);
            sprintf(strsql,"update baseImages set status=-1 where id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("Query Error,update server images status=-1 failed!");
            }
            continue;
        }else if(baseImage_status==-1){
            sprintf(strsql,"update baseImages set status=0 where id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("Query Error,update server images status=0 failed!");
            }
            fclose(fp);
        }else{
            fclose(fp);
        }
        if(baseImage_status==1){
            /**注意row[0]是char*类型，因此无需取地址*/
            //printf("\nthe normally base image len:%d",strlen(row[0]));
            //baseImages[count]=row[0];
            /*为了使用mysql中in关键字*/
            //printf("\nthe str_baseimages_id:%s",str_baseImages_id);
            strcat(str_baseImages_id,row[2]);
            strcat(str_baseImages_id,",");
            //baseImages[i]=strdup(baseImages[i]);
            //printf("\n the status=1 base image path:%s",row[0]);
            count++;
        }
        if(baseImage_status==2){
            sprintf(strsql,"UPDATE baseImages SET status=1 WHERE id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("\nQuery Error,update baseImages status failed!");
                goto fail;
            }
        }
        if(baseImage_status==0){continue;};
    }
    //exist baseImages status =1
    /**************************************************find the base images****end***************************************************/
    /************************************************read overlay images by the host images id****************************************/
    if(count>0){
        //printf("\n str_baseImages_id:%d,content:%s",strlen(str_baseImages_id),str_baseImages_id);
        /*去除最后一位逗号*/
        str_baseImages_id[strlen(str_baseImages_id)-1]='\0';
        printf("\n str_baseImages_id:%s",str_baseImages_id);
        sprintf(strsql,"select overlays.absPath,overlays.status,overlays.id \
                from overlays join baseImages \
                where baseImages.id=overlays.baseImageId and baseImages.id in(%s)",str_baseImages_id);
        //printf("\n******************strsql***************:%s",strsql);
        if(mysql_query(my_conn,strsql)){
            printf("\nQuery Error,query overlay images failed!");
            goto fail;
        }
        res=mysql_store_result(my_conn);
        count=mysql_num_rows(res);
        //printf("\nall overlay images count:%d",count);
        if(count<=0){
            printf("\nno overlay images in the server!");
            goto fail;
        }
        //image_abspath=malloc((count+1) * sizeof(char *));
        count=0;
        while((row=mysql_fetch_row(res))){
            overlayImage_status=atoi(row[1]);
            //printf("\n overlay image status:%d",overlayImage_status);
            /**first,judge the overlay image is exist or not!*/
            fp=fopen(row[0],"r");
            if(fp==NULL){
                printf("\nthe overlay image:%s not exit!",row[0]);
                sprintf(strsql,"update overlays set status=-1 where id=%s",row[2]);
                if(mysql_query(my_conn,strsql)){
                    printf("Query Error,update server images status=-1 failed!");
                }
                continue;
            }else if(overlayImage_status==-1){
                sprintf(strsql,"update overlays set status=0 where id=%s",row[2]);
                if(mysql_query(my_conn,strsql)){
                    printf("Query Error,update server images status=0 failed!");
                }
                fclose(fp);
            }else{
                fclose(fp);
            }
            if(overlayImage_status==1){
                /**注意row[0]是char*类型，因此无需取地址*/
                image_abspath[count]=malloc(strlen(row[0])+1);
                image_id[count]=malloc(strlen(row[2])+1);
                strcpy(image_abspath[count],row[0]);
                strcpy(image_id[count],row[2]);
                //image_abspath[count]=row[0];
                //image_id[count]=row[2];
                //baseImages[i]=strdup(baseImages[i]);
                //printf("\nstatus is 1,the overlay image path:%s",image_abspath[count]);
                count++;
            }
            if(overlayImage_status==2){
                sprintf(strsql,"UPDATE overlays SET status=1 WHERE id=%s",row[2]);
                if(mysql_query(my_conn,strsql)){
                    printf("\nQuery Error,update baseImages status failed!");
                    goto fail;
                }
            }
            if(overlayImage_status==0){continue;};
        }
        image_abspath[count]=NULL;
        image_id[count]=NULL;
    }else{
        printf("\n no overlay image status is 1");
        goto fail;
    }
    /**没有可用的镜像*/
    if(image_id[0]==NULL){
        goto back;
    };
    for(i=0;image_abspath[i];i++){
        printf("\n all need detect overlay images path:%s,id:%s",image_abspath[i],image_id[i]);
    }

    //printf("\nsize of imagePath:%d",sizeof(image_abspath));
    /*******************************************read overlay images by the host images id***end******************************************/
    mysql_close(my_conn);
    //free(baseImages);
    printf("\n^^^^^^^^^^^^^^^^^^^^^end of read images^1^^^^^^^^^^^^^^^^^^^^^\n\n\n\n\n\n\n\n");
    return 1;
back:
    mysql_close(my_conn);
    //free(baseImages);
    printf("\n^^^^^^^^^^^^^^^^^^^^^end of read images^0^^^^^^^^^^^^^^^^^^^^^");
    return 0;
fail:
    //free(baseImages);
fail0:
    mysql_close(my_conn);
    printf("\n^^^^^^^^^^^^^^^^^^^^^end of read images^-1^^^^^^^^^^^^^^^^^^^^^");
    return -1;
 }
/*
 *author:liuyang
 *date  :2017/3/27
 *detail:更新文件信息，inode及数据块位置
 *return 1:success,0:failed
 */
 int sql_update_file_metadata(char  *overlay_id,__U64_TYPE inode_number,int inodePosition,int dataPosition){
    printf("\n\n\n\n\n\n\n begin sql_update_file_metadata......%s,%ld,%d,%d",overlay_id,inode_number,inodePosition,dataPosition);
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char strsql[256];
    int baseHas=0;
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!");
        return 0;
    }
    /**************************************根据检测结果更新文件位置**************************************/
    sprintf(strsql,"update files join overlays \
            set inodePosition=%d,dataPosition=%d\
            where overlays.id=%s and overlays.id=files.overlayId and files.inode=%ld",inodePosition,dataPosition,overlay_id,inode_number);
    if(mysql_query(my_conn,strsql)){
        printf("in sql_update_file_metadata update failed!");
        goto fail;
    }
    if(mysql_affected_rows(my_conn)>=0){
        printf("\n%d update successfully,leave sql_update_file_metadata.......\n\n\n\n\n\n",mysql_affected_rows(my_conn));
    }
    /**************************************首次检测时，根据检测结果，单向判断文件是否在原始镜像中**************************************/
    if(inodePosition==1 || dataPosition==1){
        baseHas=1;
    }
    sprintf(strsql,"select files.firstAddFlag from files join overlays where overlays.id=%s and overlays.id=files.overlayId and files.inode=%ld",overlay_id,inode_number);
    if(mysql_query(my_conn,strsql)){
        printf("in sql_update_file_metadata  query file firstAddFlag failed!");
        goto fail;
    }
    res=mysql_store_result(my_conn);
    if(mysql_num_rows(res)<=0){
        printf("\n can not find the file system type!");
        goto fail;
    }
    row=mysql_fetch_row(res);
    if(strcmp(row[0],"0")==0){
        sprintf(strsql,"update files join overlays set files.firstAddFlag=1,files.baseHas=%d where overlays.id=%s and overlays.id=files.overlayId and files.inode=%ld",baseHas,overlay_id,inode_number);
        if(mysql_query(my_conn,strsql)){
            printf("in sql_update_file_metadata update failed!");
            goto fail;
        }
        if(mysql_affected_rows(my_conn)>=0){
            printf("\nupdate successfully,the file is the first  time detected,leave sql_update_file_metadata.......\n\n\n\n\n\n");
        }
    }
    mysql_close(my_conn);
    return 1;
fail:
    mysql_close(my_conn);
    return -1;
 }
/*
 *author:liuyang
 *date  :2017/5/3
 *detail:获取文件类型
 *return char*
 */
int get_filesystem_type(char *overlayid,char **type){
    //printf("\n\n\n\n\nbegin get_filesystem_type......%s",overlayid);
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char strsql[512];
    //char *type=NULL;
    my_conn=mysql_init(NULL);
    int count=0;
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!");
        return 0;
    }
    sprintf(strsql,"select filesystemType.name from overlays join baseImages join filesystemType where overlays.id=%s and overlays.baseImageId=baseImages.id and baseImages.type=filesystemType.id",overlayid);
    if(mysql_query(my_conn,strsql)){
        printf("in get_filesystem_type update failed!");
        goto fail;
    }
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    //printf("\n baseImages count:%d",count);
    if(count<=0){
        printf("\n can not find the file system type!");
        goto fail;
    }
    row=mysql_fetch_row(res);
    strcpy(*type,row[0]);
    printf("\nthe file system type:%s",row[0]);
    if(strcmp(*type,"EXT2")==0){
        strcpy(*type,"EXT2");
    }else if(strcmp(type,"NTFS")==0){
        strcpy(*type,"NTFS");
    }else{
        strcpy(*type,"");
    }
    mysql_close(my_conn);
    printf("\nleave get_filesystem_type.....%s\n\n\n\n",*type);
    return 1;
fail:
    mysql_close(my_conn);
    return -1;
}

