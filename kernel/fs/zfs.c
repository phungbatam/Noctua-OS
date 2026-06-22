#include "zfs.h"
#include "screen.h"
#include "string.h"
#include "mm/heap.h"

static zfs_fs_t zfs_fs;

int zfs_check_magic(zfs_vdev_label_t *label) {
    if (!label) return -1;
    /* ZFS doesn't have a simple magic number in the label */
    /* We check for valid configuration offset instead */
    if (label->config_offset == 0 || label->config_offset > 0x1000000) {
        return -1;
    }
    return 0;
}

int zfs_mount(void *device_start, uint64_t device_size) {
    memset(&zfs_fs, 0, sizeof(zfs_fs));

    zfs_vdev_label_t *label = (zfs_vdev_label_t *)((uint8_t *)device_start + ZFS_LABEL_OFFSET);
    
    if (zfs_check_magic(label) < 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("ZFS: Invalid label structure\n");
        return -1;
    }
    
    zfs_fs.label = label;
    zfs_fs.device_start = device_start;
    zfs_fs.device_size = device_size;
    zfs_fs.mounted = 1;

    screen_set_content_color(C_INFO);
    screen_term_write("ZFS: Mounted - ");
    char buf[32];
    int2str(device_size / (1024*1024), buf);
    screen_term_write(buf);
    screen_term_write(" MB, GUID: ");
    /* Display GUID (simplified) */
    screen_term_write("0x");
    int2str(label->guid >> 32, buf);
    screen_term_write(buf);
    int2str(label->guid & 0xFFFFFFFF, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    return 0;
}
