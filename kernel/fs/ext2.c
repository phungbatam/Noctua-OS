#include "ext2.h"
#include "screen.h"
#include "string.h"
#include "mm/heap.h"
#include "block/ata.h"

static ext2_fs_t ext2_fs;

int ext2_mount(void *device_start, uint32_t device_size) {
    memset(&ext2_fs, 0, sizeof(ext2_fs));

    /* Read superblock (at offset 1024) */
    ext2_superblock_t *sb = (ext2_superblock_t *)((uint8_t *)device_start + 1024);

    if (sb->s_magic != EXT2_SUPER_MAGIC) {
        screen_set_content_color(C_ERROR);
        screen_term_write("EXT2: Invalid superblock magic\n");
        return -1;
    }

    ext2_fs.sb = sb;
    ext2_fs.block_size = 1024 << sb->s_log_block_size;
    ext2_fs.device_start = device_start;
    ext2_fs.device_size = device_size;
    ext2_fs.mounted = 1;
    ext2_fs.has_journal = 0;
    ext2_fs.journal_inode = 0;
    ext2_fs.has_extents = 0;
    ext2_fs.is_64bit = 0;

    /* Check for EXT3 journaling features */
    if (sb->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL) {
        ext2_fs.has_journal = 1;
        ext2_fs.journal_inode = sb->s_journal_inum;
    }

    /* Check for EXT4 features */
    if (sb->s_feature_compat & EXT4_FEATURE_COMPAT_EXTENTS) {
        ext2_fs.has_extents = 1;
    }
    if (sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        ext2_fs.is_64bit = 1;
    }

    ext2_fs.num_block_groups = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;

    /* Read block group descriptors (located after superblock) */
    uint32_t bgdt_offset = ext2_fs.block_size;
    if (ext2_fs.block_size == 1024) bgdt_offset = 2048;

    ext2_fs.block_groups = (ext2_bg_desc_t *)((uint8_t *)device_start + bgdt_offset);

    screen_set_content_color(C_INFO);
    screen_term_write("EXT2: Mounted - ");
    char buf[16];
    int2str(sb->s_blocks_count * ext2_fs.block_size / (1024*1024), buf);
    screen_term_write(buf);
    screen_term_write(" MB, block size: ");
    int2str(ext2_fs.block_size, buf);
    screen_term_write(buf);
    
    if (ext2_fs.has_journal) {
        screen_term_write(" (EXT3");
        if (ext2_fs.has_extents) {
            screen_term_write("/EXT4");
        }
        screen_term_write(" with journaling)");
    } else if (ext2_fs.has_extents) {
        screen_term_write(" (EXT4 with extents)");
    }
    
    if (ext2_fs.is_64bit) {
        screen_term_write(" [64-bit]");
    }
    
    screen_term_write("\n");

    /* Initialize journal if present */
    if (ext2_fs.has_journal) {
        ext3_journal_init();
    }

    return 0;
}

int ext2_read_block(uint32_t block_num, void *buffer) {
    if (!ext2_fs.mounted || !buffer) return -1;
    uint32_t offset = block_num * ext2_fs.block_size;
    if (offset + ext2_fs.block_size > ext2_fs.device_size) return -1;
    memcpy(buffer, (uint8_t *)ext2_fs.device_start + offset, ext2_fs.block_size);
    return 0;
}

int ext2_read_inode(uint32_t inode_num, ext2_inode_t *inode) {
    if (!ext2_fs.mounted || !inode || inode_num == 0) return -1;

    uint32_t group = (inode_num - 1) / ext2_fs.sb->s_inodes_per_group;
    uint32_t index = (inode_num - 1) % ext2_fs.sb->s_inodes_per_group;

    if (group >= ext2_fs.num_block_groups) return -1;

    ext2_bg_desc_t *bg = &ext2_fs.block_groups[group];
    uint32_t inode_table_block = bg->bg_inode_table;
    uint32_t inode_size = ext2_fs.sb->s_inode_size;
    if (inode_size == 0) inode_size = 128;

    uint32_t offset = inode_table_block * ext2_fs.block_size + index * inode_size;
    if (offset + sizeof(ext2_inode_t) > ext2_fs.device_size) return -1;

    memcpy(inode, (uint8_t *)ext2_fs.device_start + offset, sizeof(ext2_inode_t));
    return 0;
}

/* Resolve indirect/double/triple indirect blocks */
static int read_indirect_block(uint32_t block_num, uint32_t block_idx, uint32_t *out_block) {
    if (block_num == 0) return -1;

    uint32_t ptrs_per_block = ext2_fs.block_size / 4;
    uint8_t *block_data = (uint8_t *)kmalloc(ext2_fs.block_size);
    if (!block_data) return -1;

    ext2_read_block(block_num, block_data);
    uint32_t *ptrs = (uint32_t *)block_data;

    if (block_idx < ptrs_per_block) {
        *out_block = ptrs[block_idx];
        kfree(block_data);
        return 0;
    }

    kfree(block_data);
    return -1;
}

int ext2_read_inode_block(ext2_inode_t *inode, uint32_t block_idx, void *buffer) {
    if (!inode || !buffer) return -1;

    uint32_t block_num = 0;
    uint32_t ptrs_per_block = ext2_fs.block_size / 4;

    /* Direct blocks */
    if (block_idx < 12) {
        block_num = inode->i_block[block_idx];
    }
    /* Singly indirect */
    else if (block_idx < 12 + ptrs_per_block) {
        if (read_indirect_block(inode->i_block[12], block_idx - 12, &block_num) < 0)
            return -1;
    }
    /* Doubly indirect */
    else if (block_idx < 12 + ptrs_per_block + ptrs_per_block * ptrs_per_block) {
        uint32_t idx = block_idx - 12 - ptrs_per_block;
        uint32_t indirect_block;
        if (read_indirect_block(inode->i_block[13], idx / ptrs_per_block, &indirect_block) < 0)
            return -1;
        if (read_indirect_block(indirect_block, idx % ptrs_per_block, &block_num) < 0)
            return -1;
    }
    /* Triply indirect */
    else {
        uint32_t idx = block_idx - 12 - ptrs_per_block - ptrs_per_block * ptrs_per_block;
        uint32_t indirect1, indirect2;
        if (read_indirect_block(inode->i_block[14], idx / (ptrs_per_block * ptrs_per_block), &indirect1) < 0)
            return -1;
        if (read_indirect_block(indirect1, (idx / ptrs_per_block) % ptrs_per_block, &indirect2) < 0)
            return -1;
        if (read_indirect_block(indirect2, idx % ptrs_per_block, &block_num) < 0)
            return -1;
    }

    if (block_num == 0) return -1;
    return ext2_read_block(block_num, buffer);
}

uint32_t ext2_find_file(const char *path) {
    if (!ext2_fs.mounted || !path) return 0;

    /* Start from root */
    uint32_t current_ino = EXT2_ROOT_INO;

    if (path[0] == '/') path++;

    if (path[0] == 0) return current_ino;

    char path_copy[256];
    strcpy(path_copy, path);
    char *token = path_copy;

    while (*token) {
        while (*token == '/') token++;
        if (!*token) break;

        char *end = token;
        while (*end && *end != '/') end++;
        char orig = *end;
        *end = 0;

        /* Read current directory inode */
        ext2_inode_t dir_inode;
        if (ext2_read_inode(current_ino, &dir_inode) < 0) return 0;
        if (!(dir_inode.i_mode & EXT2_S_IFDIR)) return 0;

        /* Search directory entries */
        uint32_t found = 0;
        uint32_t block_idx = 0;
        uint32_t offset_in_block = 0;
        uint8_t *block_data = (uint8_t *)kmalloc(ext2_fs.block_size);
        if (!block_data) return 0;

        int dir_done = 0;
        while (!dir_done && block_idx < (dir_inode.i_size + ext2_fs.block_size - 1) / ext2_fs.block_size) {
            if (ext2_read_inode_block(&dir_inode, block_idx, block_data) < 0) break;

            offset_in_block = 0;
            while (offset_in_block < ext2_fs.block_size) {
                ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(block_data + offset_in_block);

                if (entry->inode == 0) {
                    offset_in_block += entry->rec_len;
                    if (entry->rec_len == 0) break;
                    continue;
                }

                if (entry->name_len == (uint8_t)strlen(token) &&
                    strncmp(entry->name, token, entry->name_len) == 0) {
                    found = entry->inode;
                    dir_done = 1;
                    break;
                }

                offset_in_block += entry->rec_len;
                if (entry->rec_len == 0) break;
            }
            block_idx++;
        }
        kfree(block_data);

        if (!found) return 0;
        current_ino = found;

        if (orig == 0) break;
        token = end + 1;
    }

    return current_ino;
}

void ext2_list_dir(uint32_t inode_num) {
    if (!ext2_fs.mounted) return;

    ext2_inode_t dir_inode;
    if (ext2_read_inode(inode_num, &dir_inode) < 0) return;
    if (!(dir_inode.i_mode & EXT2_S_IFDIR)) return;

    screen_set_content_color(C_HEADER);
    screen_term_write("=== EXT2 Directory Listing ===\n");
    screen_set_content_color(C_INFO);

    uint32_t block_idx = 0;
    uint8_t *block_data = (uint8_t *)kmalloc(ext2_fs.block_size);
    if (!block_data) return;

    while (block_idx < (dir_inode.i_size + ext2_fs.block_size - 1) / ext2_fs.block_size) {
        if (ext2_read_inode_block(&dir_inode, block_idx, block_data) < 0) break;

        uint32_t offset = 0;
        while (offset < ext2_fs.block_size) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(block_data + offset);
            if (entry->inode == 0) {
                offset += entry->rec_len;
                if (entry->rec_len == 0) break;
                continue;
            }

            /* Get inode info */
            ext2_inode_t file_inode;
            char type_char = '-';
            if (ext2_read_inode(entry->inode, &file_inode) == 0) {
                if (file_inode.i_mode & EXT2_S_IFDIR) type_char = 'd';
                else if (file_inode.i_mode & EXT2_S_IFLNK) type_char = 'l';
            }

            char size_buf[16];
            int2str((int)file_inode.i_size, size_buf);

            screen_set_content_color(C_WIN_TITLE);
            screen_term_write(" ");
            char c[2] = {type_char, 0};
            screen_term_write(c);
            screen_term_write(" ");
            screen_set_content_color(C_INFO);

            /* Print name */
            for (int i = 0; i < entry->name_len; i++) {
                char n[2] = {entry->name[i], 0};
                screen_term_write(n);
            }

            /* Padding */
            for (int i = entry->name_len; i < 25; i++) screen_term_write(" ");

            screen_set_content_color(C_WIN_TEXT);
            screen_term_write(size_buf);
            screen_set_content_color(C_INFO);
            screen_term_write("\n");

            offset += entry->rec_len;
            if (entry->rec_len == 0) break;
        }
        block_idx++;
    }
    kfree(block_data);
}

int ext2_read_file(uint32_t inode_num, uint32_t offset, uint32_t size, void *buffer) {
    if (!ext2_fs.mounted || !buffer) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(inode_num, &inode) < 0) return -1;

    if (offset >= inode.i_size) return 0;
    if (offset + size > inode.i_size) size = inode.i_size - offset;

    uint32_t block_size = ext2_fs.block_size;
    uint32_t start_block = offset / block_size;
    uint32_t end_block = (offset + size + block_size - 1) / block_size;
    uint32_t bytes_read = 0;

    uint8_t *block_data = (uint8_t *)kmalloc(block_size);
    if (!block_data) return -1;

    for (uint32_t b = start_block; b < end_block; b++) {
        if (ext2_read_inode_block(&inode, b, block_data) < 0) break;

        uint32_t block_offset = (b == start_block) ? (offset % block_size) : 0;
        uint32_t to_copy = block_size - block_offset;
        if (bytes_read + to_copy > size) to_copy = size - bytes_read;

        memcpy((uint8_t *)buffer + bytes_read, block_data + block_offset, to_copy);
        bytes_read += to_copy;
    }

    kfree(block_data);
    return bytes_read;
}

/* EXT3 Journaling Support */
int ext3_journal_init(void) {
    if (!ext2_fs.has_journal) return -1;
    
    screen_set_content_color(C_INFO);
    screen_term_write("EXT3: Initializing journaling...\n");
    
    /* Read journal inode */
    ext2_inode_t journal_inode;
    if (ext2_read_inode(ext2_fs.journal_inode, &journal_inode) < 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("EXT3: Failed to read journal inode\n");
        return -1;
    }
    
    /* Read journal superblock (first block of journal) */
    uint8_t *journal_block = (uint8_t *)kmalloc(ext2_fs.block_size);
    if (!journal_block) return -1;
    
    if (ext2_read_inode_block(&journal_inode, 0, journal_block) < 0) {
        kfree(journal_block);
        return -1;
    }
    
    journal_superblock_t *jsb = (journal_superblock_t *)journal_block;
    if (jsb->s_header != JBD2_MAGIC_NUMBER) {
        screen_set_content_color(C_ERROR);
        screen_term_write("EXT3: Invalid journal superblock magic\n");
        kfree(journal_block);
        return -1;
    }
    
    screen_set_content_color(C_INFO);
    screen_term_write("EXT3: Journal initialized (");
    char buf[16];
    int2str(jsb->s_maxlen, buf);
    screen_term_write(buf);
    screen_term_write(" blocks)\n");
    
    kfree(journal_block);
    
    /* Perform journal recovery if needed */
    if (ext2_fs.sb->s_feature_incompat & EXT3_FEATURE_INCOMPAT_RECOVER) {
        ext3_journal_recover();
    }
    
    return 0;
}

int ext3_journal_recover(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("EXT3: Journal recovery not yet implemented\n");
    return 0;
}

/* EXT4 Extent Support */
int ext4_read_extent(ext2_inode_t *inode, uint32_t block_idx, void *buffer) {
    if (!ext2_fs.has_extents || !inode || !buffer) return -1;
    
    /* Check if inode uses extents (first block is extent header) */
    if (inode->i_block[0] == 0) return -1;
    
    /* Read the extent tree */
    uint8_t *block_data = (uint8_t *)kmalloc(ext2_fs.block_size);
    if (!block_data) return -1;
    
    /* Read first block which contains extent header */
    uint32_t block_num = inode->i_block[0];
    if (ext2_read_block(block_num, block_data) < 0) {
        kfree(block_data);
        return -1;
    }
    
    ext4_extent_header_t *eh = (ext4_extent_header_t *)block_data;
    if (eh->e_magic != EXT4_EXT_MAGIC) {
        kfree(block_data);
        return -1; /* Not using extents */
    }
    
    /* Search for the extent containing the requested block */
    ext4_extent_t *extents = (ext4_extent_t *)(block_data + sizeof(ext4_extent_header_t));
    
    for (int i = 0; i < eh->e_entries; i++) {
        uint32_t ext_block = extents[i].e_block;
        uint32_t ext_len = extents[i].e_len;
        uint64_t ext_start = ((uint64_t)extents[i].e_start_hi << 32) | extents[i].e_start_lo;
        
        if (block_idx >= ext_block && block_idx < ext_block + ext_len) {
            /* Found the extent, read the block */
            uint32_t phys_block = ext_start + (block_idx - ext_block);
            kfree(block_data);
            return ext2_read_block(phys_block, buffer);
        }
    }
    
    kfree(block_data);
    return -1;
}
