#include "initd.h"
#include "screen.h"
#include "lib/string.h"
#include "proc/task.h"
#include "proc/sched.h"
#include "fs/fat32.h"
#include "core/elf.h"

#define MAX_SERVICES 16
#define MAX_RUNLEVELS 7

typedef struct {
    char name[64];
    int enabled;
    int pid;
    int runlevel;
    void (*start_func)(void);
    void (*stop_func)(void);
} service_t;

static service_t services[MAX_SERVICES];
static int service_count = 0;
static int current_runlevel = 3; /* Default to multi-user */

void initd_init(void) {
    memset(services, 0, sizeof(services));
    service_count = 0;
    current_runlevel = 3;
}

int initd_register_service(const char *name, int runlevel, 
                           void (*start)(void), void (*stop)(void)) {
    if (service_count >= MAX_SERVICES) return -1;
    
    service_t *svc = &services[service_count++];
    strncpy(svc->name, name, sizeof(svc->name) - 1);
    svc->name[sizeof(svc->name) - 1] = 0;
    svc->enabled = 1;
    svc->pid = 0;
    svc->runlevel = runlevel;
    svc->start_func = start;
    svc->stop_func = stop;
    
    return service_count - 1;
}

int initd_start_service(const char *name) {
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            if (services[i].start_func) {
                services[i].start_func();
                services[i].enabled = 1;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

int initd_stop_service(const char *name) {
    for (int i = 0; i < service_count; i++) {
        if (strcmp(services[i].name, name) == 0) {
            if (services[i].stop_func) {
                services[i].stop_func();
                services[i].enabled = 0;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

int initd_set_runlevel(int level) {
    if (level < 0 || level > 6) return -1;
    
    /* Stop services for old runlevel */
    for (int i = 0; i < service_count; i++) {
        if (services[i].runlevel == current_runlevel && services[i].enabled) {
            initd_stop_service(services[i].name);
        }
    }
    
    current_runlevel = level;
    
    /* Start services for new runlevel */
    for (int i = 0; i < service_count; i++) {
        if (services[i].runlevel == level) {
            initd_start_service(services[i].name);
        }
    }
    
    return 0;
}

int initd_get_runlevel(void) {
    return current_runlevel;
}

void initd_list_services(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Services ===\n");
    screen_set_content_color(C_INFO);
    
    for (int i = 0; i < service_count; i++) {
        screen_term_write(services[i].name);
        screen_term_write(" - ");
        screen_term_write(services[i].enabled ? "RUNNING" : "STOPPED");
        screen_term_write(" (runlevel ");
        char buf[8];
        int2str(services[i].runlevel, buf);
        screen_term_write(buf);
        screen_term_write(")\n");
    }
}

/* Built-in services */
static void service_network(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("Starting network service...\n");
    /* Network initialization would go here */
}

static void service_ata(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("Starting ATA service...\n");
    /* ATA initialization would go here */
}

static void service_filesystem(void) {
    screen_set_content_color(C_INFO);
    screen_term_write("Starting filesystem service...\n");
    /* Filesystem initialization would go here */
}

void initd_register_builtin_services(void) {
    initd_register_service("network", 3, service_network, NULL);
    initd_register_service("ata", 2, service_ata, NULL);
    initd_register_service("filesystem", 2, service_filesystem, NULL);
}

void initd_main(void) {
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Noctua OS Init System ===\n");
    screen_set_content_color(C_INFO);
    screen_term_write("Initializing services for runlevel ");
    char buf[8];
    int2str(current_runlevel, buf);
    screen_term_write(buf);
    screen_term_write("...\n");
    
    initd_register_builtin_services();
    
    /* Start services for current runlevel */
    for (int i = 0; i < service_count; i++) {
        if (services[i].runlevel == current_runlevel) {
            initd_start_service(services[i].name);
        }
    }
    
    screen_set_content_color(C_HEADER);
    screen_term_write("=== Init Complete ===\n");
    screen_set_content_color(C_INFO);
}
