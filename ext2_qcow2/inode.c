
#include "inode.h"


int inodeInOverlay(char *baseImage,char *qcow2Image,unsigned int block_offset,unsigned int bytes_offset_into_block,__U16_TYPE block_bits,struct ext2_inode *inode){
    FILE *bi_fp,*l_fp;
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
    __U64_TYPE l2_entry_offset_cluster;
    __U64_TYPE data_offset;//data offset,Must be aligned to a cluster boundary.
    char *read_backingfile_name;
    bi_fp=fopen(qcow2Image,"r");
    if(bi_fp==NULL){
        printf("\n open qcow2Image failed!");
        return -1;
    }
    header=(struct QCowHeader*)malloc(sizeof(struct QCowHeader*));
    if(fread(header,sizeof(struct QCowHeader),1,bi_fp)<=0){
        printf("\n read qcow2header failed!");
        return -2;
    }
    header->version=__bswap_32(header->version);
    header->backing_file_offset=__bswap_64(header->backing_file_offset);
    header->backing_file_size=__bswap_32(header->backing_file_size);

    printf("\n the qcow2 version%d",header->version);
    fseek(bi_fp,header->backing_file_offset,SEEK_SET);
    int len=header->backing_file_size-9;
    read_backingfile_name=(char *)malloc(header->backing_file_size+1);
    if(fgets(read_backingfile_name,header->backing_file_size+1,bi_fp)==NULL){
        printf("\n read backingfile name error!");
        return -3;
    }
    printf("\n size:%d,backing file name:%s,string len %d",header->backing_file_size,read_backingfile_name,sizeof(read_backingfile_name));
    if(strstr(baseImage,read_backingfile_name)==NULL){
        printf("\n wrong overlay images!");
        return -4;
    }
    //the beginning 1M(256 blocks) not belongs to ext4 fs,I think.
    block_offset+=256;

    l1_table_offset=__bswap_64(header->l1_table_offset);
    cluster_bits=__bswap_32(header->cluster_bits);
    cluster_offset=block_offset/(1<<(cluster_bits-block_bits));
    block_into_cluster=block_offset%(1<<(cluster_bits-block_bits));
    offset_into_cluster=block_into_cluster<<block_bits;
    l2_bits=cluster_bits-3;
    l1_index=cluster_offset>>l2_bits;
    l2_index=cluster_offset & ((1<<l2_bits)-1);
    printf("\nblock_offset%d, cluster offset:%d,l1 index:%d,l2 index:%x,offset_into_cluster%x",block_offset,cluster_offset,l1_index,l2_index<<3,offset_into_cluster);
    l1_offset=l1_table_offset+(l1_index<<3);
    if(fseek(bi_fp,l1_offset,SEEK_SET)){
        printf("\n seek to l1 offset failed!");
        return -5;
    }
    if(fread(&l2_offset,sizeof(l2_offset),1,bi_fp)<=0){
        printf("\n read the l2 offset failed!");
        return -6;
    }
    l2_offset=__bswap_64(l2_offset) & L1E_OFFSET_MASK;
    printf("\nsize:%d l2_offset:%x",sizeof(l2_offset),l2_offset);
    //判断此l2表是否分配
    if(!l2_offset){
        printf("\n l2 table has not been allocated,the data must in backing file!");
        return 0;
    }
    l2_entry_offset_cluster=l2_offset+(l2_index<<3);
    if(fseek(bi_fp,l2_entry_offset_cluster,SEEK_SET)){
        printf("\n seek to l2_entry_offset_cluster failed!");
        return -7;
    }
    if(fread(&data_offset,sizeof(data_offset),1,bi_fp)<=0){
        printf("\n read data offset failed!");
        return -8;
    }
    data_offset=__bswap_64(data_offset);
    if(!(data_offset & L2E_OFFSET_MASK)){
        printf("\n l2 entry has not been allocated,the data must in backing file!");
        return 0;
    }
    printf("\n size:%d,the data offset:%x",sizeof(data_offset),data_offset);
    //data_offset=data_offset+offset_into_cluster+bytes_offset_into_block;
    //printf("\n data_offset:%d,long int:%d",data_offset,sizeof(long int));
    int i;
    int count;
    count=data_offset/(1<<cluster_bits);
    printf("\n count:%d,cluster size:%d",count,1<<cluster_bits);
    if(fseek(bi_fp,0,SEEK_SET)){
        printf("\n seek to data_offset failed!");
        return -9;
    }
    for(i=0;i<count;i++){
        //printf("\n************count********:%d",i);
        if(fseek(bi_fp,1<<cluster_bits,SEEK_CUR)){
            printf("\n seek to data_offset failed!");
            return -9;
        }
    }
    if(fseek(bi_fp,offset_into_cluster+bytes_offset_into_block,SEEK_CUR)){
        printf("\n seek to data_offset failed!");
        return -9;
    }
    if(fread(inode,sizeof(struct ext2_inode),1,bi_fp)<=0){
        printf("\n read inode failed!");
        return -10;
    }
    return 1;
}
/*
*given a block offset ,to find which image the data in?
*author:liuyang
*date:2017/3/14
*param:char *baseImage,char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits
*return int
*/
int blockInOverlay(char *baseImage,char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits){
    FILE *bi_fp,*l_fp;
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
    __U64_TYPE l2_offset_into_cluster;
    __U64_TYPE data_offset;//data offset,Must be aligned to a cluster boundary.
    char *read_backingfile_name;
    bi_fp=fopen(qcow2Image,"r");
    if(bi_fp==NULL){
        printf("\n open qcow2Image failed!");
        return -1;
    }
    header=malloc(sizeof(struct QCowHeader*));
    if(fread(header,sizeof(struct QCowHeader),1,bi_fp)<=0){
        printf("\n read qcow2header failed!");
        return -2;
    }
    header->version=__bswap_32(header->version);
    header->backing_file_offset=__bswap_64(header->backing_file_offset);
    header->backing_file_size=__bswap_32(header->backing_file_size);

    printf("\n the qcow2 version%d",header->version);
    fseek(bi_fp,header->backing_file_offset,SEEK_SET);
    int len=header->backing_file_size-9;
    read_backingfile_name=(char *)malloc(header->backing_file_size+1);
    if(fgets(read_backingfile_name,header->backing_file_size+1,bi_fp)==NULL){
        printf("\n read backingfile name error!");
        return -3;
    }
    printf("\n size:%d,backing file name:%s,string len %d",header->backing_file_size,read_backingfile_name,sizeof(read_backingfile_name));
    if(strstr(baseImage,read_backingfile_name)==NULL){
        printf("\n wrong overlay images!");
        return -4;
    }
    //the beginning 1M(256 blocks) not belongs to ext4 fs,I think.
    block_offset+=256;

    l1_table_offset=__bswap_64(header->l1_table_offset);
    cluster_bits=__bswap_32(header->cluster_bits);
    cluster_offset=block_offset/(1<<(cluster_bits-block_bits));
    block_into_cluster=block_offset%(1<<(cluster_bits-block_bits));
    offset_into_cluster=block_into_cluster<<block_bits;
    l2_bits=cluster_bits-3;
    l1_index=cluster_offset>>l2_bits;
    l2_index=cluster_offset & ((1<<l2_bits)-1);
    printf("\nblock_offset%d, cluster offset:%d,l1 index:%d,l2 index:%x,offset_into_cluster%x",block_offset,cluster_offset,l1_index,l2_index<<3,offset_into_cluster);
    l1_offset=l1_table_offset+(l1_index<<3);
    if(fseek(bi_fp,l1_offset,SEEK_SET)){
        printf("\n seek to l1 offset failed!");
        return -5;
    }
    if(fread(&l2_offset,sizeof(l2_offset),1,bi_fp)<=0){
        printf("\n read the l2 offset failed!");
        return -6;
    }
    l2_offset=__bswap_64(l2_offset) & L1E_OFFSET_MASK;
    printf("\nsize:%d l2_offset:%x",sizeof(l2_offset),l2_offset);
    //判断此l2表是否分配
    if(!l2_offset){
        printf("\n l2 table has not been allocated,the data must in backing file!");
        return 0;
    }
    l2_offset_into_cluster=l2_offset+(l2_index<<3);
    if(fseek(bi_fp,l2_offset_into_cluster,SEEK_SET)){
        printf("\n seek to l2_offset_into_cluster failed!");
        return -7;
    }
    if(fread(&data_offset,sizeof(data_offset),1,bi_fp)<=0){
        printf("\n read data offset failed!");
        return -8;
    }
    data_offset=__bswap_64(data_offset);
    if(!(data_offset & L2E_OFFSET_MASK)){
        printf("\n l2 entry has not been allocated,the data must in backing file!");
        return 0;
    }
    printf("\n size:%d,the data offset:%x",sizeof(data_offset),data_offset);
    fclose(bi_fp);
    free(read_backingfile_name);
    //free(header->autoclear_features);
    //header=NULL;
    //free(read_backingfile_name);
    return 1;
}


/*
*given a inode ,to find which image the data in?
*author:liuyang
*date:2017/3/9
*param:char * base image;char * qcow2 image;int inode
*return int
*/

int which_images_by_inode(char *baseImage,char *qcow2Image,unsigned int inode){
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
    FILE *bi_fp,*l_fp;
    //printf("\nbaseImage:%s",baseImage);
    bi_fp=fopen(baseImage,"r");
    if(bi_fp==NULL){
        printf("\n error:open failed!");
        return -1;
    }
    printf("\nopen baseImage successfully!,%ld",sizeof(struct ext2_super_block));
   //offset 1M to begin
    fseek(bi_fp,Meg,SEEK_SET);
    //offset 1k to cur

    if(fseek(bi_fp,Kilo,SEEK_CUR)){
        printf("\n seek to super block failed!");
        return -2;
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
        return -3;
    }
    block_bits=10+es->s_log_block_size;
    block_size=1<<block_bits;
    inode_per_group=es->s_inodes_per_group;
    inode_size=es->s_inode_size;
    /*
     *1.calculate which bg desc entry the inode in
     *2.get the beginning block number of inodetable the inode in from desc entry
     *3.calculate the exactly block number the inode in
     */
    inode_in_which_blockgroup=(inode-1)/inode_per_group;
    //the offest into the bg desc
    offset_in_bg_desc=inode_in_which_blockgroup*BG_DESC_SIZE;


    //index of inodetable 3878
    inode_index_in_inodetable=(inode-1)%inode_per_group;
    //offset into inodetable 992768
    inode_offset_in_inodetable=inode_index_in_inodetable*inode_size;

    inode_block_offset_in_inodetable=inode_offset_in_inodetable/block_size;
    inode_bytes_offset_into_inodetable=inode_offset_in_inodetable%block_size;
    /*
     *as we know no matter where the file in,the desc table never change
     *test inode:133415 16
     */
    fseek(bi_fp,Meg,SEEK_SET);fseek(bi_fp,block_size,SEEK_CUR);
    fseek(bi_fp,offset_in_bg_desc,SEEK_CUR);
    gdesc=(struct ext2_group_desc*)malloc(sizeof(struct ext2_group_desc));
    if(fread(gdesc,sizeof(struct ext2_group_desc),1,bi_fp)<=0){
        printf("\nread group descriptor entry failed!");
        return -4;
    }
    begin_block_of_inodetable=gdesc->bg_inode_table;
    //the real block number of inode
    inode_block_number=begin_block_of_inodetable+inode_block_offset_in_inodetable;
    printf("\n inode block offest in inodetable:%d,\n the real block number of inode%d,\n inode_bytes_offset_into_inodetable%x",
               inode_block_offset_in_inodetable,inode_block_number,inode_bytes_offset_into_inodetable);
    int inode_status=blockInOverlay(baseImage,qcow2Image,inode_block_number,block_bits);
    if(inode_status==1){
        printf("\n inode in overlay!");
        e_ino=(struct ext2_inode*)malloc(sizeof(struct ext2_inode));
        if(inodeInOverlay(baseImage,qcow2Image,inode_block_number,inode_bytes_offset_into_inodetable,block_bits,e_ino)==1){
            printf("\n read inode from overlay successful!");

            eh=(struct ext4_extent_header *)((char *) &(e_ino->i_block[0]));
            printf("\n eh->magic:%x",eh->eh_magic);
            if(eh->eh_magic==0xf30a){
                if(eh->eh_depth==0){
                    //it is leaf node
                    ee=EXT_FIRST_EXTENT(eh);
                    printf("\n ee block number:%d",(ee->ee_start_hi<<32)+ee->ee_start_lo);
                }else{
                    //it is index node,
                    ext_idx = EXT_FIRST_INDEX(eh);
                    printf("\n ext_idx leaf node block:%x",(ext_idx->ei_leaf_hi<<32)+ext_idx->ei_leaf_lo);
                }
            }//

        }

    }else if(inode_status==0){
        printf("\n inode in baseimage!");
    }else{
        printf("\n something error!");
    }
    return 0;
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
    printf("\n the free inodes:%ld,blocksize:%d,inodesize:%d,the index in inodetable:%d,sizeof(struct ext2_inode):%d",
           es->s_inodes_count,block_size,es->s_inode_size,inode_index_in_inodetable,sizeof(struct ext2_inode));
    /*
    *into the head of second block,read the blockgroup desc  entry,
    *and get the number of first block of the inodetable
    */
    fseek(fp,Meg,SEEK_SET);fseek(fp,block_size,SEEK_CUR);
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
