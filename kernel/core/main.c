#include "screen.h"
#include "keyboard.h"
#include "fb.h"
#include "string.h"
#include "ports.h"
#include "cpuid.h"
#include "fat32.h"
#include "rtc.h"
#include "idt.h"
#include "isr.h"

#include "mm/page.h"
#include "mm/paging.h"
#include "mm/heap.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "timer/pit.h"
#include "block/ata.h"
#include "bus/pci.h"
#include "block/partition.h"
#include "block/blockdev.h"
#include "char/serial.h"
#include "timer/cmos.h"
#include "char/pcspkr.h"
#include "block/floppy.h"
#include "syscall.h"
#include "proc/signal.h"
#include "proc/pid.h"
#include "core/init.h"
#include "core/tty.h"
#include "core/elf.h"
#include "net/rtl8139.h"
#include "mm/slab.h"
#include "arch/irq.h"
#include "core/initd.h"
#include "ipc/shm.h"
#include "ipc/msg.h"
#include "printk.h"
#include "editor.h"
#include "net/net.h"
#include "arch/gdt.h"
#include "arch/hw.h"
#include "mm/vm.h"
#include "drivers/block/ahci.h"
#include "fs/blockcache.h"
#include "core/script.h"
#include "core/initcall.h"
#include "core/wait.h"
#include "core/workqueue.h"

/* Commands from editor.c */
void cmd_grep(const char *arg);
void cmd_find(const char *arg);
void cmd_more(const char *arg);
void cmd_hexdump(const char *arg);

/* Network interface callbacks for RTL8139 */
int netif_rtl8139_send(netif_t *ni, const uint8_t *data, uint32_t len) {
    (void)ni;
    return rtl8139_send_packet(data, len);
}

int netif_rtl8139_poll(netif_t *ni, uint8_t *buffer, uint32_t *len) {
    (void)ni;
    return rtl8139_receive_packet(buffer, len);
}

#define BUFSZ 256

static char prompt_line[] = "\nuser@noctua > ";

static unsigned int total_mem_upper = 0;
static unsigned int total_mem_lower = 0;
static char cpu_vendor[16] = "Unknown";
static char cpu_brand[64] = "Unknown";

static void parse_mb_info(unsigned int addr) {
    unsigned int *mb = (unsigned int *)addr;
    if (mb[0] & 1) {
        total_mem_lower = mb[1];
        total_mem_upper = mb[2];
    }
}

static unsigned int total_ram_kb(void) {
    return total_mem_lower + total_mem_upper;
}

static void cmd_help(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Noctua OS v1.0 Commands ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" help       - this help\n clear      - clear terminal\n echo       - echo text\n neofetch   - system info (CPU,RAM,OS)\n info       - quick system info\n banner     - show banner\n calc       - simple calculator\n ls         - list directory\n cd         - change directory\n cat        - view file\n mkdir      - create directory\n touch      - create file\n pwd        - print working directory\n whoami     - show current user\n hostname   - show hostname\n uptime     - system uptime\n free       - memory usage\n ps         - process list\n kill       - kill process\n history    - command history\n env        - environment variables\n export     - set environment var\n about      - about Noctua OS\n color      - set text color\n cow        - cow says moo\n date       - show build date\n creds      - credits\n explore    - virtual file browser\n edit       - text editor (F1:save F2:quit)\n grep       - search text in file\n find       - list all files\n more       - view file page by page\n hexdump    - hex view of file\n ifconfig   - network interface\n ping       - ICMP echo (ping <ip>)\n netstat    - socket connections\n source     - run script file\n reboot     - restart\n shutdown   - power off\n diskinfo   - ATA drive info (GB, model, LBA)\n partitions - partition table\n pci        - PCI devices\n pciverbose - PCI devices (detailed)\n beep       - play beep sound\n cmos       - CMOS/RTC info\n diag       - system diagnostics\n pgup/pgdn  - scroll terminal\n");
}

static void cmd_neofetch(void) {
    char ram_str[16];
    int2str(total_ram_kb() / 1024, ram_str);
    get_cpu_vendor(cpu_vendor);
    if (cpu_vendor[0] == 0) memcpy(cpu_vendor, "Unknown", 8);
    get_cpu_brand(cpu_brand);
    
    /* Clean up CPU brand string - remove trailing spaces */
    int brand_len = strlen(cpu_brand);
    while (brand_len > 0 && cpu_brand[brand_len - 1] == ' ') {
        cpu_brand[brand_len - 1] = 0;
        brand_len--;
    }
    
    screen_set_content_color(C_HEADER);
    screen_term_write("            /\\\n");
    screen_term_write("           /^^\\          ");
    screen_set_content_color(C_WIN_TITLE);
    screen_term_write("-- Noctua OS v1.0 --\n");
    screen_set_content_color(C_HEADER);
    screen_term_write("          /    \\         ");
    screen_set_content_color(C_INFO);
    screen_term_write("OS: Noctua OS x86\n");
    screen_set_content_color(C_HEADER);
    screen_term_write("         /  __  \\        ");
    screen_set_content_color(C_INFO);
    screen_term_write("Kernel: v1.0 (monolithic)\n");
    screen_set_content_color(C_HEADER);
    screen_term_write("        |  (__) |       ");
    screen_set_content_color(C_INFO);
    screen_term_write("CPU Vendor: ");
    screen_term_write(cpu_vendor);
    screen_term_write("\n");
    if (cpu_brand[0]) {
        screen_set_content_color(C_HEADER);
        screen_term_write("         \\____/         ");
        screen_set_content_color(C_INFO);
        screen_term_write("CPU Model: ");
        screen_term_write(cpu_brand);
        screen_term_write("\n");
    } else {
        screen_set_content_color(C_HEADER);
        screen_term_write("         \\____/         ");
        screen_set_content_color(C_INFO);
        screen_term_write("CPU: ");
        screen_term_write(cpu_vendor);
        screen_term_write("\n");
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("                          ");
    screen_set_content_color(C_INFO);
    screen_term_write("RAM: ");
    screen_term_write(ram_str);
    screen_term_write(" MB\n");
    screen_set_content_color(C_HEADER);
    screen_term_write("   __________________     ");
    screen_set_content_color(C_INFO);
    screen_term_write("Display: VGA 640x400 framebuffer\n");
    screen_set_content_color(C_HEADER);
    screen_term_write("                          ");
    screen_set_content_color(C_INFO);
    screen_term_write("Shell: built-in CLI\n");
}

static void cmd_info(void) {
    char ram_str[16];
    int2str(total_ram_kb() / 1024, ram_str);
    get_cpu_vendor(cpu_vendor);
    screen_set_content_color(C_HEADER);
    screen_term_write("=== System Info ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" OS: Noctua OS v1.0\n Arch: x86 (32-bit)\n Mode: Protected Mode\n RAM: ");
    screen_term_write(ram_str);
    screen_term_write(" MB\n CPU Vendor: ");
    screen_term_write(cpu_vendor);
    screen_term_write("\n Display: 640x400 framebuffer\n Input: PS/2 Keyboard\n");
}

static void cmd_banner(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("  ______________________________\n");
    screen_term_write(" |  ___ _   _ _   _  ___  ___ |\n");
    screen_term_write(" | |_ _| \\ | | \\ | |/ _ \\/ _ \\|\n");
    screen_term_write(" |  | ||  \\| |  \\| | |_| | |_||\n");
    screen_term_write(" |  | || |\\  | |\\  |  _| \\__ \\|\n");
    screen_term_write(" | |___|_| \\_|_| \\_|_|   |___/|\n");
    screen_term_write(" |_____________________________|\n");
    screen_set_content_color(C_INFO);
}

static void cmd_about(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Noctua OS v1.0 ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" A unique x86 OS\n C and assembly\n GRUB multiboot\n Shell interface\n");
}

static void cmd_color(const char *arg) {
    if (strcmp(arg, "green") == 0) screen_set_content_color(C_WIN_TEXT);
    else if (strcmp(arg, "white") == 0) screen_set_content_color(C_INPUT);
    else if (strcmp(arg, "cyan") == 0) screen_set_content_color(C_INFO);
    else if (strcmp(arg, "red") == 0) screen_set_content_color(C_ERROR);
    else if (strcmp(arg, "magenta") == 0) screen_set_content_color(C_HEADER);
    else screen_term_write("Colors: green, white, cyan, red, magenta\n");
}

static void cmd_calc(const char *arg) {
    int a = 0, b = 0, i = 0;
    char op = 0;
    while (arg[i] >= '0' && arg[i] <= '9') { a = a * 10 + (arg[i] - '0'); i++; }
    while (arg[i] == ' ') i++;
    if (arg[i] == '+' || arg[i] == '-' || arg[i] == '*' || arg[i] == '/') op = arg[i++];
    while (arg[i] == ' ') i++;
    while (arg[i] >= '0' && arg[i] <= '9') { b = b * 10 + (arg[i] - '0'); i++; }
    screen_set_content_color(C_HEADER);
    screen_term_write(" Calc: ");
    screen_set_content_color(C_INFO);
    char buf[32];
    int res = 0;
    if (op == '+') res = a + b;
    else if (op == '-') res = a - b;
    else if (op == '*') res = a * b;
    else if (op == '/' && b != 0) res = a / b;
    else { screen_term_write("Invalid or div by zero\n"); return; }
    int2str(res, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

static void cmd_cow(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("   --- Noctua OS ---\n");
    screen_set_content_color(BLACK << 4 | BROWN);
    screen_term_write("  /  Moo! \\\n");
    screen_term_write("  \\______/\n");
    screen_term_write("   \\  ^__^\n");
    screen_term_write("    \\ (oo)\\_______\n");
    screen_term_write("      (__)\\       )\\\n");
    screen_term_write("          ||----w |\n");
    screen_term_write("          ||     ||\n");
    screen_set_content_color(C_INFO);
}

static void cmd_reboot(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("Rebooting...\n");
    outb(0x64, 0xFE);
    for (;;) asm("hlt");
}

static void cmd_shutdown(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("Shutting down...\n");
    screen_set_content_color(C_INFO);
    screen_term_write("System will power off now.\n");
    
    /* Disable interrupts */
    asm volatile("cli");
    
    /* Try QEMU isa-debug-exit device (port 0xF4) */
    outl(0x0, 0xF4);  /* Exit code 0 */
    
    /* Wait a bit */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* Try ACPI shutdown */
    outw(0x2000, 0x604);
    outw(0x2000, 0xB004);
    
    /* Wait a bit */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* Try Bochs shutdown (port 0x8900) */
    outw(0x2000, 0x8900);
    
    /* Wait a bit more */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* If all else fails, halt the CPU */
    screen_set_content_color(C_ERROR);
    screen_term_write("Power management not supported.\n");
    screen_term_write("Press Ctrl+C in QEMU to exit.\n");
    for (;;) asm("hlt");
}

static void cmd_date(void) {
    rtc_time_t tm;
    rtc_read_time(&tm);

    screen_set_content_color(C_HEADER);
    screen_term_write("=== Date/Time ===\n");
    screen_set_content_color(C_INFO);

    screen_term_write(rtc_get_month_name(tm.month));
    screen_term_write(" ");
    char buf[8];
    int2str(tm.day, buf);
    screen_term_write(buf);
    screen_term_write(", 20");
    if (tm.year < 10) screen_term_write("0");
    int2str(tm.year, buf);
    screen_term_write(buf);
    screen_term_write("\n");

    if (tm.hour < 10) screen_term_write("0");
    int2str(tm.hour, buf);
    screen_term_write(buf);
    screen_term_write(":");
    if (tm.minute < 10) screen_term_write("0");
    int2str(tm.minute, buf);
    screen_term_write(buf);
    screen_term_write(":");
    if (tm.second < 10) screen_term_write("0");
    int2str(tm.second, buf);
    screen_term_write(buf);
    screen_term_write("\n");
}

static void cmd_creds(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== CREDITS ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" Noctua OS - A unique x86 OS\n Tools: GCC, NASM, LD, GRUB\n Thanks to: OSDev Wiki\n");
}

static void cmd_diskinfo(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== ATA Disk Info ===\n");
    screen_set_content_color(C_INFO);

    int nd = ata_device_count();
    if (nd == 0) {
        screen_term_write("No ATA drives detected\n");
        return;
    }

    for (int i = 0; i < nd; i++) {
        ata_device_t *dev = ata_get_device(i);
        if (!dev || !dev->present) continue;

        char label = 'C' + i;
        screen_term_write(" Drive ");
        char lbl[2] = {label, 0};
        screen_term_write(lbl);
        screen_term_write(":\n");

        screen_term_write("  Model: ");
        if (dev->model[0]) screen_term_write(dev->model);
        else screen_term_write("Unknown");
        screen_term_write("\n");

        if (dev->serial[0]) {
            screen_term_write("  S/N:   ");
            screen_term_write(dev->serial);
            screen_term_write("\n");
        }

        uint64_t sectors = dev->total_sectors;
        uint64_t bytes = sectors * dev->sector_size;
        uint64_t mb = bytes / (1024 * 1024);
        uint64_t gb = mb / 1024;

        screen_term_write("  Size:  ");
        char sz[32];
        if (gb > 0) {
            int2str((int)gb, sz);
            screen_term_write(sz);
            screen_term_write(" GB (");
            int2str((int)mb, sz);
            screen_term_write(sz);
            screen_term_write(" MB)\n");
        } else {
            int2str((int)mb, sz);
            screen_term_write(sz);
            screen_term_write(" MB\n");
        }

        screen_term_write("  LBA:   ");
        screen_term_write(dev->supports_lba48 ? "48-bit" : dev->supports_lba ? "28-bit" : "CHS");
        screen_term_write("\n");

        screen_term_write("  Type:  ");
        screen_term_write(dev->is_atapi ? "ATAPI" : "ATA");
        screen_term_write("\n");
    }
}

static void cmd_partitions(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Partition Table ===\n");
    screen_set_content_color(C_INFO);

    int np = partition_get_count();
    if (np == 0) {
        screen_term_write("No partitions found (run diskinfo first)\n");
        return;
    }

    for (int i = 0; i < np; i++) {
        partition_info_t *p = partition_get(i);
        if (!p || !p->present) continue;

        char buf[32];

        screen_term_write(" ");
        int2str(p->partition_number, buf);
        screen_term_write(buf);
        screen_term_write(": ");

        if (p->bootable) screen_term_write("[BOOT] ");

        if (p->is_gpt) {
            screen_term_write("GPT ");
        } else {
            screen_term_write(p->label);
            screen_term_write(" ");
        }

        screen_term_write("LBA=");
        int2str((int)(p->lba_start & 0xFFFFFFFF), buf);
        screen_term_write(buf);

        screen_term_write(" Size=");
        uint64_t mb = p->size_bytes / (1024 * 1024);
        if (mb >= 1024) {
            int2str((int)(mb / 1024), buf);
            screen_term_write(buf);
            screen_term_write(" GB");
        } else {
            int2str((int)mb, buf);
            screen_term_write(buf);
            screen_term_write(" MB");
        }
        screen_term_write("\n");
    }
}

static void cmd_pci(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== PCI Devices ===\n");
    screen_set_content_color(C_INFO);

    int nd = pci_device_count();
    if (nd == 0) {
        screen_term_write("No PCI devices detected\n");
        return;
    }

    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;

        char buf[32];

        screen_term_write(" ");
        int2str(dev->bus, buf);
        screen_term_write(buf);
        screen_term_write(":");
        int2str(dev->slot, buf);
        screen_term_write(buf);
        screen_term_write(":");
        int2str(dev->func, buf);
        screen_term_write(buf);

        screen_term_write("  ");
        screen_term_write(pci_vendor_name(dev->info.vendor_id));
        screen_term_write(" [");
        const char *class_name = pci_class_name(dev->info.class_code);
        if (class_name) screen_term_write(class_name);
        else screen_term_write("Unknown");
        screen_term_write("]\n");
    }
}

static void cmd_pci_verbose(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== PCI Devices (Detailed) ===\n");
    screen_set_content_color(C_INFO);

    int nd = pci_device_count();
    if (nd == 0) {
        screen_term_write("No PCI devices\n");
        return;
    }

    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;

        char buf[16], buf2[16];

        screen_term_write(" ");
        int2str(dev->bus, buf);
        screen_term_write(buf);
        screen_term_write(":");
        int2str(dev->slot, buf);
        screen_term_write(buf);
        screen_term_write(".");
        int2str(dev->func, buf);
        screen_term_write(buf);

        int2str(dev->info.vendor_id, buf);
        int2str(dev->info.device_id, buf2);
        screen_term_write("  Vendor=0x");
        screen_term_write(buf);
        screen_term_write(" Device=0x");
        screen_term_write(buf2);
        screen_term_write("\n         Class=");
        int2str(dev->info.class_code, buf);
        screen_term_write(buf);
        screen_term_write(" Subclass=");
        int2str(dev->info.subclass, buf);
        screen_term_write(buf);
        screen_term_write(" IF=");
        int2str(dev->info.prog_if, buf);
        screen_term_write(buf);

        screen_term_write(" IRQ=");
        int2str(dev->info.interrupt_line, buf);
        screen_term_write(buf);

        uint32_t bar = dev->info.bar[0];
        if (bar) {
            screen_term_write(" BAR0=0x");
            int2str((int)bar, buf);
            screen_term_write(buf);
        }
        screen_term_write("\n");
    }
}

static void cmd_beep(const char *arg) {
    (void)arg;
    screen_set_content_color(C_INFO);
    screen_term_write("Playing beep...\n");
    pcspkr_beep(440, 200);
    pit_sleep(50);
    pcspkr_beep(523, 200);
    pit_sleep(50);
    pcspkr_beep(659, 400);
}

static void cmd_cmos(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== CMOS Info ===\n");
    screen_set_content_color(C_INFO);

    char buf[16];

    screen_term_write(" Base Mem: ");
    int2str(cmos_get_base_memory(), buf);
    screen_term_write(buf);
    screen_term_write(" KB\n");

    screen_term_write(" Ext Mem:  ");
    int2str(cmos_get_ext_memory(), buf);
    screen_term_write(buf);
    screen_term_write(" KB\n");

    screen_term_write(" Disk1:    Type ");
    int2str(cmos_get_disk1_type(), buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write(" Disk2:    Type ");
    int2str(cmos_get_disk2_type(), buf);
    screen_term_write(buf);
    screen_term_write("\n");

    screen_term_write(" Checksum: ");
    screen_term_write(cmos_checksum_valid() ? "Valid" : "Invalid");
    screen_term_write("\n");
}

static void cmd_diag(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== System Diagnostics ===\n");
    screen_set_content_color(C_INFO);

    char buf[32];
    
    screen_term_write("\n[BOOTLOADER DEBUG INFO]\n");
    screen_term_write(" Serial debug: ENABLED (COM1: 0x3F8)\n");
    screen_term_write(" CPU detection: ENABLED\n");
    screen_term_write(" Memory detection: ENABLED\n");
    
    screen_term_write("\n[KERNEL SUBSYSTEMS]\n");
    screen_term_write(" IDT: ");
    screen_term_write("INITIALIZED\n");
    screen_term_write(" IRQ: ");
    screen_term_write("INSTALLED\n");
    screen_term_write(" Syscall: ");
    screen_term_write("ENABLED\n");
    screen_term_write(" Paging: ");
    screen_term_write("ENABLED\n");
    screen_term_write(" Heap: ");
    int2str(heap_free() / 1024, buf);
    screen_term_write(buf);
    screen_term_write(" KB free\n");
    screen_term_write(" Slab: ");
    int2str(kmem_cache_usage() / 1024, buf);
    screen_term_write(buf);
    screen_term_write(" KB used\n");
    
    screen_term_write("\n[DRIVER STATUS]\n");
    screen_term_write(" Keyboard: ");
    screen_term_write("OK\n");
    screen_term_write(" Serial: ");
    screen_term_write("OK\n");
    screen_term_write(" ATA: ");
    int nd = ata_device_count();
    if (nd > 0) {
        int2str(nd, buf);
        screen_term_write(buf);
        screen_term_write(" device(s) detected\n");
    } else {
        screen_term_write("No devices\n");
    }
    screen_term_write(" PCI: ");
    nd = pci_device_count();
    int2str(nd, buf);
    screen_term_write(buf);
    screen_term_write(" device(s)\n");
    
    screen_term_write("\n[MEMORY STATUS]\n");
    uint32_t tp = pmem_total_pages();
    uint32_t fp = pmem_free_pages();
    int2str((tp * 4) / 1024, buf);
    screen_term_write(" Total: ");
    screen_term_write(buf);
    screen_term_write(" MB\n");
    int2str((fp * 4) / 1024, buf);
    screen_term_write(" Free: ");
    screen_term_write(buf);
    screen_term_write(" MB\n");
    
    screen_term_write("\n[TASK SYSTEM]\n");
    int task_count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = task_get(i);
        if (t) task_count++;
    }
    int2str(task_count, buf);
    screen_term_write(" Active tasks: ");
    screen_term_write(buf);
    screen_term_write("\n");
    
    screen_set_content_color(C_HEADER);
    screen_term_write("\n=== Diagnostics Complete ===\n");
    screen_set_content_color(C_INFO);
}

static void cmd_explore(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== File Browser ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" / (root)\n");
    screen_term_write("  +-- bin/        [shell, utils]\n");
    screen_term_write("  +-- home/\n");
    screen_term_write("  |   +-- user/\n");
    screen_term_write("  |       +-- readme.txt  'Welcome to Noctua OS'\n");
    screen_term_write("  +-- system/\n");
    screen_term_write("  |   +-- version         'v1.0'\n");
    screen_term_write("  |   +-- config          'default'\n");
    screen_term_write("  +-- tmp/         [empty]\n");
    screen_set_content_color(C_HEADER);
    screen_term_write("\n Virtual filesystem\n");
    if (partition_get_count() > 0) {
        screen_set_content_color(C_INFO);
        screen_term_write(" Real disk partitions detected - file content from ATA\n");
    } else {
        screen_set_content_color(C_ERROR);
        screen_term_write(" No disk partition found\n");
    }
}

static void cmd_ls(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Directory Listing ===\n");
    screen_set_content_color(C_INFO);

    vfs_node_t *root = vfs_get_root();
    if (!root) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Filesystem not initialized\n");
        return;
    }

    vfs_node_t *child = root->children;
    while (child) {
        /* Print directory name without any trailing separator */
        screen_term_write(child->name);
        if (child->is_directory) {
            screen_term_write("/");
        } else {
            char size_str[16];
            int2str(child->size, size_str);
            screen_term_write(" (");
            screen_term_write(size_str);
            screen_term_write(" bytes)");
        }
        screen_term_write("\n");
        child = child->next;
    }
    
    screen_set_content_color(C_INFO);
    screen_term_write("\nCurrent directory: /\n");
}

static void cmd_cd(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: cd <directory>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Filesystem not initialized\n");
        return;
    }

    /* Simple implementation - only support root for now */
    if (strcmp(arg, "/") == 0 || strcmp(arg, ".") == 0) {
        screen_set_content_color(C_INFO);
        screen_term_write("Current directory: /\n");
        return;
    }

    /* Try to find the directory */
    vfs_node_t *child = root->children;
    while (child) {
        if (strcmp(child->name, arg) == 0 && child->is_directory) {
            screen_set_content_color(C_INFO);
            screen_term_write("Changed to: ");
            screen_term_write(arg);
            screen_term_write("/\n");
            return;
        }
        child = child->next;
    }

    screen_set_content_color(C_ERROR);
    screen_term_write("Directory not found: ");
    screen_term_write(arg);
    screen_term_write("\n");
}

static void cmd_cat(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: cat <filename>\n");
        return;
    }

    char path[FAT_MAX_PATH];
    strcpy(path, "/");
    strcat(path, arg);

    vfs_node_t *node = vfs_find_node(path);
    if (!node) {
        screen_set_content_color(C_ERROR);
        screen_term_write("File not found: ");
        screen_term_write(arg);
        screen_term_write("\n");
        return;
    }

    if (node->is_directory) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Is a directory\n");
        return;
    }

    screen_set_content_color(C_HEADER);
    screen_term_write("=== ");
    screen_term_write(arg);
    screen_term_write(" ===\n");
    screen_set_content_color(C_INFO);

    if (node->size == 0) {
        screen_term_write("[empty file]\n");
        return;
    }

    char *buffer = (char *)kmalloc(node->size + 1);
    if (!buffer) {
        screen_term_write("(out of memory)\n");
        return;
    }

    int bytes_read = 0;
    if (node->f_op && node->f_op->read) {
        bytes_read = node->f_op->read(node, 0, node->size, buffer);
    }

    if (bytes_read > 0) {
        buffer[bytes_read] = 0;
        screen_term_write(buffer);
        if (buffer[bytes_read - 1] != '\n') screen_term_write("\n");
    } else {
        char size_str[16];
        int2str(node->size, size_str);
        screen_term_write("File size: ");
        screen_term_write(size_str);
        screen_term_write(" bytes\n");
        screen_term_write("(virtual filesystem - no disk content)\n");
    }

    kfree(buffer);
}

static void cmd_mkdir(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: mkdir <directory>\n");
        return;
    }

    screen_set_content_color(C_HEADER);
    screen_term_write("Creating directory: ");
    screen_term_write(arg);
    screen_term_write("\n");
    screen_set_content_color(C_INFO);

    char path[FAT_MAX_PATH];
    strcpy(path, "/");
    strcat(path, arg);

    vfs_node_t *node = vfs_create_node(path, 1);
    if (node) {
        screen_term_write("Directory created successfully\n");
    } else {
        screen_set_content_color(C_ERROR);
        screen_term_write("Failed to create directory\n");
    }
}

static void cmd_touch(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: touch <filename>\n");
        return;
    }

    screen_set_content_color(C_HEADER);
    screen_term_write("Creating file: ");
    screen_term_write(arg);
    screen_term_write("\n");
    screen_set_content_color(C_INFO);

    char path[FAT_MAX_PATH];
    strcpy(path, "/");
    strcat(path, arg);

    vfs_node_t *node = vfs_create_node(path, 0);
    if (node) {
        screen_term_write("File created successfully\n");
    } else {
        screen_set_content_color(C_ERROR);
        screen_term_write("Failed to create file\n");
    }
}

static void cmd_pwd(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("/\n");
}

static void cmd_whoami(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("user\n");
}

static void cmd_hostname(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("tvn-os\n");
}

static void cmd_uptime(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== System Uptime ===\n");
    screen_set_content_color(C_INFO);

    uint32_t uptime = uptime_get_seconds();
    uint32_t hours = uptime / 3600;
    uint32_t minutes = (uptime % 3600) / 60;
    uint32_t seconds = uptime % 60;

    char buf[16];
    int2str(hours, buf);
    screen_term_write(buf);
    screen_term_write("h ");
    int2str(minutes, buf);
    screen_term_write(buf);
    screen_term_write("m ");
    int2str(seconds, buf);
    screen_term_write(buf);
    screen_term_write("s\n");
}

static void cmd_free(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Memory Usage ===\n");
    screen_set_content_color(C_INFO);

    char buf[16];

    uint32_t tp = pmem_total_pages();
    uint32_t up = pmem_used_pages();
    uint32_t fp = pmem_free_pages();

    int2str((tp * 4) / 1024, buf);
    screen_term_write(" Total RAM: ");
    screen_term_write(buf);
    screen_term_write(" MB\n");

    int2str((up * 4) / 1024, buf);
    screen_term_write(" Used:      ");
    screen_term_write(buf);
    screen_term_write(" MB\n");

    int2str((fp * 4) / 1024, buf);
    screen_term_write(" Free:      ");
    screen_term_write(buf);
    screen_term_write(" MB\n");

    int2str(heap_used() / 1024, buf);
    screen_term_write(" Heap used: ");
    screen_term_write(buf);
    screen_term_write(" KB\n");

    int2str(heap_free() / 1024, buf);
    screen_term_write(" Heap free: ");
    screen_term_write(buf);
    screen_term_write(" KB\n");

    int2str(kmem_cache_usage() / 1024, buf);
    screen_term_write(" Slab used: ");
    screen_term_write(buf);
    screen_term_write(" KB\n");
}

static void cmd_ps(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Tasks ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" PID  NAME            STATE    NICE  VRUNTIME\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = task_get(i);
        if (!t) continue;
        char buf[16];
        int2str(t->pid, buf);
        screen_term_write(" ");
        screen_term_write(buf);
        if (t->pid < 10) screen_term_write(" ");
        screen_term_write("  ");
        screen_term_write(t->name);
        for (int j = strlen(t->name); j < 15; j++) screen_term_write(" ");
        const char *s = "ready";
        if (t->state == TASK_RUNNING) s = "running";
        else if (t->state == TASK_BLOCKED) s = "blocked";
        else if (t->state == TASK_SLEEPING) s = "sleeping";
        else if (t->state == TASK_ZOMBIE) s = "zombie";
        screen_term_write(s);
        char nicebuf[8];
        int2str(t->nice, nicebuf);
        screen_term_write("  ");
        screen_term_write(nicebuf);
        if (t->nice >= 0 && t->nice < 10) screen_term_write(" ");
        char vrbuf[16];
        int2str(t->vruntime, vrbuf);
        screen_term_write("  ");
        screen_term_write(vrbuf);
        screen_term_write("\n");
    }
}

static void cmd_kill(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: kill <pid>\n");
        return;
    }

    int pid = 0, i = 0;
    while (arg[i] >= '0' && arg[i] <= '9') {
        pid = pid * 10 + (arg[i] - '0');
        i++;
    }

    task_t *target = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *t = task_get(i);
        if (t && t->pid == pid) { target = t; break; }
    }

    if (!target) {
        screen_set_content_color(C_ERROR);
        screen_term_write("No task with PID ");
        char buf[8];
        int2str(pid, buf);
        screen_term_write(buf);
        screen_term_write("\n");
        return;
    }

    signal_send(target, SIGKILL);
    screen_set_content_color(C_INFO);
    screen_term_write("Sent SIGKILL to PID ");
    char buf[8];
    int2str(pid, buf);
    screen_term_write(buf);
    screen_term_write(" (");
    screen_term_write(target->name);
    screen_term_write(")\n");
}

#define HIST_SIZE 64
static char history[HIST_SIZE][BUFSZ];
static int hist_count = 0;
static int hist_pos = 0;

static void hist_add(const char *cmd) {
    if (cmd[0] == 0) return;
    int i;
    for (i = 0; cmd[i] && i < BUFSZ - 1; i++) history[hist_pos][i] = cmd[i];
    history[hist_pos][i] = 0;
    hist_pos = (hist_pos + 1) % HIST_SIZE;
    if (hist_count < HIST_SIZE) hist_count++;
}

static void cmd_history(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Command History ===\n");
    screen_set_content_color(C_INFO);

    if (hist_count == 0) {
        screen_term_write("(no commands yet)\n");
        return;
    }

    int start = hist_pos - hist_count;
    if (start < 0) start += HIST_SIZE;

    for (int i = 0; i < hist_count; i++) {
        int idx = (start + i) % HIST_SIZE;
        char buf[8];
        int2str(i + 1, buf);
        screen_term_write(" ");
        screen_term_write(buf);
        screen_term_write("  ");
        screen_term_write(history[idx]);
        screen_term_write("\n");
    }
}

static void cmd_alias(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: alias <name>=<command>\n");
        return;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("Aliases not yet implemented\n");
}

static void cmd_env(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Environment Variables ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" PATH=/bin:/usr/bin\n");
    screen_term_write(" HOME=/home/user\n");
    screen_term_write(" USER=user\n");
    screen_term_write(" SHELL=/bin/tvnsh\n");
    screen_term_write(" TERM=tvn-term\n");
}

static void cmd_export(const char *arg) {
    if (!arg || arg[0] == 0) {
        screen_set_content_color(C_ERROR);
        screen_term_write("Usage: export <VAR>=<value>\n");
        return;
    }
    screen_set_content_color(C_HEADER);
    screen_term_write("Setting environment: ");
    screen_term_write(arg);
    screen_term_write("\n");
    screen_set_content_color(C_ERROR);
    screen_term_write("Environment variables not yet implemented\n");
}

static void cmd_ifconfig(void) {
    netif_t *ni = netif_get("eth0");
    if (!ni) {
        screen_term_write(" No network interfaces\n");
        return;
    }
    char ip_str[16], mac_str[18], gw_str[16], mask_str[16];
    net_ip_to_str(ni->ip_addr, ip_str);
    net_mac_to_str(ni->hw_addr, mac_str);
    net_ip_to_str(ni->gateway, gw_str);
    net_ip_to_str(ni->netmask, mask_str);

    screen_term_write(" eth0: flags=");
    char flags_str[8];
    int2str(ni->flags, flags_str);
    screen_term_write(flags_str);
    screen_term_write("  mtu 1500\n");
    screen_term_write("     inet ");
    screen_term_write(ip_str);
    screen_term_write("  netmask ");
    screen_term_write(mask_str);
    screen_term_write("  gateway ");
    screen_term_write(gw_str);
    screen_term_write("\n     ether ");
    screen_term_write(mac_str);
    screen_term_write("\n");
}

static void cmd_ping(const char *arg) {
    uint8_t ip[4];
    if (net_str_to_ip(arg, ip) < 0) {
        screen_term_write(" Usage: ping <ip>\n");
        return;
    }
    netif_t *ni = netif_get("eth0");
    if (!ni) {
        screen_term_write(" No network interface\n");
        return;
    }
    screen_term_write(" PING ");
    screen_term_write(arg);
    screen_term_write(" 56(84) bytes of data.\n");
    icmp_ping(ni, ip, 4);
    screen_term_write(" --- ");
    screen_term_write(arg);
    screen_term_write(" ping statistics ---\n");
}

static void cmd_netstat(void) {
    screen_term_write(" Active Internet connections\n");
    for (int i = 0; i < socket_count; i++) {
        if (!sockets[i].used) continue;
        char str[16];
        screen_term_write("  ");
        if (sockets[i].type == SOCK_STREAM) screen_term_write("tcp");
        else if (sockets[i].type == SOCK_DGRAM) screen_term_write("udp");
        else screen_term_write("raw");
        screen_term_write("  ");

        net_ip_to_str(sockets[i].local.ip, str);
        screen_term_write(str);
        screen_term_write(":");
        int2str(sockets[i].local.port, str);
        screen_term_write(str);
        screen_term_write("  ");

        net_ip_to_str(sockets[i].remote.ip, str);
        screen_term_write(str);
        screen_term_write(":");
        int2str(sockets[i].remote.port, str);
        screen_term_write(str);
        screen_term_write("  ");

        const char *states[] = {"FREE","OPEN","BOUND","LISTEN","CONNECTING","CONNECTED","CLOSING","CLOSED"};
        int st = sockets[i].state;
        screen_term_write(states[st < 8 ? st : 0]);
        screen_term_write("\n");
    }
}

void execute(const char *cmd) {
    if (strncmp(cmd, "help", 4) == 0) { cmd_help(); }
    else if (strncmp(cmd, "neofetch", 8) == 0) { cmd_neofetch(); }
    else if (strncmp(cmd, "clear", 5) == 0) { screen_clear_content(); }
    else if (strncmp(cmd, "echo ", 5) == 0) { screen_term_write(cmd + 5); }
    else if (strncmp(cmd, "info", 4) == 0 && (cmd[4] == 0 || cmd[4] == ' ')) { cmd_info(); }
    else if (strncmp(cmd, "banner", 6) == 0) { cmd_banner(); }
    else if (strncmp(cmd, "about", 5) == 0) { cmd_about(); }
    else if (strncmp(cmd, "color ", 6) == 0) { cmd_color(cmd + 6); }
    else if (strncmp(cmd, "calc ", 5) == 0) { cmd_calc(cmd + 5); }
    else if (strncmp(cmd, "ls", 2) == 0 && (cmd[2] == 0 || cmd[2] == ' ')) { cmd_ls(); }
    else if (strncmp(cmd, "cd ", 3) == 0) { cmd_cd(cmd + 3); }
    else if (strncmp(cmd, "cat ", 4) == 0) { cmd_cat(cmd + 4); }
    else if (strncmp(cmd, "mkdir ", 6) == 0) { cmd_mkdir(cmd + 6); }
    else if (strncmp(cmd, "touch ", 6) == 0) { cmd_touch(cmd + 6); }
    else if (strncmp(cmd, "pwd", 3) == 0) { cmd_pwd(); }
    else if (strncmp(cmd, "whoami", 6) == 0) { cmd_whoami(); }
    else if (strncmp(cmd, "hostname", 8) == 0) { cmd_hostname(); }
    else if (strncmp(cmd, "uptime", 6) == 0) { cmd_uptime(); }
    else if (strncmp(cmd, "free", 4) == 0) { cmd_free(); }
    else if (strncmp(cmd, "ps", 2) == 0 && (cmd[2] == 0 || cmd[2] == ' ')) { cmd_ps(); }
    else if (strncmp(cmd, "kill ", 5) == 0) { cmd_kill(cmd + 5); }
    else if (strncmp(cmd, "history", 7) == 0) { cmd_history(); }
    else if (strncmp(cmd, "alias ", 6) == 0) { cmd_alias(cmd + 6); }
    else if (strncmp(cmd, "env", 3) == 0) { cmd_env(); }
    else if (strncmp(cmd, "export ", 7) == 0) { cmd_export(cmd + 7); }
    else if (strncmp(cmd, "cow", 3) == 0) { cmd_cow(); }
    else if (strncmp(cmd, "reboot", 6) == 0) { cmd_reboot(); }
    else if (strncmp(cmd, "shutdown", 8) == 0) { cmd_shutdown(); }
    else if (strncmp(cmd, "date", 4) == 0) { cmd_date(); }
    else if (strncmp(cmd, "creds", 5) == 0) { cmd_creds(); }
    else if (strncmp(cmd, "explore", 7) == 0) { cmd_explore(); }
    else if (strncmp(cmd, "edit ", 5) == 0) { editor_open(cmd + 5); }
    else if (strncmp(cmd, "grep ", 5) == 0) { cmd_grep(cmd + 5); }
    else if (strncmp(cmd, "find", 4) == 0 && (cmd[4] == 0 || cmd[4] == ' ')) { cmd_find(cmd + 4); }
    else if (strncmp(cmd, "more ", 5) == 0) { cmd_more(cmd + 5); }
    else if (strncmp(cmd, "hexdump ", 8) == 0) { cmd_hexdump(cmd + 8); }
    else if (strncmp(cmd, "diskinfo", 8) == 0) { cmd_diskinfo(); }
    else if (strncmp(cmd, "partitions", 10) == 0) { cmd_partitions(); }
    else if (strncmp(cmd, "pci", 3) == 0 && (cmd[3] == 0 || cmd[3] == ' ')) { cmd_pci(); }
    else if (strncmp(cmd, "pciverbose", 10) == 0) { cmd_pci_verbose(); }
    else if (strncmp(cmd, "beep", 4) == 0) { cmd_beep(cmd + 4); }
    else if (strncmp(cmd, "cmos", 4) == 0) { cmd_cmos(); }
    else if (strncmp(cmd, "diag", 4) == 0) { cmd_diag(); }
    else if (strncmp(cmd, "ifconfig", 8) == 0) { cmd_ifconfig(); }
    else if (strncmp(cmd, "ping ", 5) == 0) { cmd_ping(cmd + 5); }
    else if (strcmp(cmd, "netstat") == 0) { cmd_netstat(); }
    else if (strncmp(cmd, "source ", 7) == 0) { script_run_file(cmd + 7); }
    else if (strncmp(cmd, "export ", 7) == 0) {
        const char *eq_p = cmd + 7;
        while (*eq_p && *eq_p != '=') eq_p++;
        if (*eq_p == '=') {
            char ename[64], eval[256];
            int i;
            for (i = 0; cmd[7 + i] && cmd[7 + i] != '=' && i < 63; i++)
                ename[i] = cmd[7 + i];
            ename[i] = 0;
            for (i = 0; eq_p[1 + i] && i < 255; i++)
                eval[i] = eq_p[1 + i];
            eval[i] = 0;
            script_set_var(ename, eval);
        }
    }
    else if (cmd[0] == 0) { }
    else {
        screen_set_content_color(C_ERROR);
        screen_term_write(" Unknown: ");
        screen_term_write(cmd);
        screen_term_write(" ('help')\n");
    }
}

static void task_demo_entry(void) {
    for (;;) {
        task_yield();
    }
}

/* Initcall examples */
static int initcall_early_printk(void) {
    printk("INITCALL: pure_initcall(initcall_early_printk) ran");
    return 0;
}
pure_initcall(initcall_early_printk);

static int initcall_gdt_test(void) {
    printk("INITCALL: core_initcall(gdt) - GDT test: CS=0x%x", 0x08);
    return 0;
}
core_initcall(initcall_gdt_test);

static work_t wq_test_work;

static void wq_test_cb(void *data) {
    printk("WQ: work executed, data=%s", (const char *)data);
}

static wait_queue_head_t test_wq;

void kmain(unsigned int mb_info) {
    parse_mb_info(mb_info);
    get_cpu_vendor(cpu_vendor);
    get_cpu_brand(cpu_brand);
    
    screen_init(mb_info);
    keyboard_init();
    serial_init();

    printk_init();

    printk("Noctua OS v1.0 (x86, 32-bit, Protected Mode) (gcc 11.2.0)");
    printk("Command line: built-in shell interface");
    printk("KERNEL supported CPUs:");
    printk("  Intel GenuineIntel");
    printk("  AMD AuthenticAMD");

    printk("BIOS-provided physical RAM map: [mem 0x00000000-0x0009ffff] usable (%d KB total)", total_ram_kb());

    if (cpu_brand[0])
        printk("CPU: %s %s", cpu_vendor, cpu_brand);
    else
        printk("CPU: %s", cpu_vendor);

    printk("Noctua OS Kernel - Initializing core subsystems...");

    gdt_init();
    printk("GDT: Global Descriptor Table with ring 3 segments initialized");

    idt_init();
    printk("IDT: Interrupt Descriptor Table initialized");
    
    irq_install();
    printk("IRQ: Base interrupt handlers installed");

    syscall_init_idt();
    syscall_init();
    printk("SYSCALL: System call interface initialized");

    pmem_init(total_mem_upper, total_mem_lower);
    printk("PMM: Physical memory manager initialized (%d KB)", total_ram_kb());

    paging_init();
    printk("PAGING: Paging enabled");

    heap_init((void *)KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    printk("HEAP: Kernel heap allocator initialized at 0x%x", KERNEL_HEAP_START);

    slab_init();
    printk("SLAB: Slab allocator initialized");

    vm_init();
    printk("VM: Virtual memory manager initialized");

    pit_init(TICK_HZ);
    printk_set_pit_ready();
    printk("TIMER: PIT timer initialized (%d Hz)", TICK_HZ);

    irq_init();
    printk("IRQ: Interrupt system enabled");

    task_init_system();
    sched_init();
    printk("SCHED: Task scheduler initialized");

    task_create("demo", task_demo_entry, 1);
    printk("TASK: Demo kernel task created");

    do_initcalls();

    init_waitqueue_head(&test_wq);
    printk("WAIT: Wait queue subsystem initialized");

    INIT_LIST_HEAD(&system_wq.head);
    system_wq.running = 1;
    INIT_WORK(&wq_test_work, wq_test_cb, "hello workqueue");
    schedule_work(&system_wq, &wq_test_work);
    workqueue_thread();
    printk("WQ: Workqueue subsystem initialized and tested");

    ata_init();
    printk("ATA: ATA driver initialized");

    partition_init();
    printk("PART: Partition table parsed");

    int fat_dev = -1;
    for (int i = 0; i < partition_get_count(); i++) {
        partition_info_t *p = partition_get(i);
        if (p && p->present && (p->type == 0x0B || p->type == 0x0C)) {
            int ata_idx = 0;
            fat_dev = blockdev_register(ata_idx, p->lba_start, p->sector_count);
            break;
        }
    }

    if (fat_dev >= 0) {
        fat_init(blockdev_get(fat_dev));
        printk("FS: FAT32 filesystem mounted from disk (dev=%d)", fat_dev);
    }
    else {
        fat_init(0);
        printk("FS: FAT32 initialized (virtual mode)");
    }

    rtc_init();
    printk("RTC: Real-time clock initialized");

    uptime_init();
    printk("RTC: Uptime counter started");

    tty_init();
    printk("TTY: Terminal interface initialized");

    shm_init();
    msg_init();
    printk("IPC: Shared memory and message queues initialized");

    bcache_init();
    printk("CACHE: Block cache layer initialized");

    script_init();
    printk("SCRIPT: Shell scripting engine initialized");

    initd_init();
    initd_main();
    printk("INIT: Init system started");

    int nd = pci_device_count();
    printk("PCI: PCI bus scan complete (%d devices)", nd);

    ahci_init();
    
    for (int i = 0; i < nd; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (dev && dev->present &&
            dev->info.vendor_id == 0x10EC && dev->info.device_id == 0x8139) {
            uint32_t bar0 = dev->info.bar[0];
            if (bar0 & 1) {
                rtl8139_init(bar0 & ~3);
                rtl8139_register_irq(dev->info.interrupt_line);
                printk("NET: RTL8139 network card initialized at IO 0x%x", bar0 & ~3);
            }
        }
    }

    net_init();

    {
        static netif_t eth0;
        extern rtl8139_t rtl8139_dev;
        strcpy(eth0.name, "eth0");
        memcpy(eth0.hw_addr, rtl8139_dev.mac, 6);
        eth0.ip_addr[0] = 10;
        eth0.ip_addr[1] = 0;
        eth0.ip_addr[2] = 2;
        eth0.ip_addr[3] = 15;
        eth0.netmask[0] = 255;
        eth0.netmask[1] = 255;
        eth0.netmask[2] = 255;
        eth0.netmask[3] = 0;
        eth0.gateway[0] = 10;
        eth0.gateway[1] = 0;
        eth0.gateway[2] = 2;
        eth0.gateway[3] = 1;
        eth0.flags = NETIF_F_UP;
        eth0.send = netif_rtl8139_send;
        eth0.poll = netif_rtl8139_poll;
        netif_register(&eth0);
        printk("NET: Interface eth0 configured (10.0.2.15) MAC %02x:%02x:%02x:%02x:%02x:%02x",
               eth0.hw_addr[0], eth0.hw_addr[1], eth0.hw_addr[2],
               eth0.hw_addr[3], eth0.hw_addr[4], eth0.hw_addr[5]);
    }

    init_start();
    printk("INIT: User-space init process started");

    screen_set_content_color(C_WIN_TITLE);
    screen_term_write("\n Noctua OS v1.0 - Kernel Console\n");
    screen_set_content_color(C_INFO);
    screen_term_write(" Type 'help' for available commands\n");

    screen_set_content_color(C_PROMPT);
    screen_term_write(prompt_line);
    screen_set_content_color(C_INPUT);

    char buf[BUFSZ];
    int pos = 0;
    buf[0] = 0;

    while (1) {
        signal_check();

        int k = keyboard_getchar_nb();

        if (k == 0) continue;

        if (k == K_PGUP) {
            screen_scroll_up();
            continue;
        }

        if (k == K_PGDN) {
            screen_scroll_down();
            continue;
        }

        if (k == K_END && screen_scroll_offset() > 0) {
            screen_scroll_reset();
            continue;
        }

        if (k == K_CTRL_C) {
            signal_send(task_current(), SIGINT);
            continue;
        }

        if (k == '\n') {
            buf[pos] = 0;
            hist_add(buf);
            execute(buf);
            pos = 0;
            buf[0] = 0;
            screen_draw_header();
            screen_set_content_color(C_PROMPT);
            screen_term_write(prompt_line);
            screen_set_content_color(C_INPUT);
        }

        else if (k == '\b') {
            if (pos > 0) {
                pos--;
                screen_term_putchar('\b');
            }
        }

        else if (k >= 0x20 && k < 0x80) {
            if (pos < BUFSZ - 1) {
                buf[pos++] = k;
                buf[pos] = 0;
                screen_term_putchar(k);
            }
        }
    }
}
