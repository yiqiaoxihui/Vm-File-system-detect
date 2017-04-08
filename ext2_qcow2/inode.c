
//#include "inode.h"
//#include <mysql/mysql.h>

#include "inode.h"
#include <sys/stat.h>
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
    while(!feof(fp)){
        all_file_count++;
        fgets(line,256,fp);
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
        //break;

    }
    for(i=0;i<overlay_file_count;i++){
        md5str=guestfs_checksum(g,"md5",overlay_filepath[i]);
        if(md5str==NULL){
            continue;
        }
        printf("\nmd5:%s|file in overlay:%s",md5str,overlay_filepath[i]);
        //fputs(overlay_filepath[i],fp2);
        //fputc('\n',fp2);
    }
    printf("\nall file:%.0f;\nfile in overlay:%d;\ninode in overlay:%0.f;\nerror file:%.0f;\n",all_file_count,overlay_file_count,inode_in_overlay_file_count,error_file_count);
    printf("\nread_error:%0.f;\nblockInOverlay error:%d;\ninodeInOverlay:%d;\nmagic_error:%d\n",read_error,blockInOverlay_error,inodeInOverlay_error,magic_error);
    printf("\n增量文件占比:%.5f\%",(((float)overlay_file_count)/(all_file_count-error_file_count-read_error))*100);
    guestfs_free_statns(gs1);
    guestfs_umount (g, "/");
    guestfs_shutdown (g);
    guestfs_close (g);
    fclose(fp);
    //fclose(fp2);
    //fclose(fp1);
    //free(md5str);
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
    char strsql[256];
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    guestfs_h *g=guestfs_create();
    __U64_TYPE  *inodes;
    struct guestfs_statns *gs1;
    int count;
    int i;

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
    {
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
        fp=fopen("/home/heaven/Downloads/test.txt","r");
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
        guestfs_add_drive(g,overlay_image_path);
        guestfs_launch(g);
        guestfs_mount_ro(g,"/dev/sda1","/");
        while(!feof(fp)){
            all_file_count++;
            fgets(line,256,fp);
            line[strlen(line)-1]='\0';
            gs1=guestfs_lstatns(g,line);
            if(gs1==NULL){
                error_file_count++;
                printf("\nthe file not exist:%.0f",error_file_count);
                continue;
            }
    //        line[strlen(line)]='\n';
    //        fputs(line,fp1);
            inode_number=gs1->st_ino;
            which_images_by_inode("/var/lib/libvirt/images/base.img",overlay_image_path,inode_number,line);
            //break;

        }
        all_file_count--;
        for(i=0;i<overlay_file_count;i++){
            md5str=guestfs_checksum(g,"md5",overlay_filepath[i]);
            if(md5str==NULL){
                continue;
            }
            printf("\nmd5:%s|file in overlay:%s",md5str,overlay_filepath[i]);
            //fputs(overlay_filepath[i],fp2);
            //fputc('\n',fp2);
        }
        printf("\nall file:%.0f;\nfile in overlay:%d;\ninode in overlay:%0.f;",all_file_count,overlay_file_count,inode_in_overlay_file_count);
        printf("\nerror file:%.0f;\nread_error:%0.f;\nblockInOverlay error:%d;\ninodeInOverlay:%d;\nmagic_error:%d\n",error_file_count,read_error,blockInOverlay_error,inodeInOverlay_error,magic_error);
        printf("\n增量文件占比:%.2f%%",(((float)overlay_file_count)/(all_file_count-error_file_count-read_error))*100);
        guestfs_free_statns(gs1);
        guestfs_umount (g, "/");
        guestfs_shutdown (g);
        guestfs_close (g);
        fclose(fp);
    }

//
//    my_conn=mysql_init(NULL);
//    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)) //连接detect数据库
//    {
//        printf("\nConnect Error!");
//        mysql_close(my_conn);
//        pthread_exit(0);
//    }
//    /****************************************读取该增量镜像对应的监控文件**************************************************/
//    sprintf(strsql,"select file.absPath,file.id \
//            from file join overlays \
//            where overlays.id=file.overlayId and file.status=1 and overlays.id=%s",overlay_image_id);
//    if(mysql_query(my_conn,strsql)){
//        printf("\nin multi: query the image file failed!");
//        mysql_close(my_conn);
//        pthread_exit(0);
//    }
//    res=mysql_store_result(my_conn);
//    count=mysql_num_rows(res);
//    /**该镜像无徛控文件，无需加载*/
//    if(count<=0){
//        printf("\n<<<<<<<<<------in multi:no file in the image,thread exit!---->>>>>>>>>>");
//        mysql_close(my_conn);
//        pthread_exit(0);
//    }
//    /****************************************读取该增量镜像对应的监控文件***end***********************************************/
//
//    inodes=malloc((count+1)*sizeof(__U64_TYPE));
//    //printf("\n$$$$$$$$$$$$$$count:%d,sizeof(inodes):%d$$$$$$$$$$$$$$$$",count,sizeof(inodes));
//    printf("\n****$$$$*****begin read image by libguestfs***$$$$*****");
//    guestfs_add_drive(g,overlay_image_path);
//    guestfs_launch(g);
//    //gues
//    guestfs_mount(g,"/dev/sda1","/");
//    count=0;
//    /****************************************使用guestfs读取文件inode元信息，更新文件状态**************************************************/
//    while((row=mysql_fetch_row(res))){
//        //printf("\nin multi:read from datebase the file name:%s",row[0]);
//        //根据路径名获取inode等信息
//
//        gs1=guestfs_statns(g,row[0]);
//        if(gs1!=NULL){
//            sprintf(strsql,"update file \
//                    set inode=%ld,size=%ld,modifyTime=from_unixtime(%ld),\
//                    createTime=from_unixtime(%ld),accessTime=from_unixtime(%ld),status=1 \
//                    where file.id=%s",
//                    gs1->st_ino,gs1->st_size,gs1->st_mtime_sec,gs1->st_ctime_sec,gs1->st_atime_sec,row[1]);
//            if(mysql_query(my_conn,strsql)){
//                printf("\nupdate the meta data of file failed!!!");
//            }
//            inodes[count]=gs1->st_ino;
//            printf("\nthe inodes:%ld,gs1_st_ino:%ld",inodes[count],gs1->st_ino);
//            //TODO:restore file from base image
//            count++;
//        }else{/**该文件在镜像中不存在*/
//            sprintf(strsql,"update file set status=0 where file.id=%s",row[1]);
//            if(mysql_query(my_conn,strsql)){
//                printf("\nupdate the status of file failed!!!");
//            }
//            printf("\nthe file don't exist in image!");
//        }
//        //guestfs_free_statns(gs1);
//    }
//    if(count==0){
//        printf("\navaliable inode of file no! leaven multi thread.......\n\n\n\n\n\n");
//        mysql_close(my_conn);
//        free(inodes);
//        pthread_exit(0);
//    }
//    inodes[count]=0;
//    for(i=0;i<count;i++)printf("\n i:%d,inode:%ld",i,inodes[i]);
//    guestfs_umount(g,"/");
//    guestfs_shutdown(g);
//    guestfs_close(g);
//    mysql_close(my_conn);
//    update_file_metadata(overlay_image_path,base_image_path,&inodes,count,overlay_image_id);
//    //for(i=0;inodes[i];i++)printf("\n inode %d:%d",i,inodes[i]);
//    /****************************************使用guestfs读取文件inode元信息，更新文件状态***end***********************************************/
//
//    free(inodes);
//    printf("\nleaven multi thread.......\n\n\n\n\n\n");

}
/*
 *author:liuyang
 *date  :2017/3/23
 *detail:由inodes偏移位置判断inodes在哪个镜像中
 *return
 */
int inodes_in_overlay(char *baseImage,char *qcow2Image,__U32_TYPE *block_offset,__U32_TYPE *bytes_offset_into_block,__U16_TYPE BLOCK_BITS,struct ext2_inode *inodes,int inode_count){
    FILE *l_fp;
    QCowHeader *header;
    __U32_TYPE CLUSTER_BITS;
    __U32_TYPE l1_index;
    __U32_TYPE l2_index;
    __U64_TYPE L1_TABLE_OFFSET;
    __U32_TYPE cluster_offset;
    __U32_TYPE block_into_cluster;
    __U32_TYPE blocks_bytes_into_cluster;
    __U32_TYPE L2_BITS;
    __U64_TYPE l1_entry_offset;
    __U64_TYPE l2_table_offset;
    __U64_TYPE l2_entry_offset;
    __U64_TYPE data_offset;//data offset,Must be aligned to a cluster boundary.
    //char *read_backingfile_name;
    int i,j;
    int count;

    printf("\n\n\n\n\n\n\n begin inode_in_overlay.........");
    l_fp=fopen(qcow2Image,"r");
    if(l_fp==NULL){
        printf("\n open qcow2Image failed!");
        return -1;
    }
    /***************************读取增量镜像header结构体**************************************************/
    header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader));
    if(fread(header,sizeof(struct QCowHeader),1,l_fp)<=0){
        printf("\n read qcow2header failed!");
        free(header);
        fclose(l_fp);
        return -2;
    }
    header->version=__bswap_32(header->version);
    L1_TABLE_OFFSET=__bswap_64(header->l1_table_offset);
//    header->backing_file_offset=__bswap_64(header->backing_file_offset);
//    header->backing_file_size=__bswap_32(header->backing_file_size);
    CLUSTER_BITS=__bswap_32(header->cluster_bits);
    L2_BITS=CLUSTER_BITS-3;
    /***************************提取原始镜像名，检查是否一致**************************************************/
//    printf("\n the qcow2 version%d",header->version);
//    fseek(l_fp,header->backing_file_offset,SEEK_SET);
//    int len=header->backing_file_size-9;
//    read_backingfile_name=(char *)malloc(header->backing_file_size+1);
//    if(fgets(read_backingfile_name,header->backing_file_size+1,l_fp)==NULL){
//        printf("\n read backingfile name error!");
//        return -3;
//    }
//    printf("\n size:%d,backing file name:%s,string len %ld",header->backing_file_size,read_backingfile_name,sizeof(read_backingfile_name));
//    if(strstr(baseImage,read_backingfile_name)==NULL){
//        printf("\n wrong overlay images!");
//        return -4;
//    }
    /***************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配****************************************************************/
    //the beginning 1M(256 blocks) not belongs to ext4 fs,I think.
    for(i=0;i<inode_count;i++){
        block_offset[i]+=256;
        cluster_offset=block_offset[i]/(1<<(CLUSTER_BITS-BLOCK_BITS));      /**cluster偏移块数*/
        block_into_cluster=block_offset[i]%(1<<(CLUSTER_BITS-BLOCK_BITS));  /**cluster内块偏移数*/
        blocks_bytes_into_cluster=block_into_cluster<<BLOCK_BITS;           /**cluster块偏移的字节数*/
        l1_index=cluster_offset>>L2_BITS;                                   /**l1表的偏移项数*/
        l2_index=cluster_offset & ((1<<L2_BITS)-1);                         /**l2表的偏移项数*/
        printf("\nblock_offset%d,\ncluster offset:%d,\nl1 index:%d,l2 index:%x,\nblocks_bytes_into_cluster%x",block_offset[i],cluster_offset,l1_index,l2_index<<3,blocks_bytes_into_cluster);
        l1_entry_offset=L1_TABLE_OFFSET+(l1_index<<3);
        if(fseek(l_fp,l1_entry_offset,SEEK_SET)){
            printf("\n seek to l1 offset failed!");
            continue;
        }
        if(fread(&l2_table_offset,sizeof(l2_table_offset),1,l_fp)<=0){
            printf("\n read the l2 offset failed!");
            continue;
        }
        l2_table_offset=__bswap_64(l2_table_offset) & L1E_OFFSET_MASK;
        printf("\nsize:%ld l2_table_offset:%lx",sizeof(l2_table_offset),l2_table_offset);
        /***************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配,如果数据块在cluster中分配，读取相应的inode结构体信息****************************************************************/
        if(!l2_table_offset){
            printf("\n l2 table has not been allocated,the data must in backing file!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        l2_entry_offset=l2_table_offset+(l2_index<<3);
        if(fseek(l_fp,l2_entry_offset,SEEK_SET)){
            printf("\n seek to l2_entry_offset failed!");
            continue;
        }
        if(fread(&data_offset,sizeof(data_offset),1,l_fp)<=0){
            printf("\n read data offset failed!");
            continue;
        }
        data_offset=__bswap_64(data_offset);
        if(!(data_offset & L2E_OFFSET_MASK)){
            printf("\n l2 entry has not been allocated,the data must in backing file!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        printf("\nsize:%ld,the data offset:%x",sizeof(data_offset),data_offset);

        if(fseek(l_fp,0,SEEK_SET)){
            printf("\n seek to data_offset failed!");
            continue;
        }
        count=data_offset/(1<<CLUSTER_BITS);    /**TODO偏移以64k为单位，一定能整除*/
        printf("\ncount:%d,cluster size:%d",count,1<<CLUSTER_BITS);
        for(j=0;j<count;j++){
            //printf("\n************count********:%d",i);
            if(fseek(l_fp,1<<CLUSTER_BITS,SEEK_CUR)){
                printf("\n seek to data_offset failed!");
                break;
            }
        }

        if(fseek(l_fp,blocks_bytes_into_cluster+bytes_offset_into_block[i],SEEK_CUR)){
            printf("\n seek to data_offset failed!");
            continue;
        }

        if(fread(&inodes[i],sizeof(struct ext2_inode),1,l_fp)<=0){
            printf("\n read inode failed!");
            continue;
        }
        //printf("\nread inode mode:%d",inodes[i].i_mode);
    }

    //free(read_backingfile_name);

    printf("\nleave inode_in_overlay......\n\n\n\n\n\n\n ");
    fclose(l_fp);
    free(header);
    return 1;
}
/*
 *author:liuyang
 *date  :2017/3/23
 *detail:由inode判断文件的位置,更新文件信息
 *return
 */
int update_file_metadata(char *overlay_image_path,char base_image_path[],__U64_TYPE **inodes,int inode_count,char *overlay_id){
    printf("\n\n\n\n\n\n\n\n begin update_file_metadata......%ld",*((*inodes)));
    struct ext2_super_block *es;
    struct ext2_group_desc *gdesc;
    struct ext4_extent_header *eh;
    struct ext4_extent *ee;
    struct ext4_extent_idx *ext_idx;
    struct ext2_inode *e_ino,*base_ino;
    int i,j;
    int super_block=1;
    __U32_TYPE inodes_per_group;
    __U32_TYPE blocks_per_group;
    __U32_TYPE block_size,block_bits;
    __U16_TYPE inode_size;
    __U32_TYPE inode_per_group;
    __U16_TYPE reserved_gdt_blocks;//Number of reserved GDT entries for future filesystem expansion.
    __U32_TYPE inode_in_which_blockgroup;
    __U32_TYPE offset_in_bg_desc;
    __U32_TYPE inode_index_in_inodetable;
    __U32_TYPE *inode_offset_in_inodetable;//inode在inodetable的字节偏移
    __U32_TYPE *begin_block_of_inodetable;//块组inodetable起始块
    __U32_TYPE inode_block_offset_in_inodetable;
    __U32_TYPE *inode_bytes_offset_into_inodetable;//inode在块内偏移字节
    __U32_TYPE *inode_block_number;//inode在第几个块
    __U32_TYPE data_block_offset,index_block_offset;
    __U64_TYPE begin_block_offset_of_inodetable;
    FILE *bi_fp;

    //printf("\nbaseImage:%s",base_image_path);
    bi_fp=fopen(base_image_path,"r");
    if(bi_fp==NULL){
        printf("\n error:open failed!");
        return -1;
    }
    //printf("\nopen baseImage successfully!,%ld",sizeof(struct ext2_super_block));
    /***************************************************读取超级快元信息**************************************************/
   //offset 1M to begin
    fseek(bi_fp,Meg,SEEK_SET);
    //offset 1k to cur
    if(fseek(bi_fp,Kilo,SEEK_CUR)){
        printf("\n seek to super block failed!");
        fclose(bi_fp);
        return -1;
    }
    /*
    *read the super block info
    *fread return the number of reading,respected 1
    *must malloc zone for pointer es,es should point to a place it knows.
    *as we know no matter where the file in,the super block never change
    */
    es=(struct ext2_super_block*)malloc(sizeof(struct ext2_super_block));

    if(fread(es,sizeof(struct ext2_super_block),super_block,bi_fp)<=0){
        printf("\n read to super block failed!");
        free(es);
        fclose(bi_fp);
        return -1;
    }
    block_bits=10+es->s_log_block_size;
    block_size=1<<block_bits;
    inode_per_group=es->s_inodes_per_group;
    inode_size=es->s_inode_size;

    /********计算该inode所在块组，从块组描述符中读取该块组inodetable的的起始块号，而这些信息在块组描述符表中不变*******/
    /*
     *1.calculate which bg desc entry the inode in
     *2.get the beginning block number of inodetable the inode in from desc entry
     *3.calculate the exactly block number the inode in
     */
    inode_block_number=malloc(inode_count*sizeof(__U32_TYPE));
    inode_bytes_offset_into_inodetable=malloc(inode_count*sizeof(__U32_TYPE));
    begin_block_of_inodetable=malloc(inode_count*sizeof(__U32_TYPE));
    inode_offset_in_inodetable=malloc(inode_count*sizeof(__U32_TYPE));
    gdesc=(struct ext2_group_desc*)malloc(sizeof(struct ext2_group_desc));
    e_ino=malloc(inode_count*sizeof(struct ext2_inode));
    /*************************开始统计inode集合的偏移信息************************************/
    for(i=0;i<inode_count;i++){
        inode_in_which_blockgroup=(*((*inodes)+i)-1)/inode_per_group;
        //the offest into the bg desc
        offset_in_bg_desc=inode_in_which_blockgroup*BG_DESC_SIZE;
        /*
         *as we know no matter where the file in,the desc table never change
         *test inode:133415 16
         */
        fseek(bi_fp,Meg,SEEK_SET);fseek(bi_fp,block_size,SEEK_CUR);
        fseek(bi_fp,offset_in_bg_desc,SEEK_CUR);
        if(fread(gdesc,sizeof(struct ext2_group_desc),1,bi_fp)<=0){
            printf("\nread group descriptor entry failed!");
            goto fail;
        }
        begin_block_of_inodetable[i]=gdesc->bg_inode_table;
        /***************************由inodetable的起始块号,计算inode真正偏移块号和块内偏移***************************/
        //index of inodetable 3878
        inode_index_in_inodetable=(*((*inodes)+i)-1)%inode_per_group;
        //offset into inodetable 992768
        inode_offset_in_inodetable[i]=inode_index_in_inodetable*inode_size;

        inode_block_offset_in_inodetable=inode_offset_in_inodetable[i]/block_size;
        inode_bytes_offset_into_inodetable[i]=inode_offset_in_inodetable[i]%block_size;

        //the real block number of inode,
        inode_block_number[i]=begin_block_of_inodetable[i]+inode_block_offset_in_inodetable;
        //printf("\n inode block offest in inodetable:%d,\n the real block number of inode%d,\n inode_bytes_offset_into_inodetable%x",inode_block_offset_in_inodetable,inode_block_number[i],inode_bytes_offset_into_inodetable[i]);
    }

    //STOP IN HHERE,TOMORROW CONTINUE!
    int inode_status=inodes_in_overlay(base_image_path,overlay_image_path,inode_block_number,inode_bytes_offset_into_inodetable,block_bits,e_ino,inode_count);
    if(inode_status<0){
        printf("\n error:inode_in_overlay() fail!");
        goto fail;
    }
    for(i=0;i<inode_count;i++){
        printf("\nread inode info:%ld size:%d",*((*inodes)+i),e_ino[i].i_size);
        /**暂时用mode为0表示inode不在overlay中*/
        if(e_ino[i].i_mode!=0){
            printf("\n *******inode in overlay,read inode from overlay successful!!!*******");

            eh=(struct ext4_extent_header *)((char *) &(e_ino[i].i_block[0]));
            printf("\n eh->magic:%x",eh->eh_magic);
            if(eh->eh_magic==0xf30a){
                if(eh->eh_depth==0 && eh->eh_entries>0){
                    //it is leaf node
                    ee=EXT_FIRST_EXTENT(eh);
                    data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
                    //695379
                    printf("\n ee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));
                    /**由块偏移判断数据位置*/
                    int block_status=blockInOverlay(overlay_image_path,data_block_offset,block_bits);
                    if(block_status==1){
                        printf("\n *********************data blocks in overlay!!!******************");
                        sql_update_file_metadata(overlay_id,*((*inodes)+i),e_ino[i].i_mode,e_ino[i].i_dtime,2,2);
                    }else if(block_status==0){
                        printf("\n *********************data blocks in baseImage!!!******************");
                        sql_update_file_metadata(overlay_id,*((*inodes)+i),e_ino[i].i_mode,e_ino[i].i_dtime,2,1);
                    }else{
                        printf("\n *********************blockInOverlay fail*********************");
                        goto fail;
                    }
                }else if(eh->eh_depth>0 && eh->eh_entries>0){
                    //it is index node,
                    ext_idx = EXT_FIRST_INDEX(eh);
                    index_block_offset=(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo;
                    printf("\n ext_idx node block:%x",index_block_offset);
                }
            }else{
                printf("\n corrupt data in inode block!");
                goto fail;
            }
        }else{/**inode在原始镜像中，因此数据也在增量镜像中*/
            /*
            *into the head of second block,read the blockgroup desc  entry,
            *and get the number of first block of the inodetable
            */
            printf("\n ***************inode in baseImage,read inode from baseImage successful,so data must be in baseImage!!!**************");
            fseek(bi_fp,Meg,SEEK_SET);
            //seek to the inodetable block,254320,for in case of overflow.
            begin_block_offset_of_inodetable=(__U64_TYPE)begin_block_of_inodetable[i]*block_size;
//            for(j=0;j<begin_block_of_inodetable[i];j++){
//                fseek(bi_fp,block_size,SEEK_CUR);
//            }
            fseek(bi_fp,begin_block_offset_of_inodetable,SEEK_CUR);
            //offset into the inodetable
            fseek(bi_fp,inode_offset_in_inodetable[i],SEEK_CUR);
            //read the inode entry,only 128 bytes for ext2,but 256 for ext4,read 128 bytes is compatibility
            base_ino=(struct ext2_inode*)malloc(sizeof(struct ext2_inode));
            fread(base_ino,sizeof(struct ext2_inode),1,bi_fp);
            printf("\n the mode of this file:%x",base_ino->i_mode);


            sql_update_file_metadata(overlay_id,*((*inodes)+i),base_ino->i_mode,base_ino->i_dtime,1,1);
            /*
             *for ext4,use extent tree instead of block pointer
             */
            eh=(struct ext4_extent_header *)((char *) &(base_ino->i_block[0]));
            printf("\n eh->magic:%x",eh->eh_magic);
            if(eh->eh_magic==0xf30a){
                if(eh->eh_depth==0 && eh->eh_entries>0){
                    //it is leaf node
                    ee=EXT_FIRST_EXTENT(eh);
                    data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
                    printf("\n ee block number:%x",data_block_offset);
                    //695379
                    printf("\n ee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));

                }else if(eh->eh_depth>0 && eh->eh_entries>0){
                    //it is index node,
                    ext_idx = EXT_FIRST_INDEX(eh);
                    printf("\n ext_idx inode block:%x",(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo);
                }
            }//
            free(base_ino);
        }
    }
    free(es);
    free(inode_block_number);
    free(inode_bytes_offset_into_inodetable);
    free(begin_block_of_inodetable);
    free(inode_offset_in_inodetable);
    free(gdesc);
    free(e_ino);
    fclose(bi_fp);
    printf("\n leave update_file_metadata.......\n\n\n\n\n\n");
    return 1;
fail:
    fclose(bi_fp);
    free(es);
    free(inode_block_number);
    free(inode_bytes_offset_into_inodetable);
    free(begin_block_of_inodetable);
    free(inode_offset_in_inodetable);
    free(gdesc);
    free(e_ino);
    printf("\n leave update_file_metadata.......\n\n\n\n\n\n");
    return 0;
}

/*
 *author:liuyang
 *date  :2017/3/22
 *detail:判断增量镜像中原始镜像路径和给出名称是否一致
 *return 1:true,0:false
 */
 int is_base_image_identical(char *overlay_image_id,char base_image_path[]){
    char *overlay_image_path;
    char *base_image_name;
    char strsql[256];
    char *read_backingfile_name;
    FILE *l_fp;
    QCowHeader header;
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    //begin to connect mysql database
    my_conn=mysql_init(NULL);
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","detect",0,NULL,0)){
        printf("\nConnect Error!");
        return -1;
    }
    //printf("\n in is base image identical overlay_image_id:%s",overlay_image_id);

    sprintf(strsql,"select baseImages.name,overlays.absPath,baseImages.absPath    \
            from overlays join baseImages                   \
            where overlays.id=%s and overlays.baseImageId=baseImages.id",overlay_image_id);
    if(mysql_query(my_conn,strsql)){
        printf("\n query baseImages and overlays failed!baseImageId:%s",overlay_image_id);
        fclose(l_fp);
        mysql_close(my_conn);
        return -1;
    }
    res=mysql_use_result(my_conn);
    row=mysql_fetch_row(res);
    if(row!=NULL){
        base_image_name=row[0];
        overlay_image_path=row[1];
        strcpy(base_image_path,row[2]);
        //printf("\n<<<<<<<<<<<<<<<<<<<<<base image path:%s>>>>>>>>>>>>>>>>",*base_image_path);
        l_fp=fopen(overlay_image_path,"r");
        if(l_fp==NULL){
            printf("\n open qcow2Image failed!");
            mysql_close(my_conn);
            return -1;
        }
        if(fread(&header,sizeof(struct QCowHeader),1,l_fp)<=0){
            printf("\n read qcow2header failed!");
            mysql_close(my_conn);
            fclose(l_fp);
            return -1;
        }
        header.backing_file_offset=__bswap_64(header.backing_file_offset);
        header.backing_file_size=__bswap_32(header.backing_file_size);
        /**************************提取原始镜像名，检查是否一致************************************************/
        //printf("\n the qcow2 version%d",header.version);
        fseek(l_fp,header.backing_file_offset,SEEK_SET);
        read_backingfile_name=(char *)malloc(header.backing_file_size+1);
        if(fgets(read_backingfile_name,header.backing_file_size+1,l_fp)==NULL){
            printf("\n read backingfile name error!");
            goto fail;
        }
        //printf("\n size:%d,backing file name:%s,string len %ld",header.backing_file_size,read_backingfile_name,sizeof(read_backingfile_name));
        if(strstr(base_image_name,read_backingfile_name)==NULL){
            //printf("\n wrong overlay images!");
            goto fail;
        }else{
            //printf("\nbaseimage and overlay image are identical!");
            fclose(l_fp);
            free(read_backingfile_name);
            mysql_close(my_conn);
            return 1;
        }
    }else{
        printf("\n the record of base image join overlay image is empty!");
        fclose(l_fp);
        mysql_close(my_conn);
        return -1;
    }
    return -1;
fail:
    fclose(l_fp);
    mysql_close(my_conn);
    free(read_backingfile_name);
    return -1;
 }

/*
*author:liuyang
*date  :2017/3/14
*detail:根据块偏移判断是否在增量中分配
*return:int 1:inode in overlay,0:inode in baseImage,<0:error
*/
int blockInOverlay(char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits){
    printf("\n\n\n\n\n\n\n\n\nbegin blockInOverlay.......");
    FILE *l_fp;
    QCowHeader header;
    __U32_TYPE cluster_bits;
    __U32_TYPE l1_index;
    __U32_TYPE l2_index;
    __U64_TYPE l1_table_offset;
    __U32_TYPE cluster_offset;
    __U32_TYPE block_into_cluster;
    __U32_TYPE offset_into_cluster;
    __U32_TYPE l2_bits;
    __U64_TYPE l1_offset;
    __U64_TYPE l2_offset;
    __U64_TYPE l2_offset_into_cluster;
    __U64_TYPE data_offset;//data offset,Must be aligned to a cluster boundary.
    //char *read_backingfile_name;
    //printf("\n blockInOverlay fopen......");
    l_fp=fopen(qcow2Image,"r");
    //printf("\n blockInOverlay fopen comp......");
    if(l_fp==NULL){
        printf("\n in blockInOverlay,open qcow2Image failed!");
        return -1;
    }
    /***************************读取增量镜像header结构体**************************************************/
    //header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader));

    //printf("\n blockInOverlay header......");
    if(fread(&header,sizeof(struct QCowHeader),1,l_fp)<=0){
        printf("\n read qcow2header failed!");
        goto fail;
    }
    l1_table_offset=__bswap_64(header.l1_table_offset);
    cluster_bits=__bswap_32(header.cluster_bits);

    block_offset+=256;
    /*******************************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配************************************************/
    cluster_offset=block_offset/(1<<(cluster_bits-block_bits));
    block_into_cluster=block_offset%(1<<(cluster_bits-block_bits));
    offset_into_cluster=block_into_cluster<<block_bits;
    l2_bits=cluster_bits-3;
    l1_index=cluster_offset>>l2_bits;
    l2_index=cluster_offset & ((1<<l2_bits)-1);
    printf("\nin blockInOverlay block_offset%d,\ncluster offset:%d,\nl1 index:%d,\nl2 index:%x,\noffset_into_cluster%x",block_offset,cluster_offset,l1_index,l2_index<<3,offset_into_cluster);
    l1_offset=l1_table_offset+(l1_index<<3);
    if(fseek(l_fp,l1_offset,SEEK_SET)){
        printf("\n seek to l1 offset failed!");
        goto fail;
    }
    if(fread(&l2_offset,sizeof(l2_offset),1,l_fp)<=0){
        printf("\n read the l2 offset failed!");
        goto fail;
    }
    l2_offset=__bswap_64(l2_offset) & L1E_OFFSET_MASK;
    //printf("\nsize:%ld l2_offset:%x",sizeof(l2_offset),l2_offset);

    if(!l2_offset){
        printf("\n l2 table has not been allocated,the data must in backing file!");
        goto unallocated;
    }
    //printf("\n!!!!!!!!!!!!!!!seek!!!!!!!!!!!!!!l2_offset_into_cluster:%ld",l2_offset_into_cluster);
    l2_offset_into_cluster=l2_offset+(l2_index<<3);
    if(fseek(l_fp,l2_offset_into_cluster,SEEK_SET)){
        printf("\n seek to l2_offset_into_cluster failed!");
        goto fail;
    }
    //printf("\n!!!!!!!!!!!!!!!seek over!!!!!!!!!!!!!!");
    if(fread(&data_offset,sizeof(data_offset),1,l_fp)<=0){
        printf("\n read data offset failed!");
        goto fail;
    }
    data_offset=__bswap_64(data_offset);
    if(!(data_offset & L2E_OFFSET_MASK)){
        printf("\n l2 entry has not been allocated,the data must in backing file!");
        goto unallocated;
    }
    //printf("\n size:%ld,the data offset:%x",sizeof(data_offset),data_offset);

success:
    fclose(l_fp);
    printf("\nsuccess,leave blockInOverlay......\n\n\n\n\n\n");
    return 1;
fail:
    fclose(l_fp);
    printf("\nfail,leave blockInOverlay......\n\n\n\n\n\n");
    return -1;
unallocated:
    fclose(l_fp);
    printf("\nunallocated,leave blockInOverlay......\n\n\n\n\n\n");
    return 0;

}
/*
 *author:liuyang
 *date  :2017/3/16
 *detail:判断inode是否在overlay中，如果在，读取inode结构
 *return 1:inode in overlay,0:inode in baseImage,<0:error
 */
int inodeInOverlay(char *qcow2Image,unsigned int block_offset,unsigned int bytes_offset_into_block,__U16_TYPE block_bits,struct ext2_inode *inode){
    printf("\n\n\n\n\n\n\nbegin inodeInOverlay.......");
    FILE *l_fp;
    QCowHeader *header;
    __U32_TYPE cluster_bits;
    __U32_TYPE l1_index;
    __U32_TYPE l2_index;
    __U64_TYPE l1_table_offset;
    __U32_TYPE cluster_offset;
    __U32_TYPE block_into_cluster;
    __U32_TYPE offset_into_cluster;
    __U32_TYPE l2_bits;
    __U64_TYPE l1_offset;
    __U64_TYPE l2_offset;
    __U64_TYPE l2_table_offset;
    __U64_TYPE l2_entry_offset_cluster;
    __U64_TYPE data_offset;//data offset,Must be aligned to a cluster boundary.
    long int real_data_offset;
    int i;
    int count;
    char *read_backingfile_name;
    //printf("overlay image:%s",qcow2Image);
    l_fp=fopen(qcow2Image,"r");
    if(l_fp==NULL){
        printf("\n open qcow2Image failed!");
        return -1;
    }
    /***************************读取增量镜像header结构体**************************************************/
    header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader));
    if(fread(header,sizeof(struct QCowHeader),1,l_fp)<=0){
        printf("\n read qcow2header failed!");
        goto fail;
    }

    cluster_bits=__bswap_32(header->cluster_bits);
    l1_table_offset=__bswap_64(header->l1_table_offset);

    /***************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配****************************************************************/
    //the beginning 1M(256 blocks) not belongs to ext4 fs,I think.
    block_offset+=256;
    cluster_offset=block_offset/(1<<(cluster_bits-block_bits));
    block_into_cluster=block_offset%(1<<(cluster_bits-block_bits));
    offset_into_cluster=block_into_cluster<<block_bits;
    l2_bits=cluster_bits-3;
    l1_index=cluster_offset>>l2_bits;
    l2_index=cluster_offset & ((1<<l2_bits)-1);
    //printf("\nblock_offset%d, cluster offset:%d,l1 index:%d,l2 index:%x,offset_into_cluster%x",block_offset,cluster_offset,l1_index,l2_index<<3,offset_into_cluster);
    l1_offset=l1_table_offset+(l1_index<<3);
    if(fseek(l_fp,l1_offset,SEEK_SET)){
        printf("\n seek to l1 offset failed!");
        goto fail;
    }
    if(fread(&l2_table_offset,sizeof(l2_table_offset),1,l_fp)<=0){
        printf("\n read the l2 offset failed!");
        goto fail;
    }
    l2_table_offset=__bswap_64(l2_table_offset) & L1E_OFFSET_MASK;

    //printf("\nsize:%ld l2_offset:%lx",sizeof(l2_offset),l2_offset);
    /***************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配,如果数据块在cluster中分配，读取相应的inode结构体信息****************************************************************/

    if(!l2_table_offset){
        printf("\n l2 table has not been allocated,the data must in backing file!");
        free(header);
        fclose(l_fp);
        return 0;
    }
    l2_entry_offset_cluster=l2_table_offset+(l2_index<<3);
    if(fseek(l_fp,l2_entry_offset_cluster,SEEK_SET)){
        printf("\n seek to l2_entry_offset_cluster failed!");
        goto fail;
    }
    if(fread(&data_offset,sizeof(data_offset),1,l_fp)<=0){
        printf("\n read data offset failed!");
        goto fail;
    }
    data_offset=__bswap_64(data_offset);
    //printf("\n data offset:%ld",data_offset);
    data_offset=(data_offset & L2E_OFFSET_MASK);

    if(!data_offset){
        printf("\n l2 entry has not been allocated,the data must in backing file!");
        free(header);
        fclose(l_fp);
        return 0;
    }
    //printf("\n data_offset & L2E_OFFSET_MASK offset:%ld",data_offset);
    //real_data_offset=(long int)data_offset;
    //printf("\n real_data_offset offset:%ld",real_data_offset);
    //data_offset=data_offset+offset_into_cluster+bytes_offset_into_block;

    if(fseek(l_fp,0,SEEK_SET)){
        printf("\n seek to SEEK_SET failed!");
        goto fail;
    }

//    count=data_offset/(1<<cluster_bits);
//    printf("\n count:%d,cluster size:%d",count,1<<cluster_bits);
//    for(i=0;i<count;i++){
//        //printf("\n************count********:%d",i);
//        if(fseek(l_fp,1<<cluster_bits,SEEK_CUR)){
//            printf("\n seek to data_offset failed!");
//            goto fail;
//        }
//    }

    if(fseek(l_fp,data_offset,SEEK_CUR)){
        printf("\n seek to data_offset failed!");
        goto fail;
    }
    //printf("\n!!!!!!!!!!!!!!!!!!!!!!!seek to much!!!!!!!!!!!!!!!!!!!!!!!!!!!!!data_offset:%ld",data_offset);
    if(fseek(l_fp,offset_into_cluster+bytes_offset_into_block,SEEK_CUR)){
        printf("\n seek to data_offset failed!");
        goto fail;
    }
    //printf("\n!!!!!!!!!!!!!!!!!!!!!!!seek to much over!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    if(fread(inode,sizeof(struct ext2_inode),1,l_fp)<=0){
        printf("\n read inode failed!");
        goto fail;
    }
    fclose(l_fp);
    free(header);
    printf("\nleave inodeInOverlay........\n\n\n\n");
    return 1;

fail:
    fclose(l_fp);
    free(header);
    printf("\nfail,leave inodeInOverlay........\n\n\n\n");
    return -1;

}


/*
*given a inode ,to find which image the data in?
*author:liuyang
*date:2017/3/9
*param:char * base image;char * qcow2 image;int inode
*return int
*/
int which_images_by_inode(char *baseImage,char *qcow2Image,unsigned int inode,char *filepath){
    struct ext2_super_block *es;
    struct ext2_group_desc *gdesc;
    struct ext4_extent_header *eh;
    struct ext4_extent *ee;
    struct ext4_extent_idx *ext_idx;
    struct ext2_inode *e_ino;
    int i;
    int super_block=1;
    __U32_TYPE inodes_per_group;
    __U32_TYPE blocks_per_group;
    __U32_TYPE block_size,block_bits;
    __U16_TYPE inode_size;
    __U32_TYPE inode_per_group;
    __U16_TYPE reserved_gdt_blocks;//Number of reserved GDT entries for future filesystem expansion.
    __U32_TYPE inode_in_which_blockgroup;
    __U32_TYPE offset_in_bg_desc;
    __U32_TYPE inode_index_in_inodetable;
    __U32_TYPE inode_offset_in_inodetable;
    __U32_TYPE begin_block_of_inodetable;
    __U32_TYPE inode_block_offset_in_inodetable;
    __U32_TYPE inode_bytes_offset_into_inodetable;
    __U32_TYPE inode_block_number;
    __U32_TYPE data_block_offset;
    __U32_TYPE index_block_offset;
    FILE *bi_fp;
    int inode_status;
    int block_status;
    //printf("\nbaseImage:%s",baseImage);
    bi_fp=fopen(baseImage,"r");
    if(bi_fp==NULL){
        printf("\n error:open baseImage failed!");
        return -1;
    }
    es=(struct ext2_super_block*)malloc(sizeof(struct ext2_super_block));
    gdesc=(struct ext2_group_desc*)malloc(sizeof(struct ext2_group_desc));
    e_ino=(struct ext2_inode*)malloc(sizeof(struct ext2_inode));

    //printf("\nopen baseImage successfully!,%ld",sizeof(struct ext2_super_block));
    /***************************************************读取超级快元信息**************************************************/
   //offset 1M to begin
    //fseek(bi_fp,Meg,SEEK_SET);
    //offset 1k to cur
    if(fseek(bi_fp,1049600,SEEK_CUR)){
        printf("\n seek to super block failed!");
        goto fail;
    }
    /*
    *read the super block info
    *fread return the number of reading,respected 1
    *must malloc zone for pointer es,es should point to a place it knows.
    *as we know no matter where the file in,the super block never change
    */
    if(fread(es,sizeof(struct ext2_super_block),super_block,bi_fp)<=0){
        printf("\n read to super block failed!");
        goto fail;
    }
    block_bits=10+es->s_log_block_size;//12
    block_size=1<<block_bits;//4096
    inode_per_group=es->s_inodes_per_group;
    inode_size=es->s_inode_size;
    /********计算该inode所在块组，从块组描述符中读取该块组inodetable的的起始块号，而这些信息在块组描述符表中不变*******/
    /*
     *1.calculate which bg desc entry the inode in
     *2.get the beginning block number of inodetable the inode in from desc entry
     *3.calculate the exactly block number the inode in
     */
    inode_in_which_blockgroup=(inode-1)/inode_per_group;
    //the offest into the bg desc
    offset_in_bg_desc=inode_in_which_blockgroup*BG_DESC_SIZE;
    /*
     *as we know no matter where the file in,the desc table never change
     *test inode:133415 16
     */
    //TODO
    //fseek(bi_fp,Meg,SEEK_SET);fseek(bi_fp,block_size,SEEK_CUR);//1052672
    fseek(bi_fp,1052672,SEEK_SET);
    fseek(bi_fp,offset_in_bg_desc,SEEK_CUR);

    if(fread(gdesc,sizeof(struct ext2_group_desc),1,bi_fp)<=0){
        printf("\nread group descriptor entry failed!");
        goto fail;
    }
    begin_block_of_inodetable=gdesc->bg_inode_table;
    /***************************由inodetable的起始块号,计算inode真正偏移块号和块内偏移***************************/
    //index of inodetable 3878
    inode_index_in_inodetable=(inode-1)%inode_per_group;
    //offset into inodetable 992768
    inode_offset_in_inodetable=inode_index_in_inodetable*inode_size;

    inode_block_offset_in_inodetable=inode_offset_in_inodetable/block_size;
    inode_bytes_offset_into_inodetable=inode_offset_in_inodetable%block_size;

    //the real block number of inode,
    inode_block_number=begin_block_of_inodetable+inode_block_offset_in_inodetable;
//    printf("\n inode block offest in inodetable:%d,\n the real block number of inode%d,\n inode_bytes_offset_into_inodetable%x",
//               inode_block_offset_in_inodetable,inode_block_number,inode_bytes_offset_into_inodetable);

    inode_status=inodeInOverlay(qcow2Image,inode_block_number,inode_bytes_offset_into_inodetable,block_bits,e_ino);
    if(inode_status==1){
        //inode in the overlay
        printf("\n *****************inode in overlay,read inode from overlay successful!!!*****************");
        inode_in_overlay_file_count++;
        eh=(struct ext4_extent_header *)((char *) &(e_ino->i_block[0]));
        //printf("\n eh->magic:%x",eh->eh_magic);
        if(eh->eh_magic==0xf30a){
            if(eh->eh_depth==0 && eh->eh_entries>0){
                //it is leaf node
                ee=EXT_FIRST_EXTENT(eh);
                data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
                //695379
                //printf("\n ee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));
                block_status=blockInOverlay(qcow2Image,data_block_offset,block_bits);
                if(block_status==0){
                    //printf("\n *********************data blocks in baseImage!!!******************");
                }else if(block_status==1){
                    //overlay_filepath[overlay_file_count]=malloc(strlen(filepath));
                    strcpy(overlay_filepath[overlay_file_count],filepath);
                    overlay_file_count++;
                    //printf("\n *********************data blocks in overlay!!!******************");
                }else{
                    printf("\n *********************blockInOverlay fail*********************");
                    blockInOverlay_error;
                    goto fail;
                }
            }else if(eh->eh_depth>0 && eh->eh_entries>0){
                //it is index node,

                ext_idx = EXT_FIRST_INDEX(eh);
                index_block_offset=(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo;
                //printf("\n.......there is a ext_idx node block:%x",index_block_offset);
                block_status=blockInOverlay(qcow2Image,index_block_offset,block_bits);
                if(block_status==0){
                    //printf("\n *********************index block in base,so data blocks in baseImage !!!******************");
                }else if(block_status==1){
                    //overlay_filepath[overlay_file_count]=malloc(strlen(filepath));
                    strcpy(overlay_filepath[overlay_file_count],filepath);
                    overlay_file_count++;
                    //printf("\n *********************index block in overlay,so data blocks in overlay!!!******************");
                }else{
                    printf("\n *********************blockInOverlay fail*********************");
                    blockInOverlay_error++;
                    goto fail;
                }

            }
        }else{
            //printf("\n corrupt data in inode block!");
            magic_error++;
            goto fail;
        }
    }else if(inode_status==0){// inode in baseImage,so the datablock must be in baseImage
            /*
            *into the head of second block,read the blockgroup desc  entry,
            *and get the number of first block of the inodetable
            */
            printf("\n ***************inode in baseImage,read inode from baseImage successful,so data must be in baseImage!!!**************");

//            fseek(bi_fp,Meg,SEEK_SET);
//            //seek to the inodetable block,254320,for in case of overflow.
//            for(i=0;i<begin_block_of_inodetable;i++){
//                fseek(bi_fp,block_size,SEEK_CUR);
//            }
//            //offset into the inodetable
//            fseek(bi_fp,inode_offset_in_inodetable,SEEK_CUR);
//            //read the inode entry,only 128 bytes for ext2,but 256 for ext4,read 128 bytes is compatibility
//            //e_ino=(struct ext2_inode*)malloc(sizeof(struct ext2_inode));
//            fread(e_ino,sizeof(struct ext2_inode),1,bi_fp);
//            printf("\n the mode of this file:%x",e_ino->i_mode);
//            /*
//             *for ext4,use extent tree instead of block pointer
//             */
//            eh=(struct ext4_extent_header *)((char *) &(e_ino->i_block[0]));
//            printf("\n eh->magic:%x",eh->eh_magic);
//            if(eh->eh_magic==0xf30a){
//                if(eh->eh_depth==0 && eh->eh_entries>0){
//                    //it is leaf node
//                    ee=EXT_FIRST_EXTENT(eh);
//                    data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
//                    printf("\n ee block number:%x",data_block_offset);
//                    //695379
//                    printf("\n ee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));
//
//
//                }else if(eh->eh_depth>0 && eh->eh_entries>0){
//                    //it is index node,
//                    ext_idx = EXT_FIRST_INDEX(eh);
//                    printf("\n ext_idx inode block:%x",(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo);
//                }
//            }//
    }else{
        printf("\n inodeInOverlay error!");
        inodeInOverlay_error++;
        goto fail;
    }

    free(e_ino);
    free(gdesc);
    free(es);
    fclose(bi_fp);
    return 1;
fail:
    free(e_ino);
    free(gdesc);
    free(es);
    fclose(bi_fp);
    read_error++;
    //error_file_count++;//
    printf("\nerror_file_count:%.0f",error_file_count);
    return -1;
}


int test(char *baseImage,char *qcow2Image,unsigned int inode){
    struct ext2_super_block *es;
    struct ext2_group_desc *gdesc;
    struct ext4_extent_header *eh;
    struct ext4_extent *ee;
    struct ext4_extent_idx *ext_idx;
    struct ext2_inode *e_ino;
    int i;
    int super_block=1;
    __U32_TYPE inodes_per_group;
    __U32_TYPE blocks_per_group;
    __U32_TYPE block_size;
    __U16_TYPE inode_size;
    __U32_TYPE inode_per_group;
    __U16_TYPE reserved_gdt_blocks;//Number of reserved GDT entries for future filesystem expansion.
    __U32_TYPE inode_in_which_blockgroup;
    __U32_TYPE offset_in_bg_desc;
    __U32_TYPE inode_index_in_inodetable;
    __U32_TYPE inode_offset_in_inodetable;
    __U32_TYPE begin_block_of_inodetable;
    FILE *fp;
    //printf("\nbaseImage:%s",baseImage);
    fp=fopen(baseImage,"r");
    if(fp==NULL){
        printf("\n error:open failed!");
        return -1;
    }
    printf("\nopen baseImage successfully!,%ld",sizeof(struct ext2_super_block));
   //offset 1M to begin
    fseek(fp,Meg,SEEK_SET);
    //offset 1k to cur

    if(fseek(fp,Kilo,SEEK_CUR)){
        printf("\n seek to super block failed!");
        return -2;
    }
//    //printf("\n#############################");
//    //printf("\n***************************");
//    printf("\ncurrent position:%x",ftell(fp));
//
    /*
    *fread return the number of reading,respected 1
    *must apply zone for pointer es,es should point to a place it knows.
    */
    es=(struct ext2_super_block*)malloc(sizeof(struct ext2_super_block));
    if(fread(es,sizeof(struct ext2_super_block),super_block,fp)<=0){
        printf("\nread to super block failed!");
        return -3;
    }
    block_size=1<<(10+es->s_log_block_size);
    inode_per_group=es->s_inodes_per_group;
    inode_size=es->s_inode_size;
    //inode:133415 16
    inode_in_which_blockgroup=(inode-1)/inode_per_group;
    //the offest into the bg desc
    offset_in_bg_desc=inode_in_which_blockgroup*BG_DESC_SIZE;
    //index of inodetable 3878
    inode_index_in_inodetable=(inode-1)%inode_per_group;
    //offset into inodetable 992768
    inode_offset_in_inodetable=inode_index_in_inodetable*inode_size;
    printf("\n the free inodes:%d,blocksize:%d,inodesize:%d,the index in inodetable:%d,sizeof(struct ext2_inode):%ld",
           es->s_inodes_count,block_size,es->s_inode_size,inode_index_in_inodetable,sizeof(struct ext2_inode));
    /*
    *into the head of second block,read the blockgroup desc  entry,
    *and get the number of first block of the inodetable
    */
    fseek(fp,Meg,SEEK_SET);fseek(fp,block_size,SEEK_CUR);//not compatibility,do that just for ext4
    fseek(fp,offset_in_bg_desc,SEEK_CUR);
    gdesc=(struct ext2_group_desc*)malloc(sizeof(struct ext2_group_desc));
    if(fread(gdesc,sizeof(struct ext2_group_desc),1,fp)<=0){
        printf("\nread group descriptor entry failed!");
        return -4;
    }
    begin_block_of_inodetable=gdesc->bg_inode_table;
    printf("\n first block of inodetable:%d",begin_block_of_inodetable);          //524320
    /*
    *seek to where the first block of the inodetable in
    */
    fseek(fp,Meg,SEEK_SET);
    //seek to the inodetable block,254320,for in case of overflow.
    for(i=0;i<begin_block_of_inodetable;i++){
        fseek(fp,block_size,SEEK_CUR);
    }
    //offset into the inodetable
    fseek(fp,inode_offset_in_inodetable,SEEK_CUR);
    //read the inode entry,only 128 bytes for ext2,but 256 for ext4,read 128 bytes is compatibility
    e_ino=(struct ext2_inode*)malloc(sizeof(struct ext2_inode));
    fread(e_ino,sizeof(struct ext2_inode),1,fp);
    printf("\n the mode of this file:%x",e_ino->i_mode);
    /*
     *for ext4,use extent tree instead of block pointer
     */
    eh=(struct ext4_extent_header *)((char *) &(e_ino->i_block[0]));
    printf("\n eh->magic:%x",eh->eh_magic);
    if(eh->eh_magic==0xf30a){
        if(eh->eh_depth==0){
            //it is leaf node
            ee=EXT_FIRST_EXTENT(eh);
            printf("\n ee block number:%x",(ee->ee_start_hi<<32)+ee->ee_start_lo);
        }else{
            //it is index node,
            ext_idx = EXT_FIRST_INDEX(eh);
            printf("\n ext_idx leaf node block:%x",(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo);
        }
    }//

    fclose(fp);
    return 0;
}
