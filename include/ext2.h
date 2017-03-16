#ifndef __EXT2_H_INCLUDED
#define __EXT2_H_INCLUDED

#include <stdio.h>

/*
 * Structure of the super block
 */
 typedef unsigned char __U8_TYPE;
#define BG_DESC_SIZE 32
#define EXT2_N_BLOCKS 15

#define EXT_FIRST_EXTENT(__hdr__) \
	((struct ext4_extent *) (((char *) (__hdr__)) +		\
				 sizeof(struct ext4_extent_header)))
#define EXT_FIRST_INDEX(__hdr__) \
	((struct ext4_extent_idx *) (((char *) (__hdr__)) +	\
				     sizeof(struct ext4_extent_header)))

struct ext2_super_block {
	__U32_TYPE	s_inodes_count;		/* Inodes count */
	__U32_TYPE	s_blocks_count;		/* Blocks count */
	__U32_TYPE	s_r_blocks_count;	/* Reserved blocks count */
	__U32_TYPE	s_free_blocks_count;	/* Free blocks count */
	__U32_TYPE	s_free_inodes_count;	/* Free inodes count */
	__U32_TYPE	s_first_data_block;	/* First Data Block */
	__U32_TYPE	s_log_block_size;	/* Block size */
	__S32_TYPE	s_log_frag_size;	/* Fragment size */
	__U32_TYPE	s_blocks_per_group;	/* # Blocks per group */
	__U32_TYPE	s_frags_per_group;	/* # Fragments per group */
	__U32_TYPE	s_inodes_per_group;	/* # Inodes per group */
	__U32_TYPE	s_mtime;		/* Mount time */
	__U32_TYPE	s_wtime;		/* Write time */
	__U16_TYPE	s_mnt_count;		/* Mount count */
	__S16_TYPE	s_max_mnt_count;	/* Maximal mount count */
	__U16_TYPE	s_magic;		/* Magic signature */
	__U16_TYPE	s_state;		/* File system state */
	__U16_TYPE	s_errors;		/* Behaviour when detecting errors */
	__U16_TYPE	s_minor_rev_level; 	/* minor revision level */
	__U32_TYPE	s_lastcheck;		/* time of last check */
	__U32_TYPE	s_checkinterval;	/* max. time between checks */
	__U32_TYPE	s_creator_os;		/* OS */
	__U32_TYPE	s_rev_level;		/* Revision level */
	__U16_TYPE	s_def_resuid;		/* Default uid for reserved blocks */
	__U16_TYPE	s_def_resgid;		/* Default gid for reserved blocks */
	/*
	 * These fields are for EXT2_DYNAMIC_REV superblocks only.
	 *
	 * Note: the difference between the compatible feature set and
	 * the incompatible feature set is that if there is a bit set
	 * in the incompatible feature set that the kernel doesn't
	 * know about, it should refuse to mount the filesystem.
	 *
	 * e2fsck's requirements are more strict; if it doesn't know
	 * about a feature in either the compatible or incompatible
	 * feature set, it must abort and not try to meddle with
	 * things it doesn't understand...
	 */
	__U32_TYPE	s_first_ino; 		/* First non-reserved inode */
	__U16_TYPE   s_inode_size; 		/* size of inode structure */
	__U16_TYPE	s_block_group_nr; 	/* block group # of this superblock */
	__U32_TYPE	s_feature_compat; 	/* compatible feature set */
	__U32_TYPE	s_feature_incompat; 	/* incompatible feature set */
	__U32_TYPE	s_feature_ro_compat; 	/* readonly-compatible feature set */
	__U8_TYPE	s_uuid[16];		/* 128-bit uuid for volume */
	char	s_volume_name[16]; 	/* volume name */
	char	s_last_mounted[64]; 	/* directory where last mounted */
	__U32_TYPE	s_algorithm_usage_bitmap; /* For compression */
	/*
	 * Performance hints.  Directory preallocation should only
	 * happen if the EXT2_COMPAT_PREALLOC flag is on.
	 */
	__U8_TYPE	s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
	__U8_TYPE	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
	__U16_TYPE	s_padding1;
	__U32_TYPE	s_reserved[204];	/* Padding to the end of the block */
};
/*
 * Structure of a blocks group descriptor
 */
struct ext2_group_desc
{
	__U32_TYPE	bg_block_bitmap;		/* Blocks bitmap block */
	__U32_TYPE	bg_inode_bitmap;		/* Inodes bitmap block */
	__U32_TYPE	bg_inode_table;		/* Inodes table block */
	__U16_TYPE	bg_free_blocks_count;	/* Free blocks count */
	__U16_TYPE	bg_free_inodes_count;	/* Free inodes count */
	__U16_TYPE	bg_used_dirs_count;	/* Directories count */
	__U16_TYPE	bg_pad;
	__U32_TYPE	bg_reserved[3];
};

/*
 * Structure of an inode on the disk
 */
struct ext2_inode {
	__U16_TYPE	i_mode;		/* File mode */
	__U16_TYPE	i_uid;		/* Low 16 bits of Owner Uid */
	__U32_TYPE	i_size;		/* Size in bytes */
	__U32_TYPE	i_atime;	/* Access time */
	__U32_TYPE	i_ctime;	/* Creation time */
	__U32_TYPE	i_mtime;	/* Modification time */
	__U32_TYPE	i_dtime;	/* Deletion Time */
	__U16_TYPE	i_gid;		/* Low 16 bits of Group Id */
	__U16_TYPE	i_links_count;	/* Links count */
	__U32_TYPE	i_blocks;	/* Blocks count */
	__U32_TYPE	i_flags;	/* File flags */
	union {
		struct {
			__U32_TYPE  l_i_reserved1;
		} linux1;
		struct {
			__U32_TYPE  h_i_translator;
		} hurd1;
		struct {
			__U32_TYPE  m_i_reserved1;
		} masix1;
	} osd1;				/* OS dependent 1 */
	__U32_TYPE	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
	__U32_TYPE	i_generation;	/* File version (for NFS) */
	__U32_TYPE	i_file_acl;	/* File ACL */
	__U32_TYPE	i_dir_acl;	/* Directory ACL */
	__U32_TYPE	i_faddr;	/* Fragment address */
	union {
		struct {
			__U8_TYPE	l_i_frag;	/* Fragment number */
			__U8_TYPE	l_i_fsize;	/* Fragment size */
			__U16_TYPE	i_pad1;
			__U16_TYPE	l_i_uid_high;	/* these 2 fields    */
			__U16_TYPE	l_i_gid_high;	/* were reserved2[0] */
			__U16_TYPE	l_i_reserved2;
		} linux2;
		struct {
			__U8_TYPE	h_i_frag;	/* Fragment number */
			__U8_TYPE	h_i_fsize;	/* Fragment size */
			__U16_TYPE	h_i_mode_high;
			__U16_TYPE	h_i_uid_high;
			__U16_TYPE	h_i_gid_high;
			__U32_TYPE	h_i_author;
		} hurd2;
		struct {
			__U8_TYPE	m_i_frag;	/* Fragment number */
			__U8_TYPE	m_i_fsize;	/* Fragment size */
			__U16_TYPE	m_pad1;
			__U32_TYPE	m_i_reserved2[2];
		} masix2;
	} osd2;				/* OS dependent 2 */
};

/*
 * This is the extent on-disk structure.
 * It's used at the bottom of the tree.
 */
struct ext4_extent {
	__U32_TYPE	ee_block;	/* first logical block extent covers */
	__U16_TYPE	ee_len;		/* number of blocks covered by extent */
	__U16_TYPE	ee_start_hi;	/* high 16 bits of physical block */
	__U32_TYPE	ee_start_lo;	/* low 32 bits of physical block */
};

/*
 * This is index on-disk structure.
 * It's used at all the levels except the bottom.
 */
struct ext4_extent_idx {
	__U32_TYPE	ei_block;	/* index covers logical blocks from 'block' */
	__U32_TYPE	ei_leaf_lo;	/* pointer to the physical block of the next *
				 * level. leaf or next index could be there */
	__U16_TYPE	ei_leaf_hi;	/* high 16 bits of physical block */
	__U16_TYPE	ei_unused;
};

/*
 * Each block (leaves and indexes), even inode-stored has header.
 */
struct ext4_extent_header {
	__U16_TYPE	eh_magic;	/* probably will support different formats */
	__U16_TYPE	eh_entries;	/* number of valid entries */
	__U16_TYPE	eh_max;		/* capacity of store in entries */
	__U16_TYPE	eh_depth;	/* has tree real underlying blocks? */
	__U32_TYPE	eh_generation;	/* generation of the tree */
};



#endif // __EXT2_H_INCLUDED__
