#include "editor.h"
#include "screen.h"
#include "fb.h"
#include "string.h"
#include "keyboard.h"
#include "mm/heap.h"
#include "fs/fat32.h"

#define EDITOR_MAX_LINES   2048
#define EDITOR_MAX_LEN     256
#define EDITOR_TAB_STOP    4
#define STATUS_BAR_H       1

typedef struct {
    char (*lines)[EDITOR_MAX_LEN];
    int num_lines;
    int capacity;
    int cx, cy;
    int sx, sy;
    int modified;
    char filename[128];
} editor_t;

static editor_t *ed = 0;

static int editor_cols(void) {
    return fb_chars_x;
}

static int editor_rows(void) {
    int h = fb_chars_y - HEADER_ROWS - STATUS_BAR_H;
    return h > 0 ? h : 10;
}

static int line_len(int y) {
    if (y < 0 || y >= ed->num_lines) return 0;
    int i = 0;
    while (ed->lines[y][i] && i < EDITOR_MAX_LEN - 1) i++;
    return i;
}

static void editor_status(const char *msg, int is_error) {
    int cols = editor_cols();
    int row = fb_chars_y - 1;
    uint32_t bg = is_error ? 0x440000 : 0x003366;
    uint32_t fg = 0xFFFFFF;

    fb_fill(0, row * CHAR_H, fb_width, CHAR_H, bg);
    fb_str(1, row, msg, fg, bg);

    char buf[32];
    buf[0] = 0;
    if (ed->modified) strcat(buf, " [MODIFIED]");
    strcat(buf, "  F1:Save  F2:Quit");
    fb_str(cols - 22, row, buf, 0xAAAAAA, bg);
}

static void editor_refresh(void) {
    int cols = editor_cols();
    int rows = editor_rows();

    for (int y = 0; y < rows; y++) {
        int fy = y + HEADER_ROWS;
        int file_y = y + ed->sy;

        for (int x = 0; x < cols; x++) {
            int file_x = x + ed->sx;
            char c = ' ';

            if (file_y >= 0 && file_y < ed->num_lines) {
                int l = line_len(file_y);
                if (file_x >= 0 && file_x < l) {
                    c = ed->lines[file_y][file_x];
                    if (c < 0x20 || c > 0x7E) c = '.';
                }
            }

            unsigned char attr = C_INFO;
            draw_char_at(x, fy, c, attr);
        }
    }

    /* Draw cursor */
    int cur_x = ed->cx - ed->sx;
    int cur_y = ed->cy - ed->sy;
    if (cur_x >= 0 && cur_x < cols && cur_y >= 0 && cur_y < rows) {
        char c = ' ';
        if (ed->cy < ed->num_lines && ed->cx < line_len(ed->cy)) {
            c = ed->lines[ed->cy][ed->cx];
            if (c < 0x20 || c > 0x7E) c = ' ';
        }
        draw_char_at(cur_x, cur_y + HEADER_ROWS, c, 0x70);
    }

    editor_status("", 0);
}

static void editor_scroll_to_cursor(void) {
    int cols = editor_cols();
    int rows = editor_rows();

    if (ed->cx < ed->sx) ed->sx = ed->cx;
    if (ed->cx >= ed->sx + cols) ed->sx = ed->cx - cols + 1;
    if (ed->cy < ed->sy) ed->sy = ed->cy;
    if (ed->cy >= ed->sy + rows) ed->sy = ed->cy - rows + 1;
}

static void editor_insert_char(char c) {
    if (ed->cy >= ed->num_lines) return;
    int l = line_len(ed->cy);
    if (l >= EDITOR_MAX_LEN - 2) return;

    for (int i = l; i >= ed->cx; i--)
        ed->lines[ed->cy][i + 1] = ed->lines[ed->cy][i];
    ed->lines[ed->cy][ed->cx] = c;
    ed->cx++;
    ed->modified = 1;
}

static void editor_newline(void) {
    if (ed->num_lines >= ed->capacity) return;

    int l = line_len(ed->cy);

    for (int i = ed->num_lines; i > ed->cy + 1; i--)
        memcpy(ed->lines[i], ed->lines[i - 1], EDITOR_MAX_LEN);
    memset(ed->lines[ed->cy + 1], 0, EDITOR_MAX_LEN);

    int rest = l - ed->cx;
    if (rest > 0) {
        memcpy(ed->lines[ed->cy + 1], &ed->lines[ed->cy][ed->cx], rest);
        memset(&ed->lines[ed->cy][ed->cx], 0, rest);
    }

    ed->num_lines++;
    ed->cy++;
    ed->cx = 0;
    ed->modified = 1;
}

static void editor_backspace(void) {
    if (ed->cx > 0) {
        int l = line_len(ed->cy);
        for (int i = ed->cx - 1; i < l; i++)
            ed->lines[ed->cy][i] = ed->lines[ed->cy][i + 1];
        ed->cx--;
        ed->modified = 1;
    } else if (ed->cy > 0) {
        int prev_len = line_len(ed->cy - 1);
        int cur_len = line_len(ed->cy);
        if (prev_len + cur_len < EDITOR_MAX_LEN - 1) {
            memcpy(&ed->lines[ed->cy - 1][prev_len], ed->lines[ed->cy], cur_len);
            ed->lines[ed->cy - 1][prev_len + cur_len] = 0;
            for (int i = ed->cy; i < ed->num_lines - 1; i++)
                memcpy(ed->lines[i], ed->lines[i + 1], EDITOR_MAX_LEN);
            memset(ed->lines[ed->num_lines - 1], 0, EDITOR_MAX_LEN);
            ed->num_lines--;
            ed->cy--;
            ed->cx = prev_len;
            ed->modified = 1;
        }
    }
}

static void editor_delete(void) {
    int l = line_len(ed->cy);
    if (ed->cx < l) {
        for (int i = ed->cx; i < l; i++)
            ed->lines[ed->cy][i] = ed->lines[ed->cy][i + 1];
        ed->modified = 1;
    } else if (ed->cy < ed->num_lines - 1) {
        int next_len = line_len(ed->cy + 1);
        if (l + next_len < EDITOR_MAX_LEN - 1) {
            memcpy(&ed->lines[ed->cy][l], ed->lines[ed->cy + 1], next_len);
            ed->lines[ed->cy][l + next_len] = 0;
            for (int i = ed->cy + 1; i < ed->num_lines - 1; i++)
                memcpy(ed->lines[i], ed->lines[i + 1], EDITOR_MAX_LEN);
            memset(ed->lines[ed->num_lines - 1], 0, EDITOR_MAX_LEN);
            ed->num_lines--;
            ed->modified = 1;
        }
    }
}

static void editor_save(void) {
    if (!ed->filename[0]) {
        editor_status("ERROR: No filename (undefined)", 1);
        return;
    }

    vfs_node_t *node = vfs_find_node(ed->filename);
    if (!node) {
        node = vfs_create_node(ed->filename, 0);
    }
    if (!node || !node->f_op || !node->f_op->write) {
        editor_status("ERROR: Cannot write file", 1);
        return;
    }

    char *buffer = (char *)kmalloc(EDITOR_MAX_LINES * EDITOR_MAX_LEN);
    if (!buffer) {
        editor_status("ERROR: Out of memory", 1);
        return;
    }

    int pos = 0;
    for (int i = 0; i < ed->num_lines && pos < EDITOR_MAX_LINES * EDITOR_MAX_LEN - 1; i++) {
        int l = line_len(i);
        if (pos + l + 1 >= EDITOR_MAX_LINES * EDITOR_MAX_LEN) break;
        memcpy(buffer + pos, ed->lines[i], l);
        pos += l;
        buffer[pos++] = '\n';
    }
    buffer[pos] = 0;

    int written = node->f_op->write(node, 0, pos, buffer);
    kfree(buffer);

    if (written == pos) {
        ed->modified = 0;
        editor_status("File saved successfully", 0);
    } else {
        editor_status("ERROR: Write failed", 1);
    }
}

static void editor_load_file(const char *filename) {
    vfs_node_t *node = vfs_find_node(filename);
    if (!node) {
        ed->num_lines = 1;
        ed->lines[0][0] = 0;
        return;
    }

    if (node->is_directory) {
        ed->num_lines = 1;
        ed->lines[0][0] = 0;
        return;
    }

    char *buf = (char *)kmalloc(node->size + 1);
    if (!buf) {
        ed->num_lines = 1;
        ed->lines[0][0] = 0;
        return;
    }

    int bytes = 0;
    if (node->f_op && node->f_op->read) {
        bytes = node->f_op->read(node, 0, node->size, buf);
    }
    buf[bytes] = 0;

    ed->num_lines = 0;
    int line_start = 0;
    for (int i = 0; i <= bytes && ed->num_lines < ed->capacity; i++) {
        if (buf[i] == '\n' || buf[i] == 0) {
            int len = i - line_start;
            if (len >= EDITOR_MAX_LEN) len = EDITOR_MAX_LEN - 1;
            memcpy(ed->lines[ed->num_lines], buf + line_start, len);
            ed->lines[ed->num_lines][len] = 0;
            ed->num_lines++;
            line_start = i + 1;
        }
    }

    kfree(buf);

    if (ed->num_lines == 0) {
        ed->num_lines = 1;
        ed->lines[0][0] = 0;
    }
}

void editor_open(const char *filename) {
    if (!filename || !filename[0]) {
        screen_term_write("Usage: edit <filename>\n");
        return;
    }

    int alloc_size = sizeof(char) * EDITOR_MAX_LINES * EDITOR_MAX_LEN;
    char (*lines)[EDITOR_MAX_LEN] = (char (*)[EDITOR_MAX_LEN])kmalloc(alloc_size);
    if (!lines) {
        screen_term_write("EDITOR: Out of memory\n");
        return;
    }
    memset(lines, 0, alloc_size);

    editor_t e;
    ed = &e;
    ed->lines = lines;
    ed->capacity = EDITOR_MAX_LINES;
    ed->cx = 0;
    ed->cy = 0;
    ed->sx = 0;
    ed->sy = 0;
    ed->modified = 0;
    strncpy(ed->filename, filename, sizeof(ed->filename) - 1);
    ed->filename[sizeof(ed->filename) - 1] = 0;

    editor_load_file(filename);
    editor_refresh();
    editor_status("", 0);

    int running = 1;
    while (running) {
        int k = keyboard_getchar_nb();
        if (k == 0) continue;

        editor_scroll_to_cursor();

        if (k == K_F1) {
            editor_save();
        } else if (k == K_F2) {
            if (ed->modified) {
                editor_status("File modified! Press F1 to save or F2 again to quit", 1);
                /* Wait for next key */
                while (1) {
                    int k2 = keyboard_getchar_nb();
                    if (k2 == K_F1) { editor_save(); break; }
                    if (k2 == K_F2) { running = 0; break; }
                    if (k2) break;
                }
            } else {
                running = 0;
            }
        } else if (k == K_UP) {
            if (ed->cy > 0) ed->cy--;
        } else if (k == K_DOWN) {
            if (ed->cy < ed->num_lines - 1) ed->cy++;
        } else if (k == K_LEFT) {
            if (ed->cx > 0) ed->cx--;
            else if (ed->cy > 0) { ed->cy--; ed->cx = line_len(ed->cy); }
        } else if (k == K_RIGHT) {
            int l = line_len(ed->cy);
            if (ed->cx < l) ed->cx++;
            else if (ed->cy < ed->num_lines - 1) { ed->cy++; ed->cx = 0; }
        } else if (k == K_HOME) {
            ed->cx = 0;
        } else if (k == K_END) {
            ed->cx = line_len(ed->cy);
        } else if (k == K_PGUP) {
            int rows = editor_rows();
            ed->cy -= rows;
            if (ed->cy < 0) ed->cy = 0;
        } else if (k == K_PGDN) {
            int rows = editor_rows();
            ed->cy += rows;
            if (ed->cy >= ed->num_lines) ed->cy = ed->num_lines - 1;
        } else if (k == K_DEL) {
            editor_delete();
        } else if (k == '\b') {
            editor_backspace();
        } else if (k == '\n') {
            editor_newline();
        } else if (k == '\t') {
            for (int i = 0; i < EDITOR_TAB_STOP; i++)
                editor_insert_char(' ');
        } else if (k >= 0x20 && k < 0x80) {
            editor_insert_char(k);
        } else if (k == K_ESC) {
            if (ed->modified) {
                editor_status("File modified! Press F1 to save or F2 to quit", 1);
            } else {
                running = 0;
            }
        }

        editor_scroll_to_cursor();
        editor_refresh();
    }

    kfree(lines);
    ed = 0;

    /* Restore terminal display */
    screen_clear_content();
}

/* ---- Shell commands ---- */

void cmd_grep(const char *arg) {
    if (!arg || !arg[0]) {
        screen_term_write("Usage: grep <pattern> [filename]\n");
        return;
    }

    char pattern[128];
    char filename[128];
    int has_file = 0;

    const char *p = arg;
    int i = 0;
    while (*p && *p != ' ' && i < 127) pattern[i++] = *p++;
    pattern[i] = 0;
    while (*p == ' ') p++;
    if (*p) {
        has_file = 1;
        i = 0;
        while (*p && i < 127) filename[i++] = *p++;
        filename[i] = 0;
    }

    if (!has_file) {
        screen_term_write("grep: need a filename\n");
        screen_term_write("Usage: grep <pattern> <filename>\n");
        return;
    }

    vfs_node_t *node = vfs_find_node(filename);
    if (!node) {
        screen_term_write("grep: file not found: ");
        screen_term_write(filename);
        screen_term_write("\n");
        return;
    }

    char *buf = (char *)kmalloc(node->size + 1);
    if (!buf) {
        screen_term_write("grep: out of memory\n");
        return;
    }

    int bytes = 0;
    if (node->f_op && node->f_op->read)
        bytes = node->f_op->read(node, 0, node->size, buf);
    buf[bytes] = 0;

    int line_num = 1;
    int line_start = 0;
    int found = 0;
    for (int j = 0; j <= bytes; j++) {
        if (buf[j] == '\n' || buf[j] == 0) {
            int line_len = j - line_start;
            int match = 0;
            for (int k = 0; k <= line_len - (int)strlen(pattern); k++) {
                if (strncmp(buf + line_start + k, pattern, strlen(pattern)) == 0) {
                    match = 1;
                    break;
                }
            }
            if (match) {
                found = 1;
                char num[16];
                int2str(line_num, num);
                screen_term_write(num);
                screen_term_write(": ");
                for (int k = 0; k < line_len; k++)
                    screen_term_putchar(buf[line_start + k]);
                screen_term_write("\n");
            }
            line_num++;
            line_start = j + 1;
        }
    }

    if (!found) {
        screen_term_write("grep: no matches for '");
        screen_term_write(pattern);
        screen_term_write("' in ");
        screen_term_write(filename);
        screen_term_write("\n");
    }

    kfree(buf);
}

void cmd_find(const char *arg) {
    (void)arg;
    screen_term_write("Searching: / (root)\n");
    vfs_node_t *root = vfs_get_root();
    if (!root) {
        screen_term_write("find: filesystem not initialized\n");
        return;
    }

    vfs_node_t *child = root->children;
    int count = 0;
    while (child) {
        char buf[16];
        int2str(++count, buf);
        screen_term_write("  ");
        screen_term_write(buf);
        screen_term_write(". ");
        if (child->is_directory) screen_term_write("[DIR] ");
        else screen_term_write("[FILE] ");
        screen_term_write(child->name);
        screen_term_write("\n");
        child = child->next;
    }
    if (count == 0) screen_term_write("  (empty)\n");
}

void cmd_more(const char *arg) {
    if (!arg || !arg[0]) {
        screen_term_write("Usage: more <filename>\n");
        return;
    }

    vfs_node_t *node = vfs_find_node(arg);
    if (!node) {
        screen_term_write("more: file not found: ");
        screen_term_write(arg);
        screen_term_write("\n");
        return;
    }

    char *buf = (char *)kmalloc(node->size + 1);
    if (!buf) return;

    int bytes = 0;
    if (node->f_op && node->f_op->read)
        bytes = node->f_op->read(node, 0, node->size, buf);
    buf[bytes] = 0;

    int lines_displayed = 0;
    int max_lines = CONTENT_H > 0 ? CONTENT_H : 15;

    for (int i = 0; i <= bytes; i++) {
        if (buf[i] == '\n' || buf[i] == 0) {
            int start = i;
            while (start > 0 && buf[start - 1] != '\n') start--;
            int end = i;
            if (start < end) {
                char saved = buf[end];
                buf[end] = 0;
                screen_term_write(buf + start);
                buf[end] = saved;
            }
            if (buf[i] == '\n') screen_term_putchar('\n');
            lines_displayed++;

            if (lines_displayed >= max_lines) {
                screen_term_write("-- More -- (press any key)");
                keyboard_getchar();
                screen_term_write("\r             \r");
                lines_displayed = 0;
            }
        }
        if (buf[i] == 0) break;
    }

    kfree(buf);
}

void cmd_hexdump(const char *arg) {
    if (!arg || !arg[0]) {
        screen_term_write("Usage: hexdump <filename>\n");
        return;
    }

    vfs_node_t *node = vfs_find_node(arg);
    if (!node) {
        screen_term_write("hexdump: file not found: ");
        screen_term_write(arg);
        screen_term_write("\n");
        return;
    }

    char *buf = (char *)kmalloc(node->size + 1);
    if (!buf) return;

    int bytes = 0;
    if (node->f_op && node->f_op->read)
        bytes = node->f_op->read(node, 0, node->size, buf);

    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < bytes; i += 16) {
        char addr[12];
        int2str(i, addr);
        screen_term_write(addr);
        screen_term_write(": ");

        for (int j = 0; j < 16; j++) {
            if (i + j < bytes) {
                screen_term_putchar(hex[(buf[i + j] >> 4) & 0xF]);
                screen_term_putchar(hex[buf[i + j] & 0xF]);
            } else {
                screen_term_write("  ");
            }
            if (j == 7) screen_term_write("  ");
            else screen_term_write(" ");
        }

        screen_term_write(" |");
        for (int j = 0; j < 16 && i + j < bytes; j++) {
            char c = buf[i + j];
            screen_term_putchar((c >= 0x20 && c < 0x7F) ? c : '.');
        }
        screen_term_write("|\n");
    }

    char sz[16];
    int2str(bytes, sz);
    screen_term_write(sz);
    screen_term_write(" bytes\n");

    kfree(buf);
}
