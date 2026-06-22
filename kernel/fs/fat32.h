#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

/* FAT constants */
#define FAT_SECTOR_SIZE 512
#define FAT_MAX_FILENAME 256
#define FAT_MAX_PATH 256

/* FAT types */
#define FAT_TYPE_FAT12 0
#define FAT_TYPE_FAT16 1
#define FAT_TYPE_FAT32 2

/* FAT Boot Sector structure (common for FAT12/16/32) */
typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    /* FAT32 specific fields */
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  ext_boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat_boot_sector_t;

/* FAT Directory Entry structure (common for FAT12/16/32) */
typedef struct {
    uint8_t  name[11];
    uint8_t  attributes;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat_dir_entry_t;

/* File attributes */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LONG_NAME  0x0F

/* Forward declarations */
struct vfs_node;
struct block_dev;

/* Vtable kiểu Linux: file_operations, inode_operations, super_operations */
typedef struct file_operations {
    int (*open)(struct vfs_node *node);
    int (*close)(struct vfs_node *node);
    int (*read)(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer);
    int (*write)(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer);
    int (*readdir)(struct vfs_node *node, uint32_t index, struct vfs_node **child);
} file_operations_t;

typedef struct inode_operations {
    int (*create)(struct vfs_node *parent, const char *name, uint8_t is_dir);
    int (*unlink)(struct vfs_node *parent, const char *name);
    int (*mkdir)(struct vfs_node *parent, const char *name);
    int (*rmdir)(struct vfs_node *parent, const char *name);
    int (*lookup)(struct vfs_node *parent, const char *name, struct vfs_node **result);
} inode_operations_t;

/* Permission bits */
#define VFS_PERM_OTHER_EXEC  0x001
#define VFS_PERM_OTHER_WRITE 0x002
#define VFS_PERM_OTHER_READ  0x004
#define VFS_PERM_GROUP_EXEC  0x008
#define VFS_PERM_GROUP_WRITE 0x010
#define VFS_PERM_GROUP_READ  0x020
#define VFS_PERM_OWNER_EXEC  0x040
#define VFS_PERM_OWNER_WRITE 0x080
#define VFS_PERM_OWNER_READ  0x100
#define VFS_PERM_SETUID      0x400
#define VFS_PERM_SETGID      0x200

/* Virtual file system node */
typedef struct vfs_node {
    char name[FAT_MAX_FILENAME];
    uint32_t size;
    uint8_t  is_directory;
    uint8_t  attributes;
    uint16_t permissions;
    uint16_t uid;
    uint16_t gid;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    uint32_t first_cluster;
    uint32_t create_time;
    uint32_t modify_time;
    file_operations_t *f_op;
    inode_operations_t *i_op;
} vfs_node_t;

/* File handle */
typedef struct {
    vfs_node_t *node;
    uint32_t position;
    uint8_t  is_open;
} file_handle_t;

#define MAX_OPEN_FILES 16
#define MAX_VFS_NODES 128
#define MAX_DENTRY_CACHE 32
#define MAX_MOUNTS 8

typedef struct dentry {
    char     name[FAT_MAX_FILENAME];
    uint32_t hash;
    uint32_t parent_hash;
    vfs_node_t *node;
    struct dentry *next;
} dentry_t;

typedef struct mount {
    char     mount_point[FAT_MAX_PATH];
    char     device[32];
    char     fs_type[16];
    int      used;
} mount_t;

void vfs_mount(const char *path, const char *device, const char *fs_type);
int  vfs_umount(const char *path);
vfs_node_t *vfs_resolve(const char *path);
dentry_t *dentry_lookup(const char *name, vfs_node_t *parent);
void dentry_add(vfs_node_t *parent, vfs_node_t *child);

/* FAT API (generic for FAT12/16/32) */
int fat_init(struct block_dev *bdev);
int fat_detect_type(fat_boot_sector_t *bs);
int fat_read_cluster(uint32_t cluster, void *buffer);
int fat_write_cluster(uint32_t cluster, const void *buffer);
uint32_t fat_get_next_cluster(uint32_t cluster);
int fat_set_next_cluster(uint32_t cluster, uint32_t next);
uint32_t fat_get_free_cluster(void);

/* VFS API */
vfs_node_t *vfs_get_root(void);
vfs_node_t *vfs_find_node(const char *path);
vfs_node_t *vfs_create_node(const char *path, uint8_t is_directory);
int vfs_delete_node(const char *path);
vfs_node_t *vfs_list_directory(vfs_node_t *dir);

/* File API (dùng vtable) */
file_handle_t *file_open(const char *path, uint8_t write_mode);
int file_read(file_handle_t *file, void *buffer, uint32_t size);
int file_write(file_handle_t *file, const void *buffer, uint32_t size);
int file_close(file_handle_t *file);
uint32_t file_seek(file_handle_t *file, uint32_t position);
uint32_t file_tell(file_handle_t *file);

/* Gán vtable cho VFS node */
void vfs_setup_operations(vfs_node_t *node);

/* Alias */
#define vfs_lookup(path) vfs_find_node(path)

#endif