#include "btrfs.h"
#include "screen.h"
#include "string.h"
#include "mm/heap.h"

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
