
//#include "inode.h"
//#include <mysql/mysql.h>

#include "ext2_main.h"

/*
 *author:liuyang
 *date  :2017/3/23
 *detail:由inode判断文件的位置,更新文件位置
 *return
 */
int ext2_update_file_metadata(char *overlay_image_path,char base_image_path[],__U64_TYPE inodes[],int inode_count,char *overlay_id){
    printf("\n\n\n\n\n\n\n\nbegin ext2_update_file_metadata......%ld",inodes[0]);
    struct ext2_super_block *es;
    struct ext2_group_desc *gdesc;
    struct ext4_extent_header *eh;
    struct ext4_extent *ee;
    struct ext4_extent_idx *ext_idx;
    struct ext2_inode *e_ino,*base_ino;
    int i,j;
    int block_status;
    //int super_block=1;
    __U32_TYPE block_size,block_bits;
    __U16_TYPE inode_size;
    __U32_TYPE inode_per_group;
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
        printf("\nerror:open base image failed!");
        return -1;
    }
    //printf("\nopen baseImage successfully!,%ld",sizeof(struct ext2_super_block));
    /***************************************************读取超级快元信息**************************************************/
   //offset 1M to begin
    fseek(bi_fp,Meg,SEEK_SET);
    //offset 1k to cur
    if(fseek(bi_fp,Kilo,SEEK_CUR)){
        printf("\nseek to super block failed!");
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

    if(fread(es,sizeof(struct ext2_super_block),1,bi_fp)<=0){
        printf("\nread to super block failed!");
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
        inode_in_which_blockgroup=(inodes[i]-1)/inode_per_group;
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
        inode_index_in_inodetable=(inodes[i]-1)%inode_per_group;
        //offset into inodetable 992768
        inode_offset_in_inodetable[i]=inode_index_in_inodetable*inode_size;

        inode_block_offset_in_inodetable=inode_offset_in_inodetable[i]/block_size;
        inode_bytes_offset_into_inodetable[i]=inode_offset_in_inodetable[i]%block_size;

        //the real block number of inode,
        inode_block_number[i]=begin_block_of_inodetable[i]+inode_block_offset_in_inodetable;
        //printf("\n inode block offest in inodetable:%d,\n the real block number of inode%d,\n inode_bytes_offset_into_inodetable%x",inode_block_offset_in_inodetable,inode_block_number[i],inode_bytes_offset_into_inodetable[i]);
    }
    //判断文件元信息位置
    int inode_status=ext2_inodes_in_overlay(base_image_path,overlay_image_path,inode_block_number,inode_bytes_offset_into_inodetable,block_bits,e_ino,inode_count);
    if(inode_status<0){
        printf("\n error:inode_in_overlay() fail!");
        goto fail;
    }
    for(i=0;i<inode_count;i++){
        printf("\nread inode number:%ld,file size:%d",inodes[i],e_ino[i].i_size);
        /**暂时用mode为0表示inode不在overlay中*/
        if(e_ino[i].i_mode!=0){
            //printf("\n *******inode in overlay,read inode from overlay successful!!!*******");
            printf("\n文件元信息在增量镜像中，需要进一步根据文件数据块位置判断文件是否篡改........");
            eh=(struct ext4_extent_header *)((char *) &(e_ino[i].i_block[0]));
            printf("\nextent header magic:%x",eh->eh_magic);
            if(eh->eh_magic==0xf30a){
                if(eh->eh_depth==0 && eh->eh_entries>0){
                    //it is leaf node
                    ee=EXT_FIRST_EXTENT(eh);
                    data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
                    //695379
                    printf("\nee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));
                    /**由块偏移判断数据位置*/
                    block_status=ext2_blockInOverlay(overlay_image_path,data_block_offset,block_bits);
                    if(block_status==1){
                        //printf("\n *********************data blocks in overlay!!!******************");
                        printf("\n *********************文件内容在增量镜像中，需要进行特征值比较!!!******************");
                        //inodePosition,dataPosition
                        sql_update_file_metadata(overlay_id,inodes[i],2,2);
                    }else if(block_status==0){
                        //printf("\n *********************data blocks in baseImage!!!******************");
                        printf("\n *********************文件内容在原始镜像中，文件未被修改,无需特征值比较！！！******************");

                        sql_update_file_metadata(overlay_id,inodes[i],2,1);
                    }else{
                        printf("\n *********************ext2_blockInOverlay fail*********************");
                        goto fail;
                    }
                }else if(eh->eh_depth>0 && eh->eh_entries>0){
                    /**
                     *索引节点块的位置应该和数据块位置一致
                     */
                    printf("\nblock offset in the index block!!!");
                    //it is index node,
                    ext_idx = EXT_FIRST_INDEX(eh);
                    index_block_offset=(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo;
                    block_status=ext2_blockInOverlay(overlay_image_path,index_block_offset,block_bits);
                    if(block_status==1){
                        //printf("\n *********************data blocks in overlay!!!******************");
                        printf("\n *********************文件内容在增量镜像中，需要进行特征值比较!!!******************");
                        //inodePosition,dataPosition
                        //TODO need backup file in host
                        sql_update_file_metadata(overlay_id,inodes[i],2,2);
                    }else if(block_status==0){
                        //printf("\n *********************data blocks in baseImage!!!******************");
                        printf("\n *********************文件内容在原始镜像中，文件未被修改,无需特征值比较！！！******************");

                        sql_update_file_metadata(overlay_id,inodes[i],2,1);
                    }else{
                        printf("\n *********************ext2_blockInOverlay fail*********************");
                        goto fail;
                    }
                    printf("\next_idx node block:%x",index_block_offset);
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
            printf("\nthe mode of this file:%x",base_ino->i_mode);


            sql_update_file_metadata(overlay_id,inodes[i],1,1);
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
    printf("\n leave ext2_update_file_metadata.......\n\n\n\n\n\n");
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
    printf("\n leave ext2_update_file_metadata.......failed!\n\n\n\n\n\n");
    return 0;
}
/*
 *author:liuyang
 *date  :2017/3/23
 *detail:由inodes偏移位置判断inodes在哪个镜像中
 *return
 */
int ext2_inodes_in_overlay(char *baseImage,char *qcow2Image,__U32_TYPE *block_offset,__U32_TYPE *bytes_offset_into_block,__U16_TYPE BLOCK_BITS,struct ext2_inode *inodes,int inode_count){
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
    printf("\n\n\n\n\n\n\nbegin inode_in_overlay.........");
    l_fp=fopen(qcow2Image,"r");
    if(l_fp==NULL){
        printf("\nopen qcow2Image failed!");
        return -1;
    }
    /***************************读取增量镜像header结构体**************************************************/
    header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader));
    if(fread(header,sizeof(struct QCowHeader),1,l_fp)<=0){
        printf("\nread qcow2header failed!");
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
    /***************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配****************************************************************/
    //the beginning 1M(256 blocks) not belongs to ext4 fs,I think.
    for(i=0;i<inode_count;i++){
        block_offset[i]+=256;
        cluster_offset=block_offset[i]/(1<<(CLUSTER_BITS-BLOCK_BITS));      /**cluster偏移块数*/
        block_into_cluster=block_offset[i]%(1<<(CLUSTER_BITS-BLOCK_BITS));  /**cluster内块偏移数*/
        blocks_bytes_into_cluster=block_into_cluster<<BLOCK_BITS;           /**cluster块偏移的字节数*/
        l1_index=cluster_offset>>L2_BITS;                                   /**l1表的偏移项数*/
        l2_index=cluster_offset & ((1<<L2_BITS)-1);                         /**l2表的偏移项数*/
        printf("\nblock_offset%d,\ncluster offset:%d,\nl1 index:%d,l2 index:%x,\nblocks_bytes_into_cluster%x",
                        block_offset[i],cluster_offset,l1_index,l2_index,blocks_bytes_into_cluster);
        l1_entry_offset=L1_TABLE_OFFSET+(l1_index<<3);
        if(fseek(l_fp,l1_entry_offset,SEEK_SET)){
            printf("\nseek to l1 offset failed!");
            continue;
        }
        if(fread(&l2_table_offset,sizeof(l2_table_offset),1,l_fp)<=0){
            printf("\nread the l2 offset failed!");
            continue;
        }
        l2_table_offset=__bswap_64(l2_table_offset) & L1E_OFFSET_MASK;
        printf("\nsize:%ld l2_table_offset:%lx",sizeof(l2_table_offset),l2_table_offset);
        /***************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配,如果数据块在cluster中分配，读取相应的inode结构体信息****************************************************************/
        if(!l2_table_offset){
            printf("\nl2 table has not been allocated,the data must in backing file!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        l2_entry_offset=l2_table_offset+(l2_index<<3);
        if(fseek(l_fp,l2_entry_offset,SEEK_SET)){
            printf("\nseek to l2_entry_offset failed!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        if(fread(&data_offset,sizeof(data_offset),1,l_fp)<=0){
            printf("\nread data offset failed!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        data_offset=__bswap_64(data_offset);
        data_offset=data_offset&L2E_OFFSET_MASK;
        if(!(data_offset & L2E_OFFSET_MASK)){
            printf("\n l2 entry has not been allocated,the data must in backing file!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        printf("\nsize:%ld,the data offset:%x",sizeof(data_offset),data_offset);

        if(fseek(l_fp,0,SEEK_SET)){
            printf("\nseek to SEEK_SET failed!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
//        count=data_offset/(1<<CLUSTER_BITS);    /**TODO偏移以64k为单位，一定能整除*/
//        printf("\ncount:%d,cluster size:%d",count,1<<CLUSTER_BITS);
//        for(j=0;j<count;j++){
//            //printf("\n************count********:%d",i);
//            if(fseek(l_fp,1<<CLUSTER_BITS,SEEK_CUR)){
//                printf("\n seek to data_offset failed!");
//                break;
//            }
//        }
        /**TODO type not consistency*/
        printf("\nthe unsigned long int data_offset:%ld,%ld",data_offset,(long int)data_offset);
        if(fseek(l_fp,data_offset,SEEK_CUR)){
            printf("\nseek to unsigned long int data_offset failed!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        if(fseek(l_fp,blocks_bytes_into_cluster+bytes_offset_into_block[i],SEEK_CUR)){
            printf("\nseek to data_offset failed!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }

        if(fread(&inodes[i],sizeof(struct ext2_inode),1,l_fp)<=0){
            printf("\nread inode failed!");
            inodes[i].i_mode=0;
            inodes[i].i_block[0]=0;
            continue;
        }
        //printf("\nread inode mode:%d",inodes[i].i_mode);
    }

    //free(read_backingfile_name);

    printf("\nleave inode_in_overlay......\n\n\n\n\n\n\n");
    fclose(l_fp);
    free(header);
    return 1;
}
/*
 *author:liuyang
 *date  :2017/3/22
 *detail:判断增量镜像中原始镜像路径和给出名称是否一致
 *return 1:true,0:false
 */
 int is_base_image_identical(char *overlay_image_id,char base_image_path[]){
    printf("\n\n\n\n\n\n\n\nis_base_image_identical....");
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
    if(!mysql_real_connect(my_conn,"127.0.0.1","root","","lqs",0,NULL,0)){
        printf("\nConnect Error!");
        return -1;
    }
    //printf("\n in is base image identical overlay_image_id:%s",overlay_image_id);

    sprintf(strsql,"select baseImages.name,overlays.absPath,baseImages.absPath    \
            from overlays join baseImages                   \
            where overlays.id=%s and overlays.baseImageId=baseImages.id",overlay_image_id);
    if(mysql_query(my_conn,strsql)){
        printf("\n query baseImages and overlays failed!baseImageId:%s",overlay_image_id);
        mysql_close(my_conn);
        return -1;
    }
    res=mysql_use_result(my_conn);
    row=mysql_fetch_row(res);
    if(row!=NULL){
        base_image_name=row[2];
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
            goto fail;
        }
        header.backing_file_offset=__bswap_64(header.backing_file_offset);
        header.backing_file_size=__bswap_32(header.backing_file_size);
        /**************************提取原始镜像名，检查是否一致************************************************/
        //printf("\n the qcow2 version%d",header.version);
        fseek(l_fp,header.backing_file_offset,SEEK_SET);
        read_backingfile_name=(char *)malloc(header.backing_file_size+1);
        if(fgets(read_backingfile_name,header.backing_file_size+1,l_fp)==NULL){
            printf("\n read backingfile name error!");
            goto fail0;
        }
        //printf("\n size:%d,backing file name:%s,string len %ld",header.backing_file_size,read_backingfile_name,sizeof(read_backingfile_name));
        //strstr judge whether str1 contains str2
        if(strstr(base_image_name,read_backingfile_name)==NULL){
            //printf("\n wrong overlay images!");
            goto fail0;
        }else{
            //printf("\nbaseimage and overlay image are identical!");
            goto back;
        }
    }else{
        printf("\n the record of base image join overlay image is empty!");
        goto fail;
    }
back:
    fclose(l_fp);
    mysql_close(my_conn);
    free(read_backingfile_name);
    return 1;
fail0:
    free(read_backingfile_name);
fail:
    fclose(l_fp);
    mysql_close(my_conn);
    return -1;
 }

/*
*author:liuyang
*date  :2017/3/14
*detail:根据块偏移判断是否在增量中分配
*return:int 1:inode in overlay,0:inode in baseImage,<0:error
*/
int ext2_blockInOverlay(char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits){
    printf("\n\n\n\n\n\n\n\n\nbegin ext2_blockInOverlay.......");
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
    //printf("\n ext2_blockInOverlay fopen......");
    l_fp=fopen(qcow2Image,"r");
    //printf("\n ext2_blockInOverlay fopen comp......");
    if(l_fp==NULL){
        printf("\n in ext2_blockInOverlay,open qcow2Image failed!");
        return -1;
    }
    /***************************读取增量镜像header结构体**************************************************/
    //header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader));

    //printf("\n ext2_blockInOverlay header......");
    if(fread(&header,sizeof(struct QCowHeader),1,l_fp)<=0){
        printf("\n read qcow2header failed!");
        goto fail;
    }
    l1_table_offset=__bswap_64(header.l1_table_offset);
    cluster_bits=__bswap_32(header.cluster_bits);
    printf("\ndata block offset:%d",block_offset);
    block_offset+=256;

    /*******************************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配************************************************/
    cluster_offset=block_offset/(1<<(cluster_bits-block_bits));
    block_into_cluster=block_offset%(1<<(cluster_bits-block_bits));
    offset_into_cluster=block_into_cluster<<block_bits;
    l2_bits=cluster_bits-3;
    l1_index=cluster_offset>>l2_bits;
    l2_index=cluster_offset & ((1<<l2_bits)-1);
    printf("\nin ext2_blockInOverlay block_offset%d,\ncluster offset:%d,\nl1 index:%d,\nl2 index:%x,\noffset_into_cluster%x",block_offset,cluster_offset,l1_index,l2_index<<3,offset_into_cluster);
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
    printf("\ndata block:l1_table_offset:%x",l2_offset);
    printf("\ndata block:l2_table_offset:%x",data_offset&L2E_OFFSET_MASK);
    if(!(data_offset & L2E_OFFSET_MASK)){
        printf("\n l2 entry has not been allocated,the data must in backing file!");
        goto unallocated;
    }
    //printf("\n size:%ld,the data offset:%x",sizeof(data_offset),data_offset);

success:
    fclose(l_fp);
    printf("\nsuccess,leave ext2_blockInOverlay......\n\n\n\n\n\n");
    return 1;
fail:
    fclose(l_fp);
    printf("\nfail,leave ext2_blockInOverlay......\n\n\n\n\n\n");
    return -1;
unallocated:
    fclose(l_fp);
    printf("\nunallocated,leave ext2_blockInOverlay......\n\n\n\n\n\n");
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
    data_offset=(data_offset & L2E_OFFSET_MASK);

    printf("\ninode:l1_table_offset:%x",l2_table_offset);
    printf("\ninode:l2_table_offset:%x",data_offset);
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

    if(fseek(l_fp,data_offset,SEEK_CUR)){
        printf("\n seek to data_offset failed!");
        goto fail;
    }
    //printf("\n!!!!!!!!!!!!!!!!!!!!!!!seek to much!!!!!!!!!!!!!!!!!!!!!!!!!!!!!data_offset:%ld",data_offset);
    printf("\n:bytes_into_cluster%d",offset_into_cluster+bytes_offset_into_block);
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
    //__U32_TYPE inodes_per_group;
    //__U32_TYPE blocks_per_group;
    __U32_TYPE block_size,block_bits;
    __U16_TYPE inode_size;
    __U32_TYPE inode_per_group;
    //__U16_TYPE reserved_gdt_blocks;//Number of reserved GDT entries for future filesystem expansion.
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
    if(fseek(bi_fp,EXT2_SUPERBLOCK_OFFSET,SEEK_CUR)){
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
    printf("\ninode table offset:%x",begin_block_of_inodetable);
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
        printf("\n eh->magic:%x",eh->eh_magic);
        if(eh->eh_magic==0xf30a){
            printf("\neh_depth:%d, eh->eh_entries:%d,size:%d",eh->eh_depth,eh->eh_entries,e_ino->i_size);
            if(eh->eh_depth==0 && eh->eh_entries>0){
                //it is leaf node
                ee=EXT_FIRST_EXTENT(eh);
                data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
                //695379
                printf("\n ee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));

                block_status=ext2_blockInOverlay(qcow2Image,data_block_offset,block_bits);
                if(block_status==0){
                    //printf("\n *********************data blocks in baseImage!!!******************");
                }else if(block_status==1){
                    //overlay_filepath[overlay_file_count]=malloc(strlen(filepath)+1);
                    strcpy(overlay_filepath[overlay_file_count],filepath);
                    overlay_file_count++;
                    //printf("\n *********************data blocks in overlay!!!******************");
                }else{
                    printf("\n *********************ext2_blockInOverlay fail*********************");
                    blockInOverlay_error++;
                    goto fail;
                }
            }else if(eh->eh_depth>0 && eh->eh_entries>0){
                //it is index node,

                ext_idx = EXT_FIRST_INDEX(eh);
                index_block_offset=(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo;
                printf("\n.......there is a ext_idx node block:%x",index_block_offset);
                block_status=ext2_blockInOverlay(qcow2Image,index_block_offset,block_bits);
                if(block_status==0){
                    //printf("\n *********************index block in base,so data blocks in baseImage !!!******************");
                }else if(block_status==1){
                    //overlay_filepath[overlay_file_count]=malloc(strlen(filepath)+1);
                    strcpy(overlay_filepath[overlay_file_count],filepath);
                    overlay_file_count++;
                    //printf("\n *********************index block in overlay,so data blocks in overlay!!!******************");
                }else{
                    printf("\n *********************ext2_blockInOverlay fail*********************");
                    blockInOverlay_error++;
                    goto fail;
                }

            }else{
                printf("\nsomething error!");
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

/*
 *author:liuyang
 *date  :2017/5/9
 *detail:cal overlay vm file md5
 *return void
 */
int ext2_overlay_md5(char *baseImage,char *overlay){
    /**ext2文件系统相关数据结构*/
    struct ext2_super_block *es;
    struct ext2_group_desc *group_desc_table;
    struct ext4_extent_header *eh;
    struct ext4_extent *ee;
    struct ext4_extent_idx *ext_idx;
    struct ext2_inode *e_ino;
    __U32_TYPE block_size,block_bits;
    __U16_TYPE inode_size;
    __U32_TYPE inodes_per_group;
    /**ext2文件inode相关数据结构*/
    __U32_TYPE index_in_gdesc_table;
    __U32_TYPE inodetable_block_offset,inode_bytes_into_inodetable,inode_blocks_into_inodetable,inode_bytes_into_block_of_inodetable;
    __U32_TYPE inode_blocks_offset;
    /**增量镜像相关数据结构*/
    QCowHeader *header;
    __U32_TYPE cluster_bits;
    __U32_TYPE l2_bits;
    __U32_TYPE l1_size;
    __U32_TYPE cluster_offset;
    __U32_TYPE bytes_into_cluster;
    __U64_TYPE l1_table_offset;
    __U64_TYPE *l1_table;
    __U64_TYPE **l2_tables;
    __U32_TYPE l1_index;
    __U32_TYPE l2_index;
    __U64_TYPE l1_table_entry;
    __U64_TYPE l2_table_entry;
    __U32_TYPE CLUSTER_BYTES;
    /**文件相关*/
    __U32_TYPE data_block_offset;
    __U32_TYPE index_block_offset;
    __U64_TYPE inode_number;
    char *md5str;
    int i=0;

//    float all_file_count;
    __U32_TYPE ext_error_file_count=0;
    __U32_TYPE ext_all_file_count=0;
    __U32_TYPE overlay_file_number=0;
    __U32_TYPE inode_in_overlay_file_number=0;

    FILE *bi_fp,*o_fp;
    char file_name[256]={NULL};
    struct guestfs_statns *gs1;
    char **all_file_path;
    printf("\nbegin to guestfs......");

    guestfs_h *g=guestfs_create ();
    guestfs_add_drive(g,overlay);
    guestfs_launch(g);
    guestfs_mount(g,"/dev/sda1","/");
    printf("\nbegin to get all file......");
    all_file_path=guestfs_find(g,"/");
    /***************************************************初始化文件系统信息**************************************************/
    bi_fp=fopen(baseImage,"r");
    if(bi_fp==NULL){
        printf("\nerror:open baseImage failed!");
        goto error;
    }
    //printf("\n***********************");
    //偏移到超级块
    if(fseek(bi_fp,EXT2_SUPERBLOCK_OFFSET,SEEK_SET)){
        printf("\nseek to super block failed!");
        goto fail;
    }
    /***************/
    es=(struct ext2_super_block*)malloc(sizeof(struct ext2_super_block));
    if(es==NULL){
        printf("\nallocate l1 table failed!");
        goto fail_es;
    }
    e_ino=(struct ext2_inode*)malloc(sizeof(struct ext2_inode));
    if(e_ino==NULL){
        printf("\nallocate e_ino failed!");
        goto fail_inode;
    }
    group_desc_table=malloc(block_size);
    if(group_desc_table==NULL){
        printf("\nallocate group_desc_table failed!");
        goto fail_group_desc_table;
    }
    if(fread(es,sizeof(struct ext2_super_block),1,bi_fp)<=0){
        printf("\n read to super block failed!");
        goto fail1;
    }
    block_bits=10+es->s_log_block_size;//12
    block_size=1<<block_bits;//4096
    inodes_per_group=es->s_inodes_per_group;
    inode_size=es->s_inode_size;


    /**偏移到块组描述符表*/
    printf("\n_ext2_group_desc_offset(block_size):%d",_ext2_group_desc_offset(block_size));
    fseek(bi_fp,_ext2_group_desc_offset(block_size),SEEK_SET);
    if(fread(group_desc_table,block_size,1,bi_fp)<=0){
        printf("\nread group descriptor entry failed!");
        goto fail1;
    }
    /***************************************************初始化增量镜像信息**************************************************/
    o_fp=fopen(overlay,"r");
    if(o_fp==NULL){
        printf("\n open overlay image failed!");
        goto fail1;
    }
    header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader));
    if(header==NULL){
        printf("\nallocate qcowheader failed!");
        goto fail_header;
    }
    if(fread(header,sizeof(struct QCowHeader),1,o_fp)<=0){
        printf("\n read qcow2 header failed!");
        goto fail_read_header;
    }
    cluster_bits=__bswap_32(header->cluster_bits);
    CLUSTER_BYTES=1<<cluster_bits;
    l1_table_offset=__bswap_64(header->l1_table_offset);
    l2_bits=cluster_bits-3;

    l1_size=__bswap_32(header->l1_size);
    //printf("\n***********************l1_size:%d",l1_size);
    /**
     *将l1,l2表读到内存中，加快速度
     */
    l1_table=malloc(l1_size*sizeof(__U64_TYPE));
    l2_tables=malloc(l1_size*sizeof(__U64_TYPE));
    if(l1_table==NULL){
        printf("\nallocate l1 table failed!");
        goto fail_l1_table;
    }
    if(l2_tables==NULL){
        printf("\nallocate l2 tables failed!");
        goto fail_l2_tables;
    }
    for(i=0;i<l1_size;i++){
        l2_tables[i]=malloc((1<<l2_bits)*sizeof(__U64_TYPE));
        if(l2_tables[i]==NULL){
            printf("\nmalloc l2 tables failed!");
            goto fail2;
        }
        printf("\nmalloc the %d for l2_table!",i);
    }

    if(fseek(o_fp,l1_table_offset,SEEK_SET)){
        printf("\nseek to l1 table failed!");
        goto fail2;
    }
    if(fread(l1_table,sizeof(__U64_TYPE),l1_size,o_fp)<=0){
        printf("\n read the l1 table failed!");
        goto fail2;
    }
    /**
     *l2表读到内存中，加快速度
     */

    printf("\nl1 table size:%d",l1_size);
    for(i=0;i<l1_size;i++){
        l1_table[i]=__bswap_64(l1_table[i]) & L1E_OFFSET_MASK;
        if(!l1_table[i]){
            printf("\n%dthe l2 table not allocate!",i);
            //free(l2_tables[i]);
            //l2_tables[i]=NULL;
        }else{
            if(fseek(o_fp,l1_table[i],SEEK_SET)){
                printf("\n%x:seek to l2 table failed!",l1_table[i]);
                goto fail2;
            }
            if(fread(l2_tables[i],sizeof(__U64_TYPE),1<<l2_bits,o_fp)<=0){
                printf("\n%x:read the l2 table failed!",l1_table[i]);
                goto fail2;
            }
            printf("\nread the %i l2 table",i);
        }
        printf("\nthe %d",i);
    }

    for(i=0;all_file_path[i];i++){
        sprintf(file_name,"/%s",all_file_path[i]);
        //sprintf(file_name,"/home/base/Desktop/a.txt");

        gs1=guestfs_lstatns(g,file_name);
        if(gs1==NULL){
            ext_error_file_count++;
            continue;
        }
        if(S_ISREG(gs1->st_mode)!=1){
            //printf("\nfile:%s;\ninode:%d",file_name,gs1->st_ino);
            continue;
        }
        ext_all_file_count++;
        /*****************************开始计算inode结构位置*******************************/
        inode_number=gs1->st_ino;

        index_in_gdesc_table=(inode_number-1)/inodes_per_group;

        inodetable_block_offset=group_desc_table[index_in_gdesc_table].bg_inode_table;
        //printf("\ninodetable_block_offset:%x",inodetable_block_offset);
        printf("\n\n\n\nfilename:%s\ninode:%d,index_in_gdesc_table:%d,inodetable_block_offset:%x",file_name,inode_number,index_in_gdesc_table,inodetable_block_offset);
        inode_bytes_into_inodetable=((inode_number-1)%inodes_per_group)*inode_size;

        inode_blocks_into_inodetable=inode_bytes_into_inodetable/block_size;
        inode_bytes_into_block_of_inodetable=inode_bytes_into_inodetable%block_size;

        inode_blocks_offset=inodetable_block_offset+inode_blocks_into_inodetable;
        /*****************************块偏移到l1,l2表的转换*******************************/
        cluster_offset=(inode_blocks_offset>>(cluster_bits-block_bits))+(EXT2_FILESYSTEM_OFFSET/(1<<cluster_bits));
        bytes_into_cluster=(((inode_blocks_offset%CLUSTER_BYTES)*((1<<block_bits)%CLUSTER_BYTES))%CLUSTER_BYTES +
                         EXT2_FILESYSTEM_OFFSET%CLUSTER_BYTES + inode_bytes_into_block_of_inodetable%CLUSTER_BYTES)%CLUSTER_BYTES;
        //((inode_blocks_offset&((1<<(cluster_bits-block_bits))-1))<<block_bits)+inode_bytes_into_block_of_inodetable;
        //printf("\n:bytes_into_cluster%d",bytes_into_cluster);
        l1_index=cluster_offset>>l2_bits;
        l2_index=cluster_offset&((1<<l2_bits)-1);
        l1_table_entry=l1_table[l1_index];
        if(!l1_table_entry){
            printf("\nl1 table entry not be allocated,inode in base!");
            //continue;
        }
        l2_table_entry=__bswap_64(l2_tables[l1_index][l2_index])&L2E_OFFSET_MASK;
        if(!l2_table_entry){
            printf("\nl2 table entry not be allocated,inode in base!");
            continue;
        }
        printf("\ninode_blocks_offset:%d,cluster_offset:%d,l1_index:%d,l2_index:%d",inode_blocks_offset,cluster_offset,l1_index,l2_index);
//        printf("\ninode:l1_table_offset:%x",l1_table_entry);
//        printf("\ninode:l2_table_offset:%x",l2_table_entry);
        if(fseek(o_fp,l2_table_entry+bytes_into_cluster,SEEK_SET)){
            printf("\nseek to l2 table entry failed!");
            continue;
        }
        if(fread(e_ino,sizeof(struct ext2_inode),1,o_fp)<=0){
            printf("\nread inode failed!");
            continue;
        }
        /**
         *inode在增量中
         */
        printf("\nfile size:%d",e_ino->i_size);
        eh=(struct ext4_extent_header *)((char *)&(e_ino->i_block[0]));
        if(eh->eh_magic!=0xf30a){
            printf("\nbad data blocks pointer,bad magic!");
            continue;
        }
        if(eh->eh_depth==0 && eh->eh_entries>0){
            //it is leaf node
            //printf("\neh_depth:%d, eh->eh_entries:%d,size:%d",eh->eh_depth,eh->eh_entries,e_ino->i_size);
            ee=EXT_FIRST_EXTENT(eh);
            data_block_offset=(ee->ee_start_hi<<32)+ee->ee_start_lo;
            //695379
            //printf("\n ee block number:%d,size:%ld",data_block_offset,sizeof(data_block_offset));
            /**************************************由块偏移计算是否在增量中分配***********************************/
            //printf("\ndata block offset:%d",data_block_offset);

            cluster_offset=(data_block_offset>>(cluster_bits-block_bits))+(EXT2_FILESYSTEM_OFFSET/(1<<cluster_bits));
            l1_index=cluster_offset>>l2_bits;
            l2_index=cluster_offset&((1<<l2_bits)-1);
            l1_table_entry=l1_table[l1_index];
            printf("\ndata_block_offset:%ld;cluster_offset:%ld,data block l1_index:%d,l2_index:%d",data_block_offset,cluster_offset,l1_index,l2_index);
            if(!l1_table_entry){
                printf("\nl1 table entry not be allocated,data in base!");
                continue;
            }
            l2_table_entry=__bswap_64(l2_tables[l1_index][l2_index])&L2E_OFFSET_MASK;
            if(!l2_table_entry){
                printf("\nl2 table entry not be allocated,data in base!");
                continue;
            }
            //printf("\ndata block:l1_table_offset:%x",l1_table_entry);
            //printf("\ndata block:l2_table_offset:%x",l2_table_entry);
            /**
             *文件内容在增量中，需计算哈希值
             */
            {
                printf("\nbegin to cal md5.....%d,%d",ext_all_file_count,overlay_file_number);
                overlay_file_number++;
                md5str=guestfs_checksum(g,"md5",file_name);
                if(md5str!=NULL){
                    printf("\nfile:%s\nmd5:%s",file_name,md5str);
                    free(md5str);
                    md5str=NULL;
                }
            }
        }else if(eh->eh_depth>0 && eh->eh_entries>0){
            //it is index node,
            ext_idx = EXT_FIRST_INDEX(eh);
            index_block_offset=(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo;
            //printf("\n.......there is a ext_idx node block:%x",index_block_offset);
            cluster_offset=(index_block_offset>>(cluster_bits-block_bits))+(EXT2_FILESYSTEM_OFFSET/(1<<cluster_bits));
            l1_index=cluster_offset>>l2_bits;
            l2_index=cluster_offset&((1<<l2_bits)-1);
            printf("\nindex_block_offset:%ld;cluster_offset:%ld,data block l1_index:%d,l2_index:%d",index_block_offset,cluster_offset,l1_index,l2_index);
            l1_table_entry=l1_table[l1_index];
            if(!l1_table_entry){
                printf("\nl1 table entry not be allocated,data in base!");
                continue;
            }
            l2_table_entry=__bswap_64(l2_tables[l1_index][l2_index])&L2E_OFFSET_MASK;
            if(!l2_table_entry){
                printf("\nl2 table entry not be allocated,data in base!");
                continue;
            }
            /**
             *文件内容在增量中，需计算哈希值
             */
            {
                printf("\nbegin to cal md5.....%d,%d",ext_all_file_count,overlay_file_number);
                overlay_file_number++;
                md5str=guestfs_checksum(g,"md5",file_name);
                if(md5str!=NULL){
                    printf("\nfile:%s\nmd5:%s",file_name,md5str);
                    free(md5str);
                    md5str=NULL;
                }
            }
        }else{
            printf("\nsomething wrong in block pointer!");
        }
        //guestfs_free_statns_list(gs1);
    }
    printf("\nall file:%d\nregular file:%d\nerror file:%d;overlay_file:%d\n",i,ext_all_file_count,ext_error_file_count,overlay_file_number);
//    if(gs1!=NULL){
//        guestfs_free_statns_list(gs1);
//    }
//    printf("\n增量文件占比:%.5f\%",(((float)overlay_file_count)/(all_file_count-error_file_count-read_error))*100);
//    if(md5str!=NULL){
//        printf("\nfree last md5!");
//        free(md5str);
//    }
out:
    printf("\nout....");
    for(i=0;all_file_path[i];i++){
        free(all_file_path[i]);
    }
    free(all_file_path);
    for(i=0;i<l1_size;i++){
        if(l2_tables[i]!=NULL){
            free(l2_tables[i]);
        }
    }
    free(l2_tables);
    guestfs_umount (g, "/");
    guestfs_shutdown (g);
    guestfs_close (g);
    fclose(o_fp);
    fclose(bi_fp);
    free(e_ino);
    free(group_desc_table);
    free(es);
    free(header);
    free(l1_table);
    //free(l2_tables);
    return 1;

fail2:
    printf("\nfail_malloc_l2_tables....");
    for(i=0;i<l1_size;i++){
        if(l2_tables[i]!=NULL){
            free(l2_tables[i]);
        }
    }
    free(l2_tables);
fail_l2_tables:
    //printf("\nfail malloc l2 table");
    free(l1_table);
fail_l1_table:

fail_read_header:
    //printf("\nfail_read_header....");
    free(header);
fail_header:
    //printf("\nfail malloc header....");
    fclose(o_fp);
fail1:
    free(group_desc_table);
fail_group_desc_table:
    //printf("\nfail malloc group_desc_table....");
    free(e_ino);
fail_inode:
    //printf("\nfail malloc inode....");
    free(es);
fail_es:
fail:
    //printf("\nfail....");
    fclose(bi_fp);
error:
    printf("\nerror....");
    for(i=0;all_file_path[i];i++){
        free(all_file_path[i]);
    }
    free(all_file_path);
    return -1;
//    fclose(fp);
//    free(md5str);
}

int init_ext2_filesystem(){

    return 1;
fail:
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
