#include "partition.h"
#include "ata.h"
#include "string.h"
#include "heap.h"

static partition_info_t partition_list[MAX_PARTITIONS];
static int partition_count = 0;

const char *partition_type_name(uint8_t type) {
    switch (type) {
        case 0x00: return "Empty";
        case 0x01: return "FAT12";
        case 0x04: return "FAT16 (<32M)";
        case 0x05: return "Extended CHS";
        case 0x06: return "FAT16B";
        case 0x07: return "NTFS/HPFS";
        case 0x0B: return "FAT32 CHS";
        case 0x0C: return "FAT32 LBA";
        case 0x0E: return "FAT16B LBA";
        case 0x0F: return "Extended LBA";
        case 0x82: return "Linux Swap";
        case 0x83: return "Linux native";
        case 0x8E: return "Linux LVM";
        case 0xEE: return "GPT Protective";
        case 0xEF: return "EFI System";
        default: return "Unknown";
    }
}

int partition_read_mbr(uint8_t drive, partition_table_t *table) {
    ata_device_t *dev = ata_get_device(drive);
    if (!dev || !dev->present) return 0;

    uint8_t *mbr_buf = (uint8_t *)kmalloc(512);
    if (!mbr_buf) return 0;

    int ret = ata_read_sectors(dev, 0, 1, mbr_buf);
    if (ret < 0) {
        kfree(mbr_buf);
        return 0;
    }

    mbr_t *mbr = (mbr_t *)mbr_buf;
    table->signature = mbr->signature;
    table->is_valid = (mbr->signature == MBR_SIGNATURE);
    table->is_mbr = table->is_valid;
    table->is_gpt = 0;

    if (table->is_valid) {
        for (int i = 0; i < 4; i++) {
            table->mbr_entries[i] = mbr->partitions[i];
            if (mbr->partitions[i].type != 0) {
                table->partition_count++;
            }
        }
    }

    uint64_t total_sectors = 0;
    for (int i = 0; i < 4; i++) {
        if (table->mbr_entries[i].type != 0) {
            uint64_t end = (uint64_t)table->mbr_entries[i].lba_start + table->mbr_entries[i].sector_count;
            if (end > total_sectors) total_sectors = end;
        }
    }
    if (total_sectors == 0 && dev->total_sectors > 0) {
        total_sectors = dev->total_sectors;
    }
    table->disk_size_sectors = total_sectors;
    table->sector_size = 512;

    kfree(mbr_buf);
    return table->is_valid ? 1 : 0;
}

int partition_read_gpt(uint8_t drive, partition_table_t *table) {
    ata_device_t *dev = ata_get_device(drive);
    if (!dev || !dev->present) return 0;

    int ret = partition_read_mbr(drive, table);
    if (!ret) return 0;

    if (table->mbr_entries[0].type != PART_TYPE_GPT) return 0;

    uint8_t *gpt_buf = (uint8_t *)kmalloc(512);
    if (!gpt_buf) return 0;

    ret = ata_read_sectors(dev, 1, 1, gpt_buf);
    if (ret < 0) {
        kfree(gpt_buf);
        return 0;
    }

    gpt_header_t *gpt = (gpt_header_t *)gpt_buf;
    table->gpt_header = *gpt;
    table->is_gpt = (gpt->signature == GPT_SIGNATURE);
    table->is_mbr = 1;

    if (!table->is_gpt) {
        kfree(gpt_buf);
        return 0;
    }

    table->disk_size_sectors = gpt->last_usable_lba + 1;
    table->sector_size = 512;

    int num_entries = gpt->num_partition_entries;
    if (num_entries > 128) num_entries = 128;

    uint32_t entries_lba = gpt->partition_entry_lba;
    int entries_per_sector = 512 / gpt->entry_size;
    int sectors_needed = (num_entries + entries_per_sector - 1) / entries_per_sector;

    uint8_t *entries_buf = (uint8_t *)kmalloc(sectors_needed * 512);
    if (!entries_buf) {
        kfree(gpt_buf);
        return 0;
    }

    for (int s = 0; s < sectors_needed; s++) {
        ata_read_sectors(dev, entries_lba + s, 1, entries_buf + s * 512);
    }

    table->gpt_entry_count = 0;
    for (int i = 0; i < num_entries; i++) {
        gpt_entry_t *entry = (gpt_entry_t *)(entries_buf + i * gpt->entry_size);
        if (entry->partition_type_guid[0] == 0 && entry->partition_type_guid[1] == 0) continue;
        if (entry->starting_lba == 0) continue;

        table->gpt_entries[table->gpt_entry_count] = *entry;
        table->gpt_entry_count++;
        table->partition_count++;
    }

    kfree(entries_buf);
    kfree(gpt_buf);
    return 1;
}

int partition_init(void) {
    partition_count = 0;
    memset(partition_list, 0, sizeof(partition_list));

    int nd = ata_device_count();
    for (int d = 0; d < nd; d++) {
        ata_device_t *dev = ata_get_device(d);
        if (!dev || !dev->present) continue;

        partition_table_t table;
        memset(&table, 0, sizeof(table));

        int ret = partition_read_gpt(d, &table);
        if (!ret) ret = partition_read_mbr(d, &table);
        if (!ret) continue;

        if (table.is_gpt) {
            for (int i = 0; i < table.gpt_entry_count && partition_count < MAX_PARTITIONS; i++) {
                partition_info_t *p = &partition_list[partition_count];
                p->present = 1;
                p->index = partition_count;
                p->is_gpt = 1;
                p->partition_number = i + 1;
                p->lba_start = table.gpt_entries[i].starting_lba;
                p->lba_end = table.gpt_entries[i].ending_lba;
                p->sector_count = p->lba_end - p->lba_start + 1;
                p->size_bytes = p->sector_count * table.sector_size;
                p->bootable = 0;
                p->type = 0;
                p->label[0] = 0;
                partition_count++;
            }
        } else if (table.is_mbr) {
            for (int i = 0; i < 4 && partition_count < MAX_PARTITIONS; i++) {
                mbr_entry_t *e = &table.mbr_entries[i];
                if (e->type == 0 || e->type == PART_TYPE_EMPTY) continue;

                partition_info_t *p = &partition_list[partition_count];
                p->present = 1;
                p->index = partition_count;
                p->is_gpt = 0;
                p->partition_number = i + 1;
                p->type = e->type;
                p->bootable = (e->status & PART_ATTR_BOOTABLE);
                p->lba_start = e->lba_start;
                p->lba_end = (uint64_t)e->lba_start + e->sector_count - 1;
                p->sector_count = e->sector_count;
                p->size_bytes = (uint64_t)e->sector_count * table.sector_size;

                const char *tn = partition_type_name(e->type);
                int j;
                for (j = 0; tn[j] && j < 39; j++) p->label[j] = tn[j];
                p->label[j] = 0;

                partition_count++;

                if (e->type == PART_TYPE_EXTENDED || e->type == PART_TYPE_EXT_LBA) {
                    uint32_t ext_start = e->lba_start;
                    uint32_t ext_lba = ext_start;
                    for (int log = 0; log < 8 && partition_count < MAX_PARTITIONS; log++) {
                        uint8_t *ebr_buf = (uint8_t *)kmalloc(512);
                        if (!ebr_buf) break;

                        if (ata_read_sectors(dev, ext_lba, 1, ebr_buf) < 0) {
                            kfree(ebr_buf);
                            break;
                        }

                        mbr_t *ebr = (mbr_t *)ebr_buf;
                        if (ebr->signature != MBR_SIGNATURE) {
                            kfree(ebr_buf);
                            break;
                        }

                        if (ebr->partitions[0].type != 0 && ebr->partitions[0].lba_start != 0) {
                            partition_info_t *lp = &partition_list[partition_count];
                            lp->present = 1;
                            lp->index = partition_count;
                            lp->is_gpt = 0;
                            lp->partition_number = i + 1 + log + 1;
                            lp->type = ebr->partitions[0].type;
                            lp->bootable = 0;
                            lp->lba_start = ext_start + ebr->partitions[0].lba_start;
                            lp->lba_end = lp->lba_start + ebr->partitions[0].sector_count - 1;
                            lp->sector_count = ebr->partitions[0].sector_count;
                            lp->size_bytes = (uint64_t)ebr->partitions[0].sector_count * table.sector_size;

                            const char *tn2 = partition_type_name(ebr->partitions[0].type);
                            int j2;
                            for (j2 = 0; tn2[j2] && j2 < 39; j2++) lp->label[j2] = tn2[j2];
                            lp->label[j2] = 0;

                            partition_count++;
                        }

                        if (ebr->partitions[1].type == 0 || ebr->partitions[1].lba_start == 0) {
                            kfree(ebr_buf);
                            break;
                        }

                        ext_lba = ext_start + ebr->partitions[1].lba_start;
                        kfree(ebr_buf);
                    }
                }
            }
        }
    }

    return partition_count;
}

int partition_scan(partition_info_t *partitions, int max) {
    if (!partitions) return 0;
    int copy = (partition_count < max) ? partition_count : max;
    for (int i = 0; i < copy; i++) {
        partitions[i] = partition_list[i];
    }
    return copy;
}

int partition_get_count(void) {
    return partition_count;
}

partition_info_t *partition_get(int index) {
    if (index < 0 || index >= partition_count) return 0;
    return &partition_list[index];
}
