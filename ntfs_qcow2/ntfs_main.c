#include "ntfs_main.h"
/*
 *author:liuyang
 *date  :2017/4/18
 *detail: 判断文件记录是否在增量中
 *return
 */
int ntfs_inode_in_overlay(char *overlay_image_path,struct ntfs_inode_info *ntfs_inode){
    __U32_TYPE cluster_offset;
    __U32_TYPE byte_offset_into_cluster;
    FILE *l_fp;
    QCowHeader header;
    __U32_TYPE CLUSTER_BITS;
    __U32_TYPE CLUSTER_BYTES;
    __U32_TYPE l1_index;
    __U32_TYPE l2_index;
    __U64_TYPE L1_TABLE_OFFSET;
    __U32_TYPE L2_BITS;
    __U64_TYPE l1_entry_offset;
    __U64_TYPE l2_table_offset;
    __U64_TYPE l2_entry_offset;
    __U64_TYPE data_offset;//data offset,Must be aligned to a cluster boundary.
    if(ntfs_inode==NULL){
        printf("\nntfs_inode_in_overlay error:ntfs inode null!");
        goto fail;
    }
    l_fp=fopen(overlay_image_path,"r");
    if(l_fp==NULL){
        printf("\nntfs_inode_in_overlay error:open overlay image failed!");
        goto fail;
    }
    /***************************读取增量镜像header结构体**************************************************/
    if(fread(&header,sizeof(struct QCowHeader),1,l_fp)<=0){
        printf("\n read qcow2header failed!");
        goto fail0;
    }
    header.version=__bswap_32(header.version);
    printf("\nthe magic:%x",header.version);
    L1_TABLE_OFFSET=__bswap_64(header.l1_table_offset);
//    header->backing_file_offset=__bswap_64(header->backing_file_offset);
//    header->backing_file_size=__bswap_32(header->backing_file_size);
    CLUSTER_BITS=__bswap_32(header.cluster_bits);
    CLUSTER_BYTES=1<<CLUSTER_BITS;
    L2_BITS=CLUSTER_BITS-3;
    cluster_offset=((float)NTFS_OFFSET)/CLUSTER_BYTES+
                                            ((float)ntfs_inode->i_number)/(CLUSTER_BYTES/ntfs_inode->vol->mft_recordsize)+
                                                    ((float)ntfs_inode->vol->mft_cluster)/(CLUSTER_BYTES/ntfs_inode->vol->clustersize);
//    printf("\nNTFS OFFSET:%f\nntfs inode:%f\nmft begin:%f",
//           ((float)NTFS_OFFSET)/(1<<CLUSTER_BITS),
//           ((float)ntfs_inode->i_number)/((1<<CLUSTER_BITS)/ntfs_inode->vol->mft_recordsize),
//           ((float)ntfs_inode->vol->mft_cluster)/((1<<CLUSTER_BITS)/ntfs_inode->vol->clustersize));
    printf("\ncluster offset:%d",cluster_offset);
    byte_offset_into_cluster=(NTFS_OFFSET % CLUSTER_BYTES +
                        ((ntfs_inode->i_number % CLUSTER_BYTES)*(ntfs_inode->vol->mft_recordsize%CLUSTER_BYTES))%CLUSTER_BYTES +
                            ((ntfs_inode->vol->mft_cluster%CLUSTER_BYTES)*(ntfs_inode->vol->clustersize%CLUSTER_BYTES))%CLUSTER_BYTES)%CLUSTER_BYTES;
    printf("\nbyte_offset_into_cluster:%x",byte_offset_into_cluster);
    l1_index=cluster_offset>>L2_BITS;                                   /**l1表的偏移项数*/
    l2_index=cluster_offset & ((1<<L2_BITS)-1);                         /**l2表的偏移项数*/
    printf("\ncluster offset:%d\nl1 index:%d,l2 index:%x,\nblocks_bytes_into_cluster%x",cluster_offset,l1_index,l2_index<<3,byte_offset_into_cluster);
    /*****************************************************读取l1表************************************************************/
    l1_entry_offset=L1_TABLE_OFFSET+(l1_index<<3);
    if(fseek(l_fp,l1_entry_offset,SEEK_SET)){
        printf("\n seek to l1 offset failed!");
        goto fail0;
    }
    if(fread(&l2_table_offset,sizeof(l2_table_offset),1,l_fp)<=0){
        printf("\n read the l2 offset failed!");
        goto fail0;
    }
    l2_table_offset=__bswap_64(l2_table_offset) & L1E_OFFSET_MASK;
    printf("\nsize:%ld l2_table_offset:%lx",sizeof(l2_table_offset),l2_table_offset);
    if(!l2_table_offset){
        printf("\n l2 table has not been allocated,the data must in backing file!");
        goto inbacking;
    }
    /*****************************************************读取l2表************************************************************/
    l2_entry_offset=l2_table_offset+(l2_index<<3);
    if(fseek(l_fp,l2_entry_offset,SEEK_SET)){
        printf("\n seek to l2_entry_offset failed!");
        goto fail0;
    }
    if(fread(&data_offset,sizeof(data_offset),1,l_fp)<=0){
        printf("\n read data offset failed!");
        goto fail0;
    }
    data_offset=__bswap_64(data_offset);
    data_offset=data_offset&L2E_OFFSET_MASK;
    if(!(data_offset & L2E_OFFSET_MASK)){
        printf("\n l2 entry has not been allocated,the data must in backing file!");
        goto inbacking;
    }
    printf("\nsize:%ld,the data offset:%x",sizeof(data_offset),data_offset);

    if(fseek(l_fp,0,SEEK_SET)){
        printf("\n seek to SEEK_SET failed!");
        goto fail0;
    }
    if(fseek(l_fp,data_offset,SEEK_CUR)){
        printf("\n seek to unsigned long int data_offset failed!");
        goto fail0;
    }
    if(fseek(l_fp,byte_offset_into_cluster,SEEK_CUR)){
        goto fail0;
    }
    if(fread(ntfs_inode->attr,ntfs_inode->vol->mft_recordsize,1,l_fp)<=0){
        printf("\nerror: read file record failed!");
        goto fail0;
    }
    fclose(l_fp);
    return 1;
inbacking:
    fclose(l_fp);
    return 0;
fail0:
    fclose(l_fp);
fail:
    return -1;
}
/*
 *author:liuyang
 *date  :2017/5/10
 *detail:
 *return
 */
int ntfs_overlay_md5(char *baseImage,char *overlay,char *overlay_id){
    /**病毒库匹配*/
    MYSQL *my_conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    char strsql[256];
    my_conn=mysql_init(NULL);
    int count,k;
    char **viruses;
    __U32_TYPE *viruses_id;
    time_t start,end;
    start=time(NULL);
    /**ntfs文件系统相关数据结构*/
    ntfs_volume vol;
    char ntfs_super_block[80];
    __U16_TYPE ntfs_cluster_bits;
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
    struct ntfs_inode_info ino;
    ntfs_attribute *attr;
    __U32_TYPE data_cluster_offset;
    char *attrdata;
    ntfs_attribute *ntfs_attr;
    int attr_type,attr_len;
    void *data;
    int namelen;
    short int *name;
    /**runlist相关*/
    unsigned char *run_data;
    int startvcn,endvcn;
    int vcn,cnum;
    __U32_TYPE cluster;
    int len,ctype;

    char file_name[256]={NULL};
    char *md5str;
    int i,j;
//    float all_file_count;
    __U32_TYPE ext_error_file_count=0;
    __U32_TYPE ext_all_file_count=0;
    __U32_TYPE overlay_file_number=0;
    __U32_TYPE inode_in_overlay_file_number=0;

    FILE *bi_fp,*o_fp;
    struct guestfs_statns *gs1;
    char **all_file_path;
    printf("\nbegin to guestfs......");

    guestfs_h *g=guestfs_create ();
    guestfs_add_drive(g,overlay);
    guestfs_launch(g);
    guestfs_mount(g,"/dev/sda1","/");
    printf("\nbegin to get all file......");
    //all_file_path=guestfs_find(g,"/Windows");
    /***************************************************初始化ntfs文件系统信息**************************************************/
    bi_fp=fopen(baseImage,"r");
    if(bi_fp==NULL){
        printf("\nerror:open baseImage failed!");
        goto error;
    }
    /**偏移到ntfs文件系统开始*/
    if(fseek(bi_fp,NTFS_OFFSET,SEEK_SET)){
        printf("\n seek to ntfs super block failed!");
        goto fail;
    }
    if(fread(ntfs_super_block,80,1,bi_fp)<=0){
        printf("\n read to super block failed!");
        goto fail;
    }
    ntfs_init_volume(&vol,ntfs_super_block);
    printf("\nthe blocksize:%d\nclusterfactor:%d\n mft_cluster offset:%d\nmft_recordsize:%d,\nmft_clusters_per_record:%d\nclustersize:%d",
           vol.blocksize,vol.clusterfactor,vol.mft_cluster,vol.mft_recordsize,vol.mft_clusters_per_record,vol.clustersize);

    ino.vol=&vol;
    ino.attrs=0;
    /*all bit-1*/
    BIT_1_POS(ino.vol->clustersize,ntfs_cluster_bits);
    ino.attr=malloc(vol.mft_recordsize);
    if(!ino.attr){
        printf("\nmalloc error:malloc ino.attr failed!");
        goto fail;
    }
    ino.attrs=malloc(sizeof(ntfs_attribute));
    if(!ino.attrs){
        printf("\nmalloc error:malloc ino.attrs failed!");
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
    if(l1_table==NULL){
        printf("\nallocate l1 table failed!");
        goto fail_l1_table;
    }
    l2_tables=malloc(l1_size*sizeof(__U64_TYPE));
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
        //printf("\nmalloc the %d for l2_table!",i);
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
            //printf("\nread the %i l2 table",i);
        }
        //printf("\nthe %d",i);
    }
    /***********************************************加载病毒库*****************************************************/
    if(!mysql_real_connect(my_conn,dataBase.url,dataBase.username,dataBase.password,dataBase.database_name,0,NULL,0)) //连接detect数据库
    {
        printf("\nConnect Error!n");
        goto fail2;
    }
    if(mysql_query(my_conn,"select id,hash from virus")){
        printf("\nquery virus failed!");
        goto fail3;
    }
    i=0;
    res=mysql_store_result(my_conn);
    count=mysql_num_rows(res);
    printf("\ncount:%d",count);
    viruses=malloc(count*sizeof(char *));
    if(viruses==NULL){
        goto fail3;
    }
    viruses_id=malloc(count*sizeof(__U32_TYPE));
    if(viruses_id==NULL){
        goto fail4;
    }
    while(row=mysql_fetch_row(res)){
        viruses[i]=malloc(strlen(row[1])+1);
        strcpy(viruses[i],row[1]);
        viruses_id[i]=atoi(row[0]);
        printf("\nviruses_id:%d,length:%d;the virus md5:%s,%s",viruses_id[i],strlen(row[1]),viruses[i],row[1]);
        i++;
    }
    /***********************************************开始全盘扫描*****************************************************/
    char root_dir_name[128];
    char **root_d=guestfs_ls(g,"/");
    for(j=0;root_d[j];j++){
        sprintf(root_dir_name,"/%s",root_d[j]);
        if(S_ISDIR(guestfs_lstatns(g,root_dir_name)->st_mode)!=1||strcmp(root_dir_name,"/ProgramData")==0){
            continue;
        }
        printf("\n\n\nroot_dir:%s",root_dir_name);
        all_file_path=guestfs_find(g,root_dir_name);
        /****************************************************************************************/
        for(i=0;all_file_path[i];i++){
            sprintf(file_name,"%s%s",root_dir_name,all_file_path[i]);
            //printf("\nfilename:%s",file_name);
            //sprintf(file_name,"/liuyang.txt");
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
            ino.i_number=gs1->st_ino;//1072 80H non-resident,10720 resident
            //ino.i_number=1072;// 10720 5625
            cluster_offset=((float)NTFS_OFFSET)/CLUSTER_BYTES+
                                                    ((float)ino.i_number)/(CLUSTER_BYTES/ino.vol->mft_recordsize)+
                                                            ((float)ino.vol->mft_cluster)/(CLUSTER_BYTES/ino.vol->clustersize);
        //    printf("\nNTFS OFFSET:%f\nntfs inode:%f\nmft begin:%f",
        //           ((float)NTFS_OFFSET)/(1<<CLUSTER_BITS),
        //           ((float)ntfs_inode->i_number)/((1<<CLUSTER_BITS)/ntfs_inode->vol->mft_recordsize),
        //           ((float)ntfs_inode->vol->mft_cluster)/((1<<CLUSTER_BITS)/ntfs_inode->vol->clustersize));

            bytes_into_cluster=(NTFS_OFFSET % CLUSTER_BYTES +
                                ((ino.i_number % CLUSTER_BYTES)*(ino.vol->mft_recordsize%CLUSTER_BYTES))%CLUSTER_BYTES +
                                    ((ino.vol->mft_cluster%CLUSTER_BYTES)*(ino.vol->clustersize%CLUSTER_BYTES))%CLUSTER_BYTES)%CLUSTER_BYTES;
            //printf("\nbyte_offset_into_cluster:%x",byte_offset_into_cluster);
            //((inode_blocks_offset&((1<<(cluster_bits-block_bits))-1))<<block_bits)+inode_bytes_into_block_of_inodetable;
            //printf("\nfile record cluster offset:%d",cluster_offset);
            //printf("\nfile record bytes_into_cluster:%d",bytes_into_cluster);
            l1_index=cluster_offset>>l2_bits;
            l2_index=cluster_offset&((1<<l2_bits)-1);
            l1_table_entry=l1_table[l1_index];
            if(!l1_table_entry){
                //printf("\nl1 table entry not be allocated,inode in base!");
                //continue;
            }
            l2_table_entry=__bswap_64(l2_tables[l1_index][l2_index])&L2E_OFFSET_MASK;
            if(!l2_table_entry){
                //printf("\nl2 table entry not be allocated,inode in base!");
                continue;
            }
            //printf("\nl1_index:%d,l2_index:%d",l1_index,l2_index);
    //        printf("\ninode:l1_table_offset:%x",l1_table_entry);
    //        printf("\ninode:l2_table_offset:%x",l2_table_entry);
            if(fseek(o_fp,l2_table_entry+bytes_into_cluster,SEEK_SET)){
                printf("\nseek to l2 table entry failed!");
                continue;
            }
            if(fread(ino.attr,ino.vol->mft_recordsize,1,o_fp)<=0){
                printf("\nerror: read file record failed!");
                continue;
            }
            /**
             *inode在增量中
             */
            inode_in_overlay_file_number++;
            //printf("\n*******************NTFS inode in overlay !");
            if(!ntfs_check_mft_record(ino.vol,ino.attr,"FILE",ino.vol->mft_recordsize))
            {
                printf("Invalid MFT record corruption");
                continue;
            }
            //ino.sequence_number=NTFS_GETU16(ino.attr+0x10);
            ino.record_count=0;
            ino.records=0;
            //ntfs_load_attributes(&ino);
            /**
             *加载80属性
             */
            {
                attrdata=ino.attr+NTFS_GETU16(ino.attr+0x14);
                ino.attr_count=0;
                do{
                    attr_type=NTFS_GETU32(attrdata);
                    attr_len=NTFS_GETU32(attrdata+4);
                    if(attr_type!=-1) {
                        /* FIXME: check ntfs_insert_attribute for failure (e.g. no mem)? */
                        //ntfs_insert_attribute(ino,attrdata);
                        /*file main meta data info*/
                        if(attr_type==ino.vol->at_data){
                            ntfs_attr=ino.attrs+ino.attr_count;
                            namelen = NTFS_GETU8(attrdata+9);
                            /* read the attribute's name if it has one */
                            if(!namelen){
                                //TODO:in my program ,it no problem
                                name=0;
                                ntfs_attr->name=0;
                            }else{
                                printf("\n*********************never happen in 80H attr******************");
                                //TODO do not deal the name
                                /* 1 Unicode character fits in 2 bytes */
    //                            name=malloc(2*namelen);
    //                            if(!name ){
    //                                printf("\nntfs_load_attributes:malloc error!");
    //
    //                            }
    //                            //Unicode 2400 4900 3300 3000 -> short int 24 49 33 30
    //                            memcpy(name,attrdata+NTFS_GETU16(attrdata+10),2*namelen);
    //                            ntfs_attr->name=name;
                            }
                            ntfs_attr->namelen=namelen;
                            //printf("\nattribution type:%x",attr_type);
                            ntfs_attr->type=attr_type;
                            ntfs_attr->resident=(NTFS_GETU8(attrdata+8)==0);
                            /*如果是常驻属性*/
                            if(ntfs_attr->resident==1) {
                                //printf("\n:this is a resident attribution");
    //                            ntfs_attr->size=NTFS_GETU16(attrdata+0x10);
    //                            data=attrdata+NTFS_GETU16(attrdata+0x14);
    //                            ntfs_attr->d.data = (void*)malloc(ntfs_attr->size);
    //                            if( !ntfs_attr->d.data )
    //                                return -1;
    //                            memcpy(ntfs_attr->d.data,data,ntfs_attr->size);
    //                            ntfs_attr->indexed=NTFS_GETU16(attrdata+0x16);
    //                            printf("\nthe attr length:%d,%s",ntfs_attr->size,ntfs_attr->d.data);
                                /**
                                 *文件数据块在增量中,md5
                                 */
                                //printf("\nbegin to cal md5.....%d,%d",ext_all_file_count,overlay_file_number);
                                //md5str=guestfs_cat (g,file_name);
                                overlay_file_number++;
                                md5str=guestfs_checksum(g,"md5",file_name);
                                if(md5str!=NULL){
                                    for(k=0;k<count;k++){
                                        if(strcmp(viruses[k],md5str)==0){
                                            printf("\nvirus md5:%s,file md5:%s",viruses[k],md5str);
                                            if(guestfs_rm(g,file_name)==0){
                                                //printf("\noverlay_id:%s;file_name:%s,virus_id:%s",overlay_id,file_name,row[0]);
                                                sql_add_virus_detect_info(overlay_id,file_name,viruses_id[k]);
                                            }
                                        }
                                    }
                                    //printf("\nfile:%s\nmd5:%s",file_name,md5str);
                                    free(md5str);

                                    //md5str=NULL;
                                }
                            }else if(ntfs_attr->resident==0){
                                //printf("\n:this is a non-resident attribution");
                                ntfs_attr->size=NTFS_GETU32(attrdata+0x30);
                                ino.attrs[ino.attr_count].d.r.runlist=0;
                                ino.attrs[ino.attr_count].d.r.len=0;
                                //ntfs_process_runs(ino,ntfs_attr,attrdata);
                                /**
                                 *ntfs_process_runs
                                 */
                                {
                                    run_data=attrdata;
                                    startvcn = NTFS_GETU64(run_data+0x10);
                                    endvcn = NTFS_GETU64(run_data+0x18);

                                    /* check whether this chunk really belongs to the end */
                                    for(cnum=0,vcn=0;cnum<ntfs_attr->d.r.len;cnum++)
                                        vcn+=ntfs_attr->d.r.runlist[cnum].len;
                                    if(vcn!=startvcn)
                                    {
                                        printf("\nProblem with runlist in extended record\n");
                                        break;
                                    }
                                    if(!endvcn)
                                    {
                                        endvcn = NTFS_GETU64(run_data+0x28)-1; /* allocated length */
                                        endvcn /= ino.vol->clustersize;
                                        //printf("\nin ntfs_process_runs endvcn:%ld",endvcn);
                                    }
                                    run_data=run_data+NTFS_GETU16(run_data+0x20);
                                    cnum=ntfs_attr->d.r.len;
                                    cluster=0;
                                    for(vcn=startvcn; vcn<=endvcn; vcn+=len)
                                    {
                                        if(ntfs_decompress_run(&run_data,&len,&cluster,&ctype)){
                                            printf("\nerror:ntfs_decompress_run,next file!");
                                            break;//for(vcn=startvcn; vcn<=endvcn; vcn+=len)
                                            break;//do while
                                        }
                                        if(ctype){
                                            ntfs_insert_run(ntfs_attr,cnum,-1,len);
                                        }else{
                                            /**
                                             *开始判断文件数据块是否在增量中
                                             */
                                            //printf("\n\nget the first file data block offset:%x!",cluster);
                                            //ntfs_insert_run(ntfs_attr,cnum,cluster,len);
                                            cluster_offset=cluster/(1<<(cluster_bits-ntfs_cluster_bits))+(float)NTFS_OFFSET/(1<<cluster_bits);
                                            l1_index=cluster_offset>>l2_bits;
                                            l2_index=cluster_offset&((1<<l2_bits)-1);
                                            l1_table_entry=l1_table[l1_index];
                                            //printf("\l1 index:%x,l2 index:%x",l1_index,l2_index);
                                            if(!l1_table_entry){
                                                //printf("\nl1 table entry not be allocated,data in base,next file!");
                                                break;//for(vcn=startvcn; vcn<=endvcn; vcn+=len)
                                                break;//do while
                                            }
                                            l2_table_entry=__bswap_64(l2_tables[l1_index][l2_index])&L2E_OFFSET_MASK;
                                            //printf("\nl1_table_entry:%x,l2_table_entry:%x",l1_table_entry,l2_table_entry);
                                            if(!l2_table_entry){
                                                //printf("\nl2 table entry not be allocated,data in base,next file!");
                                                break;//for(vcn=startvcn; vcn<=endvcn; vcn+=len)
                                                break;//do while
                                            }
                                            /**
                                             *文件数据块在增量中,md5
                                             */
                                            //printf("\nbegin to cal md5.....%d,%d",ext_all_file_count,overlay_file_number);
                                            overlay_file_number++;
                                            //md5str=guestfs_cat (g,file_name);
                                            md5str=guestfs_checksum(g,"md5",file_name);
                                            if(md5str!=NULL){
                                                for(k=0;k<count;k++){
                                                    if(strcmp(viruses[k],md5str)==0){
                                                        printf("\nvirus md5:%s,file md5:%s",viruses[k],md5str);
                                                        if(guestfs_rm(g,file_name)==0){
                                                            //printf("\noverlay_id:%s;file_name:%s,virus_id:%s",overlay_id,file_name,row[0]);
                                                            sql_add_virus_detect_info(overlay_id,file_name,viruses_id[k]);
                                                        }
                                                    }
                                                }
                                                //printf("\nfile:%s\nmd5:%s",file_name,md5str);
                                                free(md5str);
                                                //md5str=NULL;
                                            }
                                            break;//for(vcn=startvcn; vcn<=endvcn; vcn+=len)
                                            break;//do while
                                        }
                                        cnum++;
                                    }
                                }//end ntfs_process_runs
                            }else{
                                printf("\nresident not 1 or 0,never happen!");
                            }
                            //ino.attr_count++;
                            break;
                        }//attr_type=ino.vol->at_data
                    }//attr_type!=-1
                    attrdata+=attr_len;
                }while(attr_type!=-1); /* attribute list ends with type -1 */

            }//end 加载80属性
            //guestfs_free_statns_list(gs1);
        }
        for(i=0;all_file_path[i];i++){
            free(all_file_path[i]);
        }
        free(all_file_path);
//        for(i=0;all_file_path[i];i++){
//            free(all_file_path[i]);
//        }
//        free(all_file_path);
        //break;
        //printf("\nroot:%s",root_d[i]);
    }
    printf("\nall file:%d\nregular file:%d\nerror file:%d;\ndata in overlay file:%d\ninode_in_overlay_file_number:%d",
           i,ext_all_file_count,ext_error_file_count,overlay_file_number,inode_in_overlay_file_number);
//    if(gs1!=NULL){
//        guestfs_free_statns_list(gs1);
//    }
    printf("\n增量文件占比:%.5f%%",(((float)overlay_file_number)/(i-ext_error_file_count))*100);
    end = time(NULL);
    sql_update_scan_info(overlay_id,ext_all_file_count,overlay_file_number,end-start);
out:
    printf("\nout....");
    for(i=0;i<count;i++){
        if(viruses[i]!=NULL){
            free(viruses[i]);
        }
    }
    free(viruses);
    free(viruses_id);
    for(i=0;i<l1_size;i++){
        if(l2_tables[i]!=NULL){
            free(l2_tables[i]);
        }
    }
    for(j=0;root_d[j];j++){
        free(root_d[j]);
    }
    free(root_d);
    free(l2_tables);
    free(header);
    free(l1_table);
    free(ino.attr);
    free(ino.attrs);
    fclose(o_fp);
    fclose(bi_fp);
    guestfs_umount (g, "/");
    guestfs_shutdown (g);
    guestfs_close (g);
    return 1;
fail4:
    free(viruses);
fail3:
    mysql_close(my_conn);
fail2:
    printf("\nfail_malloc_l2_tables....");
    for(i=0;i<l1_size;i++){
        if(l2_tables[i]!=NULL){
            free(l2_tables[i]);
        }
    }
    free(l2_tables);
fail_l2_tables:
    free(l1_table);
fail_l1_table:

fail_read_header:
    free(header);
fail_header:
    free(ino.attrs);
    fclose(o_fp);
fail1:
    free(ino.attr);
fail:
    //printf("\nfail....");
    fclose(bi_fp);
error:
    printf("\nerror....");
//    for(i=0;all_file_path[i];i++){
//        free(all_file_path[i]);
//    }
//    free(all_file_path);
    for(j=0;root_d[j];j++){
        free(root_d[j]);
    }
    free(root_d);
    end = time(NULL);
    sql_update_scan_info(overlay_id,ext_all_file_count,overlay_file_number,end-start);
    return -1;
//    fclose(fp);
//    free(md5str);
}
/*
 *author:liuyang
 *date  :2017/4/18
 *detail:循环读取全部runlist项并解析
 *return int
 */
static int ntfs_process_runs(struct ntfs_inode_info *ino,ntfs_attribute* attr,unsigned char *data)
{
	int startvcn,endvcn;
	int vcn,cnum;
	__U32_TYPE cluster;
	int len,ctype;
	startvcn = NTFS_GETU64(data+0x10);
	endvcn = NTFS_GETU64(data+0x18);

	/* check whether this chunk really belongs to the end */
	for(cnum=0,vcn=0;cnum<attr->d.r.len;cnum++)
		vcn+=attr->d.r.runlist[cnum].len;
	if(vcn!=startvcn)
	{
		printf("\nProblem with runlist in extended record\n");
		return -1;
	}
	if(!endvcn)
	{
		endvcn = NTFS_GETU64(data+0x28)-1; /* allocated length */
		endvcn /= ino->vol->clustersize;
		printf("\nin ntfs_process_runs endvcn:%ld",endvcn);
	}
	data=data+NTFS_GETU16(data+0x20);
	cnum=attr->d.r.len;
	cluster=0;
	for(vcn=startvcn; vcn<=endvcn; vcn+=len)
	{
		if(ntfs_decompress_run(&data,&len,&cluster,&ctype))
			return -1;
		if(ctype)
			ntfs_insert_run(attr,cnum,-1,len);
		else
			ntfs_insert_run(attr,cnum,cluster,len);
		cnum++;
	}
	return 0;
}
/*
 *author:liuyang
 *date  :2017/4/18
 *detail:将runlist结构化
 *return
 */
int ntfs_decompress_run(unsigned char **data, int *length, __U32_TYPE *cluster,int *ctype)
{
    /*读取第一个字节，并跳过第一字节*/
	unsigned char type=*(*data)++;
	*ctype=0;
	switch(type & 0xF)
	{
	case 1: *length=NTFS_GETU8(*data);break;
	case 2: *length=NTFS_GETU16(*data);break;
	case 3: *length=NTFS_GETU24(*data);break;
        case 4: *length=NTFS_GETU32(*data);break;
        	/* Note: cases 5-8 are probably pointless to code,
        	   since how many runs > 4GB of length are there?
        	   at the most, cases 5 and 6 are probably necessary,
        	   and would also require making length 64-bit
        	   throughout */
	default:
		printf("\nCan't decode run type field %x\n",type);
		return -1;
	}
	/*跳过length所占字节长度*/
	*data+=(type & 0xF);
    /* *cluster+= because data maybe <0,it is the run list rule*/
	switch(type & 0xF0)
	{
	case 0:	   *ctype=2; break;
	case 0x10: *cluster += NTFS_GETS8(*data);break;//pay attention to +=,why?
	case 0x20: *cluster += NTFS_GETS16(*data);break;
	case 0x30: *cluster += NTFS_GETS24(*data);break;
	case 0x40: *cluster += NTFS_GETS32(*data);break;
#if 0 /* Keep for future, in case ntfs_cluster_t ever becomes 64bit */
	case 0x50: *cluster += NTFS_GETS40(*data);break;
	case 0x60: *cluster += NTFS_GETS48(*data);break;
	case 0x70: *cluster += NTFS_GETS56(*data);break;
	case 0x80: *cluster += NTFS_GETS64(*data);break;
#endif
	default:
		printf("\nCan't decode run type field %x\n",type);
		return -1;
	}
	/*越过簇偏移所占字节*/
	*data+=(type >> 4);
	return 0;
}
/*
 *author:liuyang
 *date  :2017/4/18
 *detail:将结构化runlist插入到attr中
 *return void
 */
void ntfs_insert_run(ntfs_attribute *attr,int cnum,__U32_TYPE cluster,int len)
{
	/* (re-)allocate space if necessary */
	if(attr->d.r.len % 8 == 0) {
		ntfs_runlist* new;
		new = malloc((attr->d.r.len+8)*sizeof(ntfs_runlist));
		if( !new )
			return;
		if( attr->d.r.runlist ) {
			memcpy(new, attr->d.r.runlist, attr->d.r.len
				    *sizeof(ntfs_runlist));
			free( attr->d.r.runlist );
		}
		attr->d.r.runlist = new;
	}
	if(attr->d.r.len>cnum)
		memmove(attr->d.r.runlist+cnum+1,attr->d.r.runlist+cnum,
			    (attr->d.r.len-cnum)*sizeof(ntfs_runlist));
	attr->d.r.runlist[cnum].cluster=cluster;
	attr->d.r.runlist[cnum].len=len;
	attr->d.r.len++;
	printf("\nthe %d entry run list:len:%x,cluster:%x",attr->d.r.len,len,cluster);
}
/*
 *author:liuyang
 *date  :2017/4/16
 *detail:由inode判断文件的位置,更新文件位置
 *return
 */
int ntfs_update_file_metadata(char *overlay_image_path,char *base_image_path,__U64_TYPE inode[],int inode_count,char *overlay_id){
    ntfs_volume vol;
    FILE *bi_fp;
    struct ntfs_inode_info ino;
    int i;
    char ntfs_super_block[80];
    int in_overlay;
    bi_fp=fopen(base_image_path,"r");
    ntfs_attribute *attr;
    __U32_TYPE data_cluster_offset;
    __U16_TYPE ntfs_cluster_bits;
    if(bi_fp==NULL){
        printf("\n error:open ntfs image failed!");
        return -1;
    }
    /**偏移到ntfs文件系统开始*/
    if(fseek(bi_fp,0x7e00,SEEK_SET)){
        printf("\n seek to ntfs super block failed!");
        goto fail;
    }
    if(fread(ntfs_super_block,80,1,bi_fp)<=0){
        printf("\n read to super block failed!");
        goto fail;
    }
    ntfs_init_volume(&vol,ntfs_super_block);
    printf("\nthe blocksize:%d\nclusterfactor:%d\n mft_cluster offset:%d\nmft_recordsize:%d,\nmft_clusters_per_record:%d\nclustersize:%d",
           vol.blocksize,vol.clusterfactor,vol.mft_cluster,vol.mft_recordsize,vol.mft_clusters_per_record,vol.clustersize);

    ino.vol=&vol;
    ino.attr=malloc(vol.mft_recordsize);
    if(!ino.attr){
        printf("\nmalloc error:malloc ino.attr failed!");
        goto fail;
    }
    for(i=0;i<inode_count;i++){
        printf("\nthe file index:%ld",inode[i]);
        ino.i_number=inode[i];//1072 80H non-resident
        in_overlay=ntfs_inode_in_overlay(overlay_image_path,&ino);
        if(in_overlay==0){
            printf("\n*******************NTFS inode in backing image!");
            /*更新文件最新状态信息*/
            sql_update_file_metadata(overlay_id,ino.i_number,1,1);
        }else if(in_overlay==1){
            printf("\n*******************NTFS inode in overlay !");

            if(!ntfs_check_mft_record(ino.vol,ino.attr,"FILE",ino.vol->mft_recordsize))
            {
                printf("Invalid MFT record corruption");
                goto fail0;
            }
            ino.sequence_number=NTFS_GETU16(ino.attr+0x10);
            ino.attr_count=0;
            ino.record_count=0;
            ino.records=0;
            ino.attrs=0;
            ntfs_load_attributes(&ino);
            //printf("\nthe runlist number:%d,cluster:%x",ino.attrs[1].d.r.len,ino.attrs[1].d.r.runlist[0].cluster);
            /*文件的80属性，文件内容*/
            attr=ntfs_find_attr(&ino,ino.vol->at_data,0);
            if(attr){
                if(attr->resident==0){
                    printf("\非常驻80属性，文件内容在run list 中");
                    printf("\nsuccessful!the runlist number:%d,cluster:%x",attr->d.r.len,attr->d.r.runlist[0].cluster);
                    if(attr->d.r.len>0){
                        data_cluster_offset=attr->d.r.runlist[0].cluster;
                        /*all bit-1*/
                        BIT_1_POS(ino.vol->clustersize,ntfs_cluster_bits);
                        printf("\ndata_cluster_offset:%x,%d;\nthe ntfs_cluster_bits:%d",
                               data_cluster_offset,data_cluster_offset,ntfs_cluster_bits);
                        in_overlay=ntfs_blockInOverlay(overlay_image_path,data_cluster_offset,ntfs_cluster_bits);
                        if(in_overlay==0){
                            sql_update_file_metadata(overlay_id,ino.i_number,2,1);
                            printf("\n*******************:data in overlay not been allocated!");
                        }else if(in_overlay==1){
                            //TODO update file info
                            sql_update_file_metadata(overlay_id,ino.i_number,2,2);
                            printf("\n*******************:data in overlay  been allocated!");
                        }else{
                            printf("\n*******************:ntfs_blockInOverlay ERROR!");
                        }
                    }else{
                        printf("\nntfs_update_file_metadata:run list problem!");
                    }

                }else{
                    printf("\nthe file data attribution is resident,file data must be in overlay!");
                    /*更新文件最新状态信息*/
                    sql_update_file_metadata(overlay_id,ino.i_number,2,2);
                }
            }else{
                printf("\ncan not find file data attribution!");
            }
        }else{
            //NOT RUN!!!!!!!!!!!!!!!!
            if(ino.attrs!=NULL){
                free(ino.attrs);
            }
            printf("\n*******************ntfs_in_overlay error!!!");
        }
    }
    /********************************************************************/
    free(ino.attr);
    fclose(bi_fp);
    return 1;
fail0:
    free(ino.attr);
fail:
    fclose(bi_fp);
    return -1;
}
/*
*author:liuyang
*date  :2017/4/19
*detail:根据块偏移判断是否在增量中分配
*return:int 1:inode in overlay,0:inode in baseImage,<0:error
*/
int ntfs_blockInOverlay(char *qcow2Image,unsigned int block_offset,__U16_TYPE block_bits){
    printf("\n\n\n\n\n\n\n\n\nbegin ntfs_blockInOverlay.......");
    FILE *l_fp;
    QCowHeader header;
    __U32_TYPE cluster_bits;
    __U32_TYPE CLUSTER_BYTES;
    __U32_TYPE l1_index;
    __U32_TYPE l2_index;
    __U64_TYPE l1_table_offset;
    __U32_TYPE cluster_offset;
    __U32_TYPE block_into_cluster;
    __U32_TYPE bytes_into_cluster;
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
    CLUSTER_BYTES=1<<cluster_bits;
    l2_bits=cluster_bits-3;
    /*******************************************根据块偏移映射到l1,l2表，判断该块在增量中是否分配************************************************/
    //cluster_offset=block_offset/(1<<(cluster_bits-block_bits));
    printf("\n*cluster offset:%f",(float)NTFS_OFFSET/(1<<cluster_bits));
    cluster_offset=block_offset/(1<<(cluster_bits-block_bits))+(float)NTFS_OFFSET/(1<<cluster_bits);
    printf("\n*cluster offset:%d",cluster_offset);
    //block_into_cluster=block_offset%(1<<(cluster_bits-block_bits));
    bytes_into_cluster=(((block_offset%CLUSTER_BYTES)*((1<<block_bits)%CLUSTER_BYTES))%CLUSTER_BYTES +
                         NTFS_OFFSET%CLUSTER_BYTES)%CLUSTER_BYTES;
    l1_index=cluster_offset>>l2_bits;
    l2_index=cluster_offset & ((1<<l2_bits)-1);
    printf("\nblock_offset%d;\ncluster offset:%d;\nl1 index:%d;\nl2 index:%x,bytes_into_cluster:%x",
                                            block_offset,cluster_offset,l1_index,l2_index<<3,bytes_into_cluster);
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
        //printf("\n l2 table has not been allocated,the data must in backing file!");
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
        //printf("\n l2 entry has not been allocated,the data must in backing file!");
        goto unallocated;
    }
    //printf("\n size:%ld,the data offset:%x",sizeof(data_offset),data_offset);

success:
    fclose(l_fp);
    printf("\nsuccess,leave ntfs_blockInOverlay......\n\n\n\n\n\n");
    return 1;
fail:
    fclose(l_fp);
    printf("\nfail,leave ntfs_blockInOverlay......\n\n\n\n\n\n");
    return -1;
unallocated:
    fclose(l_fp);
    printf("\nunallocated,leave ntfs_blockInOverlay......\n\n\n\n\n\n");
    return 0;

}
/*
 *author:liuyang
 *date  :2017/4/18
 *detail:
 *return
 */
ntfs_attribute* ntfs_find_attr(struct ntfs_inode_info *ino,int type,char *name)
{
	int i;
	if(!ino){
		printf("\nntfs_find_attr: NO INODE!\n");
		return 0;
	}
	for(i=0;i<ino->attr_count;i++)
	{
		if(type==ino->attrs[i].type)
		{
			if(!name && !ino->attrs[i].name)
				return ino->attrs+i;
			if(name && !ino->attrs[i].name)
				return 0;
			if(!name && ino->attrs[i].name)
				return 0;
			if(ntfs_ua_strncmp(ino->attrs[i].name,name,strlen(name))==0)
				return ino->attrs+i;
		}
		if(type<ino->attrs[i].type)
			return 0;
	}
	return 0;
}
/*
 *author:liuyang
 *date  :2017/4/18
 *detail:
 *return
 */
/* strncmp between Unicode and ASCII strings */
int ntfs_ua_strncmp(short int* a,char* b,int n)
{
    printf("\nntfs_ua_strncmp!");
	int i;
	for(i=0;i<n;i++)
	{
		if(NTFS_GETU16(a+i)<b[i])
			return -1;
		if(b[i]<NTFS_GETU16(a+i))
			return 1;
		if (b[i] == 0)
			return 0;
	}
	return 0;
}

/*
 *author:liuyang
 *date  :2017/4/18
 *detail: 对文件记录属性进行结构化解析
 *return
 */
static void ntfs_load_attributes(struct ntfs_inode_info* ino)
{
	//ntfs_attribute *alist;
	//int datasize;
	//int offset,len,delta;
	//char *buf;
	ntfs_volume *vol=ino->vol;
	char *attrdata;
	ntfs_attribute *ntfs_attr;
	int attr_type,attr_len;
	void *data;
	int namelen;
	short int *name;
    attrdata=ino->attr+NTFS_GETU16(ino->attr+0x14);
    ino->attrs=malloc(8*sizeof(ntfs_attribute));
    ino->attr_count=0;

	do{
		attr_type=NTFS_GETU32(attrdata);
		attr_len=NTFS_GETU32(attrdata+4);
		if(attr_type!=-1) {
			/* FIXME: check ntfs_insert_attribute for failure (e.g. no mem)? */
			//ntfs_insert_attribute(ino,attrdata);
            /*******************************特殊处理，如果存在，只加载0x30H和0x80H属性***********************************/
            /*file main meta data info*/
            if(attr_type==vol->at_standard_information || attr_type==vol->at_data){
                ntfs_attr=ino->attrs+ino->attr_count;
                namelen = NTFS_GETU8(attrdata+9);
                /* read the attribute's name if it has one */
                if(!namelen){
                    //TODO:in my program ,it no problem
                    name=0;
                    ntfs_attr->name=0;
                }else{
                    //TODO do not deal the name
                    /* 1 Unicode character fits in 2 bytes */
                    name=malloc(2*namelen);
                    if( !name ){
                        printf("\nntfs_load_attributes:malloc error!");
                        exit(0);
                    }
                    //Unicode 2400 4900 3300 3000 -> short int 24 49 33 3000
                    memcpy(name,attrdata+NTFS_GETU16(attrdata+10),2*namelen);
                    ntfs_attr->name=name;
                }
                ntfs_attr->namelen=namelen;
                printf("\nattribution type:%x",attr_type);
                ntfs_attr->type=attr_type;
                ntfs_attr->resident=(NTFS_GETU8(attrdata+8)==0);
                ntfs_attr->compressed=NTFS_GETU16(attrdata+0xC);
                ntfs_attr->attrno=NTFS_GETU16(attrdata+0xE);
                //printf("\the resident:%d",ntfs_attr->resident);
                /*如果是常驻属性*/
                if(ntfs_attr->resident) {
                    printf("\n:this is a resident attribution");
                    ntfs_attr->size=NTFS_GETU16(attrdata+0x10);
                    data=attrdata+NTFS_GETU16(attrdata+0x14);
                    ntfs_attr->d.data = (void*)malloc(ntfs_attr->size);
                    if( !ntfs_attr->d.data )
                        return -1;
                    memcpy(ntfs_attr->d.data,data,ntfs_attr->size);
                    ntfs_attr->indexed=NTFS_GETU16(attrdata+0x16);
                    printf("\nthe attr length:%d",ntfs_attr->size);
                }else{
                    printf("\n:this is a non-resident attribution");
                    ntfs_attr->allocated=NTFS_GETU32(attrdata+0x28);
                    ntfs_attr->size=NTFS_GETU32(attrdata+0x30);
                    ntfs_attr->initialized=NTFS_GETU32(attrdata+0x38);
                    ntfs_attr->cengine=NTFS_GETU16(attrdata+0x22);
                    if(ntfs_attr->compressed)
                        ntfs_attr->compsize=NTFS_GETU32(attrdata+0x40);
                    ino->attrs[ino->attr_count].d.r.runlist=0;
                    ino->attrs[ino->attr_count].d.r.len=0;
                    ntfs_process_runs(ino,ntfs_attr,attrdata);
                }
                ino->attr_count++;
            }
		}
		attrdata+=attr_len;
	}while(attr_type!=-1); /* attribute list ends with type -1 */


}

/*
 *author:liuyang
 *date  :2017/4/18
 *detail: 判断文件记录是否损坏
 *return
 */
 int ntfs_check_mft_record(ntfs_volume *vol, char *record, char *magic, int size){
	int count;
	//ntfs_u16 fixup;

	if(!IS_MAGIC(record,magic))
		return 0;
	//start=NTFS_GETU16(record+4);
	count=NTFS_GETU16(record+6);
	//printf("\ncount:%d",count);
	count--;
	if(size && vol->blocksize*count != size)
		return 0;
    /**增量中文件记录的第二个扇区最后两字节与fixup不一致*/
//	fixup = NTFS_GETU16(record+start);
//	start+=2;
//	offset=vol->blocksize-2;
//	while(count--){
//		if(NTFS_GETU16(record+offset)!=fixup)
//			return 0;
//		NTFS_PUTU16(record+offset, NTFS_GETU16(record+start));
//		start+=2;
//		offset+=vol->blocksize;
//	}
	return 1;
 }

/*
 *author:liuyang
 *date  :2017/4/16
 *detail: 初始化ntfs文件系统信息
 *return
 */
/* Get vital informations about the ntfs partition from the boot sector */
int ntfs_init_volume(ntfs_volume *vol,char *boot)
{
	/* Historical default values, in case we don't load $AttrDef */
	vol->at_standard_information=0x10;
	vol->at_attribute_list=0x20;
	vol->at_file_name=0x30;
	vol->at_volume_version=0x40;
	vol->at_security_descriptor=0x50;
	vol->at_volume_name=0x60;
	vol->at_volume_information=0x70;
	vol->at_data=0x80;
	vol->at_index_root=0x90;
	vol->at_index_allocation=0xA0;
	vol->at_bitmap=0xB0;
	vol->at_symlink=0xC0;

	/* Sector size */
	vol->blocksize=NTFS_GETU16(boot+0xB);
	vol->clusterfactor=NTFS_GETU8(boot+0xD);
	vol->mft_clusters_per_record=NTFS_GETS8(boot+0x40);
	vol->index_clusters_per_record=NTFS_GETS8(boot+0x44);

	/* Just some consistency checks */
	if(NTFS_GETU32(boot+0x40)>256){
		printf("Unexpected data #1 in boot block\n");
	}

	if(NTFS_GETU32(boot+0x44)>256){
        printf("Unexpected data #2 in boot block\n");
	}
	if(vol->index_clusters_per_record<0){
		printf("Unexpected data #3 in boot block\n");
		/* If this really means a fraction, setting it to 1
		   should be safe. */
		vol->index_clusters_per_record=1;
	}
	/* in some cases, 0xF6 meant 1024 bytes. Other strange values have not
	   been observed */
	if(vol->mft_clusters_per_record<0 && vol->mft_clusters_per_record!=-10)
		printf("Unexpected data #4 in boot block\n");

	vol->clustersize=vol->blocksize*vol->clusterfactor;
	if(vol->mft_clusters_per_record>0)
		vol->mft_recordsize=
			vol->clustersize*vol->mft_clusters_per_record;
	else
		vol->mft_recordsize=1<<(-vol->mft_clusters_per_record);
	vol->index_recordsize=vol->clustersize*vol->index_clusters_per_record;
	/* FIXME: long long value */
	vol->mft_cluster=NTFS_GETU64(boot+0x30);

	/* This will be initialized later */
	vol->upcase=0;
	vol->upcase_length=0;
	vol->mft_ino=0;
	return 0;
}
