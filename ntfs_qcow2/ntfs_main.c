#include "ntfs_main.h"

/*
 *author:liuyang
 *date  :2017/4/16
 *detail:由inode判断文件的位置,更新文件位置
 *return
 */
int ntfs_update_file_metadata(char *overlay_image_path,char *base_image_path,__U64_TYPE inode,int inode_count,char *overlay_id){
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
    ino.i_number=1072;//1072 80H non-resident
    ino.vol=&vol;
    ino.attr=malloc(vol.mft_recordsize);
    if(!ino.attr){
        printf("\nmalloc error:malloc ino.attr failed!");
        goto fail;
    }
    /********************************************************************/
    in_overlay=ntfs_inode_in_overlay(overlay_image_path,&ino);
    if(in_overlay==0){
        printf("\n*******************NTFS inode in backing image!");
    }else if(in_overlay==1){
        printf("\n*******************NTFS inode in overlay !");

        if(!ntfs_check_mft_record(ino.vol,ino.attr,"FILE",ino.vol->mft_recordsize))
        {
            printf("Invalid MFT record corruption");
            goto fail;
        }
        ino.sequence_number=NTFS_GETU16(ino.attr+0x10);
        ino.attr_count=0;
        ino.record_count=0;
        ino.records=0;
        ino.attrs=0;
        ntfs_load_attributes(&ino);
        //printf("\nthe runlist number:%d,cluster:%x",ino.attrs[1].d.r.len,ino.attrs[1].d.r.runlist[0].cluster);
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
                        printf("\n*******************:data in overlay not been allocated!");
                    }else if(in_overlay==1){
                        //TODO update file info
                        printf("\n*******************:data in overlay  been allocated!");
                    }else{
                        printf("\n*******************:ntfs_blockInOverlay ERROR!");
                    }
                }else{
                    printf("\nntfs_update_file_metadata:run list problem!");
                }

            }
        }
    }else{
        printf("\n*******************ntfs_in_overlay error!!!");
    }
    fclose(bi_fp);
    return 1;
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
	ntfs_attribute *alist;
	int datasize;
	int offset,len,delta;
	char *buf;
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
