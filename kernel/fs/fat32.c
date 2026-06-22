#include "fat32.h"
#include "blockdev.h"
#include "string.h"
#include "heap.h"

/* Static storage for VFS */
static vfs_node_t vfs_nodes[MAX_VFS_NODES];
static int vfs_node_count = 0;
static vfs_node_t *root_node = 0;

static dentry_t dentry_cache[MAX_DENTRY_CACHE];
static int dentry_count = 0;

static mount_t mount_table[MAX_MOUNTS];

/* File handles */
static file_handle_t file_handles[MAX_OPEN_FILES];
static int file_handle_count = 0;

/* FAT state */
static block_dev_t *fat_bdev = 0;
static fat_boot_sector_t boot_sector_copy;
static uint8_t *fat_table = 0;
static uint32_t fat_size = 0;
static uint32_t fat_sectors = 0;
static uint32_t data_start = 0;
static uint32_t reserved_sectors = 0;
static uint32_t num_fats = 0;
static uint32_t sectors_per_cluster = 0;
static uint32_t bytes_per_cluster = 0;
static int fat_type = FAT_TYPE_FAT32; /* Default to FAT32 */

static int fat_read_sectors(uint64_t lba, void *buffer, uint32_t count) {
    if (!fat_bdev) return -1;
    return fat_bdev->read(fat_bdev->priv, lba, buffer, count);
}

static int fat_read_sector(uint64_t lba, void *buffer) {
    return fat_read_sectors(lba, buffer, 1);
}

/* Detect FAT type from boot sector */
int fat_detect_type(fat_boot_sector_t *bs) {
    uint32_t total_sectors = bs->total_sectors_16 ? bs->total_sectors_16 : bs->total_sectors_32;
    uint32_t fat_size = bs->fat_size_16 ? bs->fat_size_16 : bs->fat_size_32;
    uint32_t root_dir_sectors = ((bs->root_entry_count * 32) + (bs->bytes_per_sector - 1)) / bs->bytes_per_sector;
    uint32_t data_sectors = total_sectors - (bs->reserved_sectors + (bs->num_fats * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / bs->sectors_per_cluster;

    if (cluster_count < 4085) {
        return FAT_TYPE_FAT12;
    } else if (cluster_count < 65525) {
        return FAT_TYPE_FAT16;
    } else {
        return FAT_TYPE_FAT32;
    }
}

/* Initialize FAT filesystem (generic for FAT12/16/32) */
int fat_init(struct block_dev *bdev) {
    fat_bdev = bdev;

    root_node = &vfs_nodes[vfs_node_count++];
    strcpy(root_node->name, "root");
    root_node->size = 0;
    root_node->is_directory = 1;
    root_node->attributes = FAT_ATTR_DIRECTORY;
    root_node->permissions = VFS_PERM_OWNER_READ | VFS_PERM_OWNER_WRITE | VFS_PERM_OWNER_EXEC | VFS_PERM_GROUP_READ | VFS_PERM_GROUP_EXEC | VFS_PERM_OTHER_READ | VFS_PERM_OTHER_EXEC;
    root_node->uid = 0;
    root_node->gid = 0;
    root_node->create_time = 0;
    root_node->modify_time = 0;
    root_node->parent = 0;
    root_node->children = 0;
    root_node->next = 0;
    root_node->first_cluster = 0;
    vfs_setup_operations(root_node);

    if (!bdev) {
        /* Virtual mode - no real disk */
        vfs_create_node("/bin", 1);
        vfs_create_node("/home", 1);
        vfs_create_node("/home/user", 1);
        vfs_create_node("/system", 1);
        vfs_create_node("/tmp", 1);
        vfs_create_node("/dev", 1);
        vfs_create_node("/etc", 1);

        vfs_node_t *readme = vfs_create_node("/home/user/readme.txt", 0);
        if (readme) {
            const char *welcome = "Chao mung ban den voi TVN_OS!\n";
            readme->size = strlen(welcome);
            vfs_setup_operations(readme);
        }
        return 0;
    }

    /* Read boot sector from disk */
    if (fat_read_sector(0, &boot_sector_copy) < 0)
        return -1;

    if (boot_sector_copy.bytes_per_sector != FAT_SECTOR_SIZE)
        return -1;

    /* Detect FAT type */
    fat_type = fat_detect_type(&boot_sector_copy);
    
    sectors_per_cluster = boot_sector_copy.sectors_per_cluster;
    bytes_per_cluster = sectors_per_cluster * FAT_SECTOR_SIZE;
    reserved_sectors = boot_sector_copy.reserved_sectors;
    num_fats = boot_sector_copy.num_fats;

    /* Set FAT size based on type */
    if (fat_type == FAT_TYPE_FAT32) {
        fat_sectors = boot_sector_copy.fat_size_32;
    } else {
        fat_sectors = boot_sector_copy.fat_size_16;
    }
    fat_size = fat_sectors * FAT_SECTOR_SIZE;

    data_start = reserved_sectors + (num_fats * fat_sectors);
    
    /* FAT12/16 have fixed root directory, FAT32 uses cluster */
    if (fat_type == FAT_TYPE_FAT32) {
        root_node->first_cluster = boot_sector_copy.root_cluster;
    } else {
        root_node->first_cluster = 0; /* Fixed root directory */
        /* Add root directory sectors to data_start */
        uint32_t root_dir_sectors = ((boot_sector_copy.root_entry_count * 32) + (boot_sector_copy.bytes_per_sector - 1)) / boot_sector_copy.bytes_per_sector;
        data_start += root_dir_sectors;
    }

    /* Read FAT table into memory */
    fat_table = (uint8_t *)kmalloc(fat_size);
    if (!fat_table) return -1;

    if (fat_read_sectors(reserved_sectors, fat_table, fat_sectors) < 0) {
        kfree(fat_table);
        fat_table = 0;
        return -1;
    }

    return 0;
}

/* Read a cluster from disk */
int fat_read_cluster(uint32_t cluster, void *buffer) {
    if (!buffer || cluster < 2 || !fat_bdev) return -1;

    uint64_t cluster_lba = data_start + (uint64_t)(cluster - 2) * sectors_per_cluster;

    return fat_read_sectors(cluster_lba, buffer, sectors_per_cluster);
}

/* Write a cluster to disk */
int fat_write_cluster(uint32_t cluster, const void *buffer) {
    if (!buffer || cluster < 2 || !fat_bdev) return -1;

    uint64_t cluster_lba = data_start + (uint64_t)(cluster - 2) * sectors_per_cluster;

    return fat_bdev->write(fat_bdev->priv, cluster_lba, buffer, sectors_per_cluster);
}

/* Get next cluster in chain (handles FAT12/16/32) */
uint32_t fat_get_next_cluster(uint32_t cluster) {
    if (cluster < 2 || !fat_table) return 0xFFFFFFFF;

    uint32_t fat_entry;
    
    if (fat_type == FAT_TYPE_FAT32) {
        fat_entry = ((uint32_t *)fat_table)[cluster];
    } else if (fat_type == FAT_TYPE_FAT16) {
        fat_entry = ((uint16_t *)fat_table)[cluster];
    } else { /* FAT12 */
        uint32_t offset = cluster + (cluster / 2);
        uint16_t fat_value = ((uint16_t *)fat_table)[offset];
        if (cluster & 1) {
            fat_entry = fat_value >> 4;
        } else {
            fat_entry = fat_value & 0x0FFF;
        }
    }

    if (fat_entry >= 0xFFFFFF8) return 0xFFFFFFFF;

    return fat_entry;
}

/* Set next cluster in chain (handles FAT12/16/32) */
int fat_set_next_cluster(uint32_t cluster, uint32_t next) {
    if (cluster < 2 || !fat_table) return -1;

    if (fat_type == FAT_TYPE_FAT32) {
        ((uint32_t *)fat_table)[cluster] = next;
    } else if (fat_type == FAT_TYPE_FAT16) {
        ((uint16_t *)fat_table)[cluster] = next;
    } else { /* FAT12 */
        uint32_t offset = cluster + (cluster / 2);
        uint16_t fat_value = ((uint16_t *)fat_table)[offset];
        if (cluster & 1) {
            fat_value = (fat_value & 0x000F) | ((next & 0x0FFF) << 4);
        } else {
            fat_value = (fat_value & 0xF000) | (next & 0x0FFF);
        }
        ((uint16_t *)fat_table)[offset] = fat_value;
    }

    return 0;
}

/* Find a free cluster (handles FAT12/16/32) */
uint32_t fat_get_free_cluster(void) {
    if (!fat_table) return 0;

    uint32_t num_entries;
    if (fat_type == FAT_TYPE_FAT32) {
        num_entries = fat_size / 4;
    } else if (fat_type == FAT_TYPE_FAT16) {
        num_entries = fat_size / 2;
    } else { /* FAT12 */
        num_entries = (fat_size * 2) / 3; /* Approximate */
    }

    for (uint32_t i = 2; i < num_entries; i++) {
        uint32_t entry = fat_get_next_cluster(i);
        if (entry == 0) return i;
    }

    return 0;
}

/* VFS - Get root node */
vfs_node_t *vfs_get_root(void) {
    return root_node;
}

/* Find a node by path */
vfs_node_t *vfs_find_node(const char *path) {
    if (!path || !root_node) return 0;
    
    /* Start from root */
    vfs_node_t *current = root_node;
    
    /* Skip leading slash */
    if (path[0] == '/') path++;
    
    char path_copy[FAT_MAX_PATH];
    strcpy(path_copy, path);
    char *token = path_copy;
    while (*token && current) {
        /* Bỏ qua các dấu '/' liên tiếp (tương tự strtok) */
        while (*token == '/') token++;
        if (!*token) break;

        char *end = token;
        while (*end && *end != '/') end++;
        
        char orig = *end;
        *end = 0; /* Ngắt chuỗi tại dấu '/' */
        
        /* Search in children */
        vfs_node_t *child = current->children;
        vfs_node_t *found = 0;
        while (child) {
            if (strcmp(child->name, token) == 0) {
                found = child;
                break;
            }
            child = child->next;
        }
        if (!found) return 0; /* Not found */
        current = found;
        
        if (orig == 0) break;
        token = end + 1;
    }
    
    return current;
}

/* Create a new node */
vfs_node_t *vfs_create_node(const char *path, uint8_t is_directory) {
    if (!path || vfs_node_count >= MAX_VFS_NODES) return 0;
    
    /* Extract parent path and name */
    char parent_path[FAT_MAX_PATH];
    char name[FAT_MAX_FILENAME];
    
    const char *last_slash = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') last_slash = p; /* Handle both / and \ */
    }
    
    if (!last_slash) {
        strcpy(parent_path, "/");
        strcpy(name, path);
    } else {
        int len = last_slash - path;
        for (int i = 0; i < len; i++) parent_path[i] = path[i];
        parent_path[len] = 0;
        strcpy(name, last_slash + 1);
    }
    
    /* Find parent */
    vfs_node_t *parent = vfs_find_node(parent_path);
    if (!parent) return 0;
    
    /* Check if already exists */
    vfs_node_t *child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child; /* Already exists */
        }
        child = child->next;
    }
    
    /* Create new node */
    vfs_node_t *new_node = &vfs_nodes[vfs_node_count++];
    strcpy(new_node->name, name);
    new_node->size = 0;
    new_node->is_directory = is_directory;
    new_node->attributes = is_directory ? FAT_ATTR_DIRECTORY : FAT_ATTR_ARCHIVE;
    new_node->parent = parent;
    new_node->children = 0;
    new_node->next = 0;
    new_node->first_cluster = 0;
    vfs_setup_operations(new_node);
    
    /* Add to parent's children list */
    if (!parent->children) {
        parent->children = new_node;
    } else {
        vfs_node_t *last = parent->children;
        while (last->next) last = last->next;
        last->next = new_node;
    }

    dentry_add(parent, new_node);
    
    return new_node;
}

/* Delete a node */
int vfs_delete_node(const char *path) {
    vfs_node_t *node = vfs_find_node(path);
    if (!node) return -1;
    
    if (node->children && node->is_directory) {
        return -1; /* Directory not empty */
    }
    
    vfs_node_t *parent = node->parent;
    if (!parent) return -1;
    
    /* Remove from parent's children list */
    if (parent->children == node) {
        parent->children = node->next;
    } else {
        vfs_node_t *prev = parent->children;
        while (prev && prev->next != node) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = node->next;
        }
    }
    
    return 0;
}

/* List directory contents */
vfs_node_t *vfs_list_directory(vfs_node_t *dir) {
    if (!dir || !dir->is_directory) return 0;
    return dir->children;
}

/* File operations */
file_handle_t *file_open(const char *path, uint8_t write_mode) {
    (void)write_mode;  /* <-- Thêm dòng này để tắt warning */
    if (!path || file_handle_count >= MAX_OPEN_FILES) return 0;
    
    vfs_node_t *node = vfs_find_node(path);
    if (!node) {
        /* Try to create the file */
        node = vfs_create_node(path, 0);
        if (!node) return 0;
    }
    
    if (node->is_directory) return 0;
    
    file_handle_t *handle = &file_handles[file_handle_count++];
    handle->node = node;
    handle->position = 0;
    handle->is_open = 1;
    
    return handle;
}

int file_read(file_handle_t *file, void *buffer, uint32_t size) {
    if (!file || !file->is_open || !buffer) return -1;

    vfs_node_t *node = file->node;
    if (!node->f_op || !node->f_op->read) return -1;

    if (file->position + size > node->size)
        size = node->size - file->position;

    int ret = node->f_op->read(node, file->position, size, buffer);
    if (ret > 0) file->position += ret;
    return ret;
}

int file_write(file_handle_t *file, const void *buffer, uint32_t size) {
    if (!file || !file->is_open || !buffer) return -1;

    vfs_node_t *node = file->node;
    if (!node->f_op || !node->f_op->write) return -1;

    int ret = node->f_op->write(node, file->position, size, buffer);
    if (ret > 0) file->position += ret;
    return ret;
}

int file_close(file_handle_t *file) {
    if (!file) return -1;
    
    file->is_open = 0;
    file->node = 0;
    file->position = 0;
    
    return 0;
}

uint32_t file_seek(file_handle_t *file, uint32_t position) {
    if (!file || !file->is_open) return 0;
    
    vfs_node_t *node = file->node;
    if (position > node->size) position = node->size;
    
    file->position = position;
    return position;
}

uint32_t file_tell(file_handle_t *file) {
    if (!file || !file->is_open) return 0;
    return file->position;
}

/* ---- Vtable operations (Linux-style) ---- */

static int fat_file_open(struct vfs_node *node) {
    if (!node) return -1;
    return 0;
}

static int fat_file_close(struct vfs_node *node) {
    if (!node) return -1;
    return 0;
}

static int fat_file_read(struct vfs_node *node, uint32_t offset, uint32_t size, void *buffer) {
    if (!node || !buffer || !bytes_per_cluster) return -1;
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    if (node->first_cluster && size > 0) {
        uint32_t cluster = node->first_cluster;
        uint32_t pos = offset;
        uint32_t buf_pos = 0;
        uint8_t *temp = (uint8_t *)kmalloc(bytes_per_cluster);
        if (!temp) return -1;

        while (pos < offset + size && cluster < 0xFFFFFF8) {
            fat_read_cluster(cluster, temp);
            uint32_t cluster_off = pos % bytes_per_cluster;
            uint32_t to_read = bytes_per_cluster - cluster_off;
            if (to_read > offset + size - pos) to_read = offset + size - pos;
            memcpy((uint8_t *)buffer + buf_pos, temp + cluster_off, to_read);
            buf_pos += to_read;
            pos += to_read;
            cluster = fat_get_next_cluster(cluster);
        }
        kfree(temp);
    }

    return size;
}

static int fat_file_write(struct vfs_node *node, uint32_t offset, uint32_t size, const void *buffer) {
    if (!node || !buffer || !bytes_per_cluster) return -1;

    uint32_t new_size = offset + size;
    if (new_size > node->size) node->size = new_size;

    if (node->first_cluster && size > 0 && fat_bdev) {
        uint32_t cluster = node->first_cluster;
        uint32_t pos = offset;
        uint32_t buf_pos = 0;
        uint8_t *temp = (uint8_t *)kmalloc(bytes_per_cluster);
        if (!temp) return -1;

        while (pos < offset + size && cluster < 0xFFFFFF8) {
            uint32_t cluster_off = pos % bytes_per_cluster;
            uint32_t to_write = bytes_per_cluster - cluster_off;
            if (to_write > offset + size - pos) to_write = offset + size - pos;

            if (to_write == bytes_per_cluster) {
                memcpy(temp, (uint8_t *)buffer + buf_pos, to_write);
            } else {
                fat_read_cluster(cluster, temp);
                memcpy(temp + cluster_off, (uint8_t *)buffer + buf_pos, to_write);
            }
            fat_write_cluster(cluster, temp);

            buf_pos += to_write;
            pos += to_write;
            cluster = fat_get_next_cluster(cluster);
        }
        kfree(temp);
    }

    return size;
}

static int fat_readdir(struct vfs_node *node, uint32_t index, struct vfs_node **child) {
    if (!node || !node->is_directory || !child) return -1;
    struct vfs_node *c = node->children;
    uint32_t i = 0;
    while (c) {
        if (i == index) { *child = c; return 0; }
        i++;
        c = c->next;
    }
    return -1;
}

static int fat_lookup(struct vfs_node *parent, const char *name, struct vfs_node **result) {
    if (!parent || !name || !result) return -1;
    for (struct vfs_node *c = parent->children; c; c = c->next) {
        if (strcmp(c->name, name) == 0) { *result = c; return 0; }
    }
    return -1;
}

static uint32_t str_hash(const char *s) {
    uint32_t h = 0;
    while (*s) { h = h * 33 + (uint8_t)*s++; }
    return h;
}

void vfs_mount(const char *path, const char *device, const char *fs_type) {
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (!mount_table[i].used) {
            int j;
            for (j = 0; path[j] && j < (int)sizeof(mount_table[i].mount_point) - 1; j++)
                mount_table[i].mount_point[j] = path[j];
            mount_table[i].mount_point[j] = 0;
            for (j = 0; device[j] && j < (int)sizeof(mount_table[i].device) - 1; j++)
                mount_table[i].device[j] = device[j];
            mount_table[i].device[j] = 0;
            for (j = 0; fs_type[j] && j < (int)sizeof(mount_table[i].fs_type) - 1; j++)
                mount_table[i].fs_type[j] = fs_type[j];
            mount_table[i].fs_type[j] = 0;
            mount_table[i].used = 1;
            return;
        }
    }
}

int vfs_umount(const char *path) {
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].used && strcmp(mount_table[i].mount_point, path) == 0) {
            mount_table[i].used = 0;
            return 0;
        }
    }
    return -1;
}

vfs_node_t *vfs_resolve(const char *path) {
    if (!path || path[0] == 0) return vfs_get_root();
    if (path[0] == '/') return vfs_find_node(path);
    char full[FAT_MAX_PATH];
    full[0] = '/';
    int i = 1, j = 0;
    while (path[j] && i < (int)sizeof(full) - 1) full[i++] = path[j++];
    full[i] = 0;
    return vfs_find_node(full);
}

dentry_t *dentry_lookup(const char *name, vfs_node_t *parent) {
    if (!name || !parent) return 0;
    uint32_t h = str_hash(name);
    uint32_t ph = (uint32_t)(uintptr_t)parent;
    for (int i = 0; i < dentry_count; i++) {
        if (dentry_cache[i].hash == h &&
            dentry_cache[i].parent_hash == ph &&
            strcmp(dentry_cache[i].name, name) == 0) {
            return &dentry_cache[i];
        }
    }
    return 0;
}

void dentry_add(vfs_node_t *parent, vfs_node_t *child) {
    if (!parent || !child || dentry_count >= MAX_DENTRY_CACHE) return;
    int i;
    for (i = 0; child->name[i] && i < (int)sizeof(dentry_cache[dentry_count].name) - 1; i++)
        dentry_cache[dentry_count].name[i] = child->name[i];
    dentry_cache[dentry_count].name[i] = 0;
    dentry_cache[dentry_count].hash = str_hash(child->name);
    dentry_cache[dentry_count].parent_hash = (uint32_t)(uintptr_t)parent;
    dentry_cache[dentry_count].node = child;
    dentry_cache[dentry_count].next = 0;
    dentry_count++;
}

/* Gán vtable operations cho node */
void vfs_setup_operations(vfs_node_t *node) {
    if (!node) return;

    static file_operations_t def_fops = {
        fat_file_open, fat_file_close,
        fat_file_read, fat_file_write,
        fat_readdir
    };

    static inode_operations_t def_iops = {
        0,  /* create */
        0,  /* unlink */
        0,  /* mkdir */
        0,  /* rmdir */
        fat_lookup
    };

    node->f_op = &def_fops;
    node->i_op = &def_iops;
}
