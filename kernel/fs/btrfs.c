#include "btrfs.h"
#include "../ui/screen.h"
#include "../lib/string.h"
#include "../mm/heap.h"

static btrfs_fs_t btrfs_fs;

int btrfs_check_magic(btrfs_super_block_t *sb) {
    if (!sb) return -1;
    return memcmp(&sb->magic, BTRFS_MAGIC, 8) == 0 ? 0 : -1;
}

int btrfs_read_super(void *device_start, btrfs_super_block_t *sb) {
    if (!device_start || !sb) return -1;
    
    /* Read superblock from offset 0x10000 */
    uint8_t *sb_ptr = (uint8_t *)device_start + BTRFS_SUPER_INFO_OFFSET;
    memcpy(sb, sb_ptr, sizeof(btrfs_super_block_t));
    
    return 0;
}

int btrfs_read_tree_block(btrfs_fs_t *fs, uint64_t bytenr, btrfs_node_t **node) {
    if (!fs || !node) return -1;
    if (bytenr >= fs->device_size) return -1;

    uint32_t nodesize = fs->super->nodesize;
    btrfs_node_t *n = (btrfs_node_t *)kmalloc(nodesize);
    if (!n) return -1;

    uint8_t *src = (uint8_t *)fs->device_start + bytenr;
    memcpy(n, src, nodesize);

    /* Verify header checksum (simplified - just check level is reasonable) */
    if (n->header.level > BTRFS_MAX_LEVEL) {
        kfree(n);
        return -1;
    }

    *node = n;
    return 0;
}

void btrfs_release_path(btrfs_path_t *path) {
    if (!path) return;

    for (int i = 0; i < BTRFS_MAX_LEVEL; i++) {
        if (path->nodes[i]) {
            kfree(path->nodes[i]);
            path->nodes[i] = 0;
        }
        path->slots[i] = 0;
    }
    path->level = 0;
}

int btrfs_search_slot(btrfs_fs_t *fs, btrfs_key_t *key, btrfs_path_t *path) {
    if (!fs || !key || !path) return -1;

    btrfs_release_path(path);

    uint64_t root_bytenr = fs->super->root;
    int level = fs->super->root_level;

    path->level = level;

    /* Read root node */
    if (btrfs_read_tree_block(fs, root_bytenr, &path->nodes[level]) < 0) {
        return -1;
    }

    /* Traverse down the tree */
    while (level > 0) {
        btrfs_node_t *node = path->nodes[level];
        int slot = 0;
        int found = 0;

        /* Binary search for the key */
        int low = 0;
        int high = 0; /* Need to get item count from header */

        /* Linear search for now (simplified) */
        for (slot = 0; slot < 10; slot++) {
            /* In a real implementation, we would compare keys here */
            /* For now, just take the first slot */
            break;  
        }

        path->slots[level] = slot;
        level--;

        /* Read child node */
        uint64_t child_bytenr = 0; /* Would extract from node pointer */
        if (btrfs_read_tree_block(fs, child_bytenr, &path->nodes[level]) < 0) {
            btrfs_release_path(path);
            return -1;
        }

        path->level = level;
    }

    return 0;
}

int btrfs_lookup_root_item(btrfs_fs_t *fs, uint64_t objectid, btrfs_key_t *key) {
    if (!fs || !key) return -1;

    /* Simplified root lookup - would search the root tree */
    key->objectid = objectid;
    key->type = 0; /* ROOT_ITEM */
    key->offset = 0;

    return 0;
}

int btrfs_mount(void *device_start, uint64_t device_size) {
    memset(&btrfs_fs, 0, sizeof(btrfs_fs));

    btrfs_super_block_t *sb = (btrfs_super_block_t *)kmalloc(sizeof(btrfs_super_block_t));
    if (!sb) return -1;

    if (btrfs_read_super(device_start, sb) < 0) {
        kfree(sb);
        screen_set_content_color(C_ERROR);
        screen_term_write("Btrfs: Failed to read superblock\n");
        return -1;
    }

    if (btrfs_check_magic(sb) < 0) {
        kfree(sb);
        screen_set_content_color(C_ERROR);
        screen_term_write("Btrfs: Invalid magic number\n");
        return -1;
    }

    btrfs_fs.super = sb;
    btrfs_fs.device_start = device_start;
    btrfs_fs.device_size = device_size;
    btrfs_fs.mounted = 1;

    /* Allocate path for tree traversal */
    btrfs_fs.path = (btrfs_path_t *)kmalloc(sizeof(btrfs_path_t));
    if (btrfs_fs.path) {
        memset(btrfs_fs.path, 0, sizeof(btrfs_path_t));
    }

    screen_set_content_color(C_INFO);
    screen_term_write("Btrfs: Mounted - ");
    char buf[32];
    int2str(sb->total_bytes / (1024*1024), buf);
    screen_term_write(buf);
    screen_term_write(" MB, nodesize: ");
    int2str(sb->nodesize, buf);
    screen_term_write(buf);
    screen_term_write(" bytes\n");

    return 0;
}
