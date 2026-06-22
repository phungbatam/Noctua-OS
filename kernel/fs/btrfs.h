#ifndef BTRFS_H
#define BTRFS_H

#include <stdint.h>

/* Btrfs magic numbers */
#define BTRFS_MAGIC          "_BHRfS_M"
#define BTRFS_SUPER_INFO_OFFSET 0x10000
#define BTRFS_SUPER_INFO_SIZE 4096

/* Btrfs superblock structure */
typedef struct {
    uint8_t  csum[32];
    uint8_t  fsid[16];
    uint64_t bytenr;
    uint64_t flags;
    uint64_t magic;
    uint64_t generation;
    uint64_t root_level;
    uint64_t root;
    uint64_t chunk_root_level;
    uint64_t chunk_root;
    uint64_t log_root_level;
    uint64_t log_root;
    uint64_t log_root_transid;
    uint64_t log_root_bytenr;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t num_devices;
    uint32_t sectorsize;
    uint32_t nodesize;
    uint32_t leafsize;
    uint32_t stripesize;
    uint64_t chunk_root_generation;
    uint64_t compat_flags;
    uint64_t compat_ro_flags;
    uint64_t incompat_flags;
    uint16_t csum_type;
    uint8_t  dev_item_count;
    char     label[256];
} __attribute__((packed)) btrfs_super_block_t;

/* Btrfs key structure */
typedef struct {
    uint64_t objectid;
    uint8_t  type;
    uint64_t offset;
} __attribute__((packed)) btrfs_key_t;

/* Btrfs item structure */
typedef struct {
    uint32_t offset;
    uint32_t size;
} __attribute__((packed)) btrfs_item_t;

/* Btrfs header structure */
typedef struct {
    uint8_t  csum[32];
    uint8_t  fsid[16];
    uint64_t bytenr;
    uint64_t flags;
    uint8_t  level;
    uint8_t  chunk_tree_uuid[16];
} __attribute__((packed)) btrfs_header_t;

/* Btrfs tree node */
typedef struct {
    btrfs_header_t header;
    uint8_t data[];
} btrfs_node_t;

/* Btrfs path for tree traversal */
#define BTRFS_MAX_LEVEL 8
typedef struct {
    btrfs_node_t *nodes[BTRFS_MAX_LEVEL];
    int slots[BTRFS_MAX_LEVEL];
    int level;
} btrfs_path_t;

/* Btrfs filesystem state */
typedef struct {
    btrfs_super_block_t *super;
    int mounted;
    void *device_start;
    uint64_t device_size;
    btrfs_path_t *path;
} btrfs_fs_t;

/* Btrfs API */
int btrfs_mount(void *device_start, uint64_t device_size);
int btrfs_read_super(void *device_start, btrfs_super_block_t *sb);
int btrfs_check_magic(btrfs_super_block_t *sb);
int btrfs_read_tree_block(btrfs_fs_t *fs, uint64_t bytenr, btrfs_node_t **node);
int btrfs_search_slot(btrfs_fs_t *fs, btrfs_key_t *key, btrfs_path_t *path);
void btrfs_release_path(btrfs_path_t *path);
int btrfs_lookup_root_item(btrfs_fs_t *fs, uint64_t objectid, btrfs_key_t *key);

#endif
