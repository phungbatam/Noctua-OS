#ifndef NTFS_H
#define NTFS_H

#include <stdint.h>

/* NTFS boot sector constants */
#define NTFS_OEM_ID          "NTFS    "
#define NTFS_SIGNATURE       0x5346544E  /* "NTFS" */

/* NTFS boot sector structure */
typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  extended_boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[426];
    uint16_t boot_signature;
} __attribute__((packed)) ntfs_boot_sector_t;

/* NTFS specific boot sector fields (after standard part) */
typedef struct {
    uint64_t sectors_per_cluster;
    uint64_t mft_cluster;
    uint64_t mft_mirror_cluster;
    uint8_t  cluster_per_file_record;
    uint8_t  reserved1[3];
    uint8_t  cluster_per_index_record;
    uint8_t  reserved2[3];
    uint64_t volume_serial;
    uint16_t checksum;
} __attribute__((packed)) ntfs_extended_boot_t;

/* MFT (Master File Table) entry header */
typedef struct {
    uint32_t magic;
    uint16_t update_seq_offset;
    uint16_t update_seq_size;
    uint64_t log_file_seq;
    uint16_t sequence;
    uint16_t link_count;
    uint16_t attribute_offset;
    uint16_t flags;
    uint32_t used_size;
    uint32_t allocated_size;
    uint64_t file_reference;
    uint16_t next_attr_id;
    uint16_t padding;
    uint32_t mft_record_number;
} __attribute__((packed)) ntfs_mft_record_t;

#define NTFS_MFT_MAGIC 0x454C4946  /* "FILE" */

/* NTFS attribute types */
#define NTFS_ATTR_STANDARD_INFORMATION 0x10
#define NTFS_ATTR_ATTRIBUTE_LIST      0x20
#define NTFS_ATTR_FILE_NAME          0x30
#define NTFS_ATTR_OBJECT_ID           0x40
#define NTFS_ATTR_SECURITY_DESCRIPTOR 0x50
#define NTFS_ATTR_VOLUME_NAME        0x60
#define NTFS_ATTR_VOLUME_INFORMATION 0x70
#define NTFS_ATTR_DATA               0x80
#define NTFS_ATTR_INDEX_ROOT         0x90
#define NTFS_ATTR_INDEX_ALLOCATION   0xA0
#define NTFS_ATTR_BITMAP             0xB0
#define NTFS_ATTR_REPARSE_POINT      0xC0
#define NTFS_ATTR_EA_INFORMATION     0xD0
#define NTFS_ATTR_EA                 0xE0
#define NTFS_ATTR_LOGGED_TOOL_STREAM 0x100

/* NTFS attribute header */
typedef struct {
    uint32_t type;
    uint32_t length;
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t instance;
} __attribute__((packed)) ntfs_attr_header_t;

/* NTFS filesystem state */
typedef struct {
    ntfs_boot_sector_t *boot;
    uint64_t bytes_per_cluster;
    uint64_t mft_cluster;
    uint64_t mft_mirror_cluster;
    int mounted;
    void *device_start;
    uint64_t device_size;
} ntfs_fs_t;

/* NTFS API */
int ntfs_mount(void *device_start, uint64_t device_size);
int ntfs_read_mft(uint64_t mft_num, ntfs_mft_record_t *record);
int ntfs_read_cluster(uint64_t cluster, void *buffer);

#endif
