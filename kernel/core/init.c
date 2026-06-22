#include "init.h"
#include "screen.h"
#include "fb.h"
#include "string.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "core/elf.h"
#include "fs/fat32.h"

/* Init process: the first user-space process that sets up the system */
static void init_process_entry(void) {
    screen_set_content_color(C_WIN_TITLE);
    screen_term_write("\n=== Noctua OS Init Process ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Loading system services...\n");

    /* Mount filesystem if available */
    screen_term_write("Mounting virtual filesystem...\n");
    vfs_node_t *root = vfs_get_root();
    if (root) {
        screen_term_write("Filesystem ready - root node at /");
        screen_term_write("\n");
    }

    /* Try loading ELF binaries from VFS */
    screen_set_content_color(C_HEADER);
    screen_term_write("Checking for user-space programs...\n");
    screen_set_content_color(C_INFO);

    vfs_node_t *bin_dir = vfs_find_node("/bin");
    if (bin_dir && bin_dir->children) {
        screen_term_write("Programs found in /bin:\n");
        vfs_node_t *prog = bin_dir->children;
        while (prog) {
            screen_term_write("  - ");
            screen_term_write(prog->name);
            screen_term_write("\n");
            prog = prog->next;
        }
    } else {
        screen_term_write("No user programs found (virtual mode)\n");
        screen_term_write("Create ELF binaries and place in /bin/\n");
    }

    screen_set_content_color(C_INFO);
    screen_term_write("\nInit complete. System ready.\n");
    screen_set_content_color(C_PROMPT);
    screen_term_write("\n $ ");

    /* Become a background idle task */
    for (;;) {
        task_yield();
    }
}

void init_start(void) {
    int pid = task_create("init", init_process_entry, 5);
    if (pid < 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("INIT: Failed to create init process\n");
    } else {
        screen_set_content_color(C_INFO);
        screen_term_write("INIT: Created init process (PID ");
        char buf[8];
        int2str(pid, buf);
        screen_term_write(buf);
        screen_term_write(")\n");
    }
}
