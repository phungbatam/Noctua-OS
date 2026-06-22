#ifndef TVN_EXT2_H
#define TVN_EXT2_H

#include <stdint.h>

/* Ext2 superblock constants */
#define EXT2_SUPER_MAGIC    0xEF53
#define EXT2_ROOT_INO       2

/* Ext3/4 feature flags */
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL 0x00000004
#define EXT3_FEATURE_INCOMPAT_RECOVER   0x00000004
#define EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001

/* EXT4 specific features */
#define EXT4_FEATURE_COMPAT_EXTENTS    0x00000040
#define EXT4_FEATURE_INCOMPAT_64BIT     0x00000080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG  0x00000200

/* EXT4 extent structure */
typedef struct {
    uint32_t e_block;
    uint16_t e_len;
    uint16_t e_start_hi;
    uint32_t e_start_lo;
} __attribute__((packed)) ext4_extent_t;

/* EXT4 extent header */
typedef struct {
    uint16_t e_magic;
    uint16_t e_entries;
    uint16_t e_max;
    uint16_t e_depth;
    uint32_t e_generation;
} __attribute__((packed)) ext4_extent_header_t;

#define EXT4_EXT_MAGIC 0xF30A

/* Journal superblock */
typedef struct {
    uint32_t s_header;
    uint32_t s_blocksize;
    uint32_t s_maxlen;
    uint32_t s_first;
    uint32_t s_sequence;
    uint32_t s_start;
} __attribute__((packed)) journal_superblock_t;

#define JBD2_MAGIC_NUMBER 0xC03B3998

/* Ext2 superblock structure */
typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    /* Extended fields for revision 1+ */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    uint8_t  s_volume_name[16];
    uint8_t  s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_reserved_1;
    uint32_t s_mount_opts;
    uint32_t s_raid_stride;
    uint8_t  s_reserved[820];
} __attribute__((packed)) ext2_superblock_t;

/* Ext2 block group descriptor */
typedef struct {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed)) ext2_bg_desc_t;

/* Ext2 inode */
typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} __attribute__((packed)) ext2_inode_t;

/* Ext2 directory entry */
typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[255];
} __attribute__((packed)) ext2_dir_entry_t;

/* Inode modes */
#define EXT2_S_IFMT     0xF000
#define EXT2_S_IFSOCK   0xC000
#define EXT2_S_IFLNK    0xA000
#define EXT2_S_IFREG    0x8000
#define EXT2_S_IFBLK    0x6000
#define EXT2_S_IFDIR    0x4000
#define EXT2_S_IFCHR    0x2000
#define EXT2_S_IFIFO    0x1000

/* File types in directory entry */
#define EXT2_FT_UNKNOWN 0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

/* Block size */
#define EXT2_BLOCK_SIZE(sb) (1024 << (sb)->s_log_block_size)
#define EXT2_BLOCK_GROUP(sb,ino) (((ino)-1) / (sb)->s_inodes_per_group)
#define EXT2_INODE_INDEX(sb,ino) (((ino)-1) % (sb)->s_inodes_per_group)

/* Ext2 state */
typedef struct {
    ext2_superblock_t *sb;
    uint32_t block_size;
    int mounted;
    void *device_start;
    uint32_t device_size;
    uint32_t num_block_groups;
    ext2_bg_desc_t *block_groups;
    int has_journal;  /* EXT3 journaling support */
    uint32_t journal_inode;
    int has_extents;  /* EXT4 extent support */
    int is_64bit;     /* EXT4 64-bit support */
} ext2_fs_t;

int ext2_mount(void *device_start, uint32_t device_size);
int ext2_read_inode(uint32_t inode_num, ext2_inode_t *inode);
int ext2_read_block(uint32_t block_num, void *buffer);
int ext2_read_inode_block(ext2_inode_t *inode, uint32_t block_idx, void *buffer);
uint32_t ext2_find_file(const char *path);
void ext2_list_dir(uint32_t inode_num);
int ext2_read_file(uint32_t inode_num, uint32_t offset, uint32_t size, void *buffer);

/* EXT3 journaling functions */
int ext3_journal_init(void);
int ext3_journal_recover(void);

/* EXT4 extent functions */
int ext4_read_extent(ext2_inode_t *inode, uint32_t block_idx, void *buffer);

#endif
