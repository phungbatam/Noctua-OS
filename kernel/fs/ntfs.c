#include "ntfs.h"
#include "screen.h"
#include "string.h"
#include "mm/heap.h"

static ntfs_fs_t ntfs_fs;

int ntfs_mount(void *device_start, uint64_t device_size) {
    memset(&ntfs_fs, 0, sizeof(ntfs_fs));

    ntfs_boot_sector_t *boot = (ntfs_boot_sector_t *)device_start;

    /* Check for NTFS signature in OEM ID */
    if (memcmp(boot->oem_id, NTFS_OEM_ID, 8) != 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("NTFS: Invalid OEM ID\n");
        return -1;
    }

    /* Check for NTFS signature in extended boot sector */
    ntfs_extended_boot_t *ext = (ntfs_extended_boot_t *)((uint8_t *)device_start + 0x30);
    if (ext->sectors_per_cluster == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("NTFS: Invalid extended boot sector\n");
        return -1;
    }

    ntfs_fs.boot = boot;
    ntfs_fs.device_start = device_start;
    ntfs_fs.device_size = device_size;
    ntfs_fs.mounted = 1;
    ntfs_fs.bytes_per_cluster = boot->bytes_per_sector * boot->sectors_per_cluster;
    ntfs_fs.mft_cluster = ext->mft_cluster;
    ntfs_fs.mft_mirror_cluster = ext->mft_mirror_cluster;

    screen_set_content_color(C_INFO);
    screen_term_write("NTFS: Mounted - ");
    char buf[32];
    int2str(device_size / (1024*1024), buf);
    screen_term_write(buf);
    screen_term_write(" MB, cluster size: ");
    int2str(ntfs_fs.bytes_per_cluster / 1024, buf);
    screen_term_write(buf);
    screen_term_write(" KB\n");

    return 0;
}

int ntfs_read_cluster(uint64_t cluster, void *buffer) {
    if (!ntfs_fs.mounted || !buffer) return -1;
    
    uint64_t offset = cluster * ntfs_fs.bytes_per_cluster;
    if (offset + ntfs_fs.bytes_per_cluster > ntfs_fs.device_size) return -1;
    
    memcpy(buffer, (uint8_t *)ntfs_fs.device_start + offset, ntfs_fs.bytes_per_cluster);
    return 0;
}

int ntfs_read_mft(uint64_t mft_num, ntfs_mft_record_t *record) {
    if (!ntfs_fs.mounted || !record) return -1;
    
    /* Calculate MFT record position */
    uint64_t mft_cluster = ntfs_fs.mft_cluster + (mft_num * 4); /* Approximate */
    
    uint8_t *cluster_data = (uint8_t *)kmalloc(ntfs_fs.bytes_per_cluster);
    if (!cluster_data) return -1;
    
    if (ntfs_read_cluster(mft_cluster, cluster_data) < 0) {
        kfree(cluster_data);
        return -1;
    }
    
    /* Copy MFT record */
    memcpy(record, cluster_data, sizeof(ntfs_mft_record_t));
    
    if (record->magic != NTFS_MFT_MAGIC) {
        screen_set_content_color(C_ERROR);
        screen_term_write("NTFS: Invalid MFT record magic\n");
        kfree(cluster_data);
        return -1;
    }
    
    kfree(cluster_data);
    return 0;
}
