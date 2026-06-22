#ifndef TVN_PARTITION_H
#define TVN_PARTITION_H

#include <stdint.h>

#define MBR_SIGNATURE 0xAA55
#define GPT_SIGNATURE 0x5452415020494645ULL

#define PART_TYPE_EMPTY      0x00
#define PART_TYPE_FAT12      0x01
#define PART_TYPE_FAT16      0x04
#define PART_TYPE_FAT16B     0x06
#define PART_TYPE_NTFS       0x07
#define PART_TYPE_FAT32_CHS  0x0B
#define PART_TYPE_FAT32_LBA  0x0C
#define PART_TYPE_FAT16B_LBA 0x0E
#define PART_TYPE_EXTENDED   0x05
#define PART_TYPE_EXT_LBA    0x0F
#define PART_TYPE_LINUX      0x83
#define PART_TYPE_LINUX_SWAP 0x82
#define PART_TYPE_LINUX_LVM  0x8E
#define PART_TYPE_GPT        0xEE

#define PART_ATTR_NONE       0x00
#define PART_ATTR_BOOTABLE   0x01

typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed)) mbr_entry_t;

typedef struct {
    uint8_t  code[446];
    mbr_entry_t partitions[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint64_t guid[2];
    uint32_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t entry_size;
    uint32_t partition_entries_crc32;
} __attribute__((packed)) gpt_header_t;

typedef struct {
    uint64_t partition_type_guid[2];
    uint64_t unique_guid[2];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    uint16_t name[36];
} __attribute__((packed)) gpt_entry_t;

typedef struct {
    int is_valid;
    int is_gpt;
    int is_mbr;
    int partition_count;
    uint64_t disk_size_sectors;
    uint32_t sector_size;
    uint16_t signature;

    mbr_entry_t mbr_entries[4];

    gpt_header_t gpt_header;
    gpt_entry_t gpt_entries[128];
    int gpt_entry_count;
} partition_table_t;

#define MAX_PARTITIONS 16

typedef struct {
    int present;
    int index;
    uint8_t type;
    uint8_t bootable;
    uint64_t lba_start;
    uint64_t lba_end;
    uint64_t sector_count;
    uint64_t size_bytes;
    int is_gpt;
    int partition_number;
    char label[40];
} partition_info_t;

int partition_init(void);
int partition_read_mbr(uint8_t drive, partition_table_t *table);
int partition_read_gpt(uint8_t drive, partition_table_t *table);
int partition_scan(partition_info_t *partitions, int max);
int partition_get_count(void);
partition_info_t *partition_get(int index);
const char *partition_type_name(uint8_t type);

#endif
