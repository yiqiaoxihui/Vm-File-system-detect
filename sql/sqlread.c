#include "sqlread.h"

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
    char **baseImages;
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)) //连接detect数据库
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
        printf("\n host_status:%d,serverId:%d",host_status,serverId);
    }
    /**************************************************check whether the host exist in database***end*************************************************/
    /**************************************************find the base images*******************************************************/
    //select the baseImage by host id
    sprintf(strsql,"select baseImages.absPath,baseImages.status,baseImages.id\
            from servers join serverImages join baseImages      \
            where servers.id=%d and servers.id=serverImages.serverId and serverImages.baseImageId=baseImages.id",serverId);
    //printf("******************strsql***************:%s",strsql);
    if(mysql_query(my_conn,strsql)){
        printf("Query Error,query server images failed!n");
        goto fail0;
    }
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    printf("\n count:%d",count);
    if(count<=0){
        printf("\n no images in the host!");
        goto fail0;
    }
    //char *baseImages[count];
    baseImages=malloc(count * sizeof(char *));
    count=0;
    char str_baseImages_id[128];
    while((row=mysql_fetch_row(res))){
        baseImage_status=atoi(row[1]);
        if(baseImage_status==1){
            /**注意row[0]是char*类型，因此无需取地址*/
            printf("the normally base image len:%d",strlen(row[0]));
            baseImages[count]=row[0];
            /*为了使用mysql中in关键字*/
            strcat(str_baseImages_id,row[2]);
            strcat(str_baseImages_id,",");
            //baseImages[i]=strdup(baseImages[i]);
            printf("\n the base image path:%s",baseImages[count]);
            count++;
        }
        if(baseImage_status==2){
            sprintf(strsql,"UPDATE baseImages SET status=1 WHERE id=%s",row[2]);
            if(mysql_query(my_conn,strsql)){
                printf("Query Error,update baseImages status failed!");
                goto fail;
            }
        }
        if(baseImage_status==0){continue;};
    }
    //exist baseImages status =1
    /**************************************************find the base images****end***************************************************/
    /************************************************read overlay images by the host images id****************************************/
    if(count>0){
        printf("\n str_baseImages_id:%d,content:%s",strlen(str_baseImages_id),str_baseImages_id);
        /*去除最后一位逗号*/
        str_baseImages_id[strlen(str_baseImages_id)-1]='\0';
        printf("\n content:%s",str_baseImages_id);
        sprintf(strsql,"select overlays.absPath,overlays.status,overlays.id \
                from overlays join baseImages \
                where baseImages.id=overlays.baseImageId and baseImages.id in(%s)",str_baseImages_id);
        //printf("******************strsql***************:%s",strsql);
        if(mysql_query(my_conn,strsql)){
            printf("\nQuery Error,query overlay images failed!");
            goto fail;
        }
        res=mysql_store_result(my_conn);
        count=mysql_num_rows(res);
        printf("\n all overlay images count:%d",count);
        if(count<=0){
            printf("\n no overlay images in the server!");
            goto fail;
        }
        //image_abspath=malloc((count+1) * sizeof(char *));
        count=0;
        while((row=mysql_fetch_row(res))){
            overlayImage_status=atoi(row[1]);
            //printf("\n overlay image status:%d",overlayImage_status);
            if(overlayImage_status==1){
                /**注意row[0]是char*类型，因此无需取地址*/
                image_abspath[count]=row[0];
                image_id[count]=row[2];
                //baseImages[i]=strdup(baseImages[i]);
                printf("\n status is 1,the overlay image path:%s",image_abspath[count]);
                count++;
            }
            if(overlayImage_status==2){
                sprintf(strsql,"UPDATE overlays SET status=1 WHERE id=%s",row[2]);
                if(mysql_query(my_conn,strsql)){
                    printf("Query Error,update baseImages status failed!");
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
        printf("\n test all the overlay images path:%s,id:%s",image_abspath[i],image_id[i]);
    }

    //printf("\nsize of imagePath:%d",sizeof(image_abspath));
    /*******************************************read overlay images by the host images id***end******************************************/

    //test mysql_fetch_fields,mysql_num_fields
//    int num_fields = mysql_num_fields(res);
//    MYSQL_FIELD *fields = mysql_fetch_fields(res);
//    for(i = 0; i < num_fields; i++)
//    {
//       printf("Field %u is %s\n", i, fields[i].name);
//    }
//    int num_fields = mysql_num_fields(res);
//    while ((row = mysql_fetch_row(res)))
//    {
//       unsigned long *lengths;
//       lengths = mysql_fetch_lengths(res);
//       for(i = 0; i < num_fields; i++)
//       {
//           printf("[%.*s] ", (int) lengths[i],
//                  row[i] ? row[i] : "NULL");
//       }
//       printf("\n");
//    }
    mysql_close(my_conn);
    free(baseImages);
    printf("\n^^^^^^^^^^^^^^^^^^^^^end of read images^1^^^^^^^^^^^^^^^^^^^^^");
    return 1;
back:
    mysql_close(my_conn);
    free(baseImages);
    printf("\n^^^^^^^^^^^^^^^^^^^^^end of read images^0^^^^^^^^^^^^^^^^^^^^^");
    return 0;
fail:
    free(baseImages);
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
 int sql_update_file_metadata(char  *overlay_id,__U64_TYPE inode_number,__U16_TYPE mode,__U32_TYPE dtime,int inodePosition,int dataPosition){
    printf("\n\n\n\n\n\n\n begin sql_update_file_metadata......%s,%d,%d,%d,%d,%d",overlay_id,inode_number,mode,dtime,inodePosition,dataPosition);
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char strsql[256];
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!");
        return 0;
    }
    sprintf(strsql,"update file join overlays \
            set file.mode=%d,deleteTime=from_unixtime(%d),inodePosition=%d,dataPosition=%d \
            where overlays.id=%s and overlays.id=file.overlayId and file.inode=%ld",mode,dtime,inodePosition,dataPosition,overlay_id,inode_number);
    if(mysql_query(my_conn,strsql)){
        printf("in sql_update_file_metadata update failed!");
        return -1;
    }
    if(mysql_affected_rows(my_conn)>=0){
        printf("\n%d update successfully,leave sql_update_file_metadata.......\n\n\n\n\n\n",mysql_affected_rows(my_conn));
        return 1;
    }
    return 0;
 }


