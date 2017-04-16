#include "ntfs_main.h"

/*
 *author:liuyang
 *date  :2017/3/23
 *detail:由inode判断文件的位置,更新文件位置
 *return
 */
int ntfs_update_file_metadata(char *overlay_image_path,char *base_image_path,__U64_TYPE inodes[],int inode_count,char *overlay_id){
    ntfs_volume vol;
    FILE *bi_fp;

    int i;
    char ntfs_super_block[80];
    bi_fp=fopen(base_image_path,"r");
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
    printf("\nthe blocksize:%d\nclusterfactor:%d\n mft_cluster offset:%x\nmft_recordsize:%d,\nmft_clusters_per_record:%d",
           vol.blocksize,vol.clusterfactor,vol.mft_cluster,vol.mft_recordsize,vol.mft_clusters_per_record);
    for(i=0;i<inode_count;i++){

    }

    fclose(bi_fp);
    return 1;
fail:
    fclose(bi_fp);
    return -1;
}



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
