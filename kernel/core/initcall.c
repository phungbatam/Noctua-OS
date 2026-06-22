#include "initcall.h"
#include "printk.h"

extern initcall_t __start_initcall0_init[];
extern initcall_t __stop_initcall0_init[];
extern initcall_t __start_initcall1_init[];
extern initcall_t __stop_initcall1_init[];
extern initcall_t __start_initcall2_init[];
extern initcall_t __stop_initcall2_init[];
extern initcall_t __start_initcall3_init[];
extern initcall_t __stop_initcall3_init[];
extern initcall_t __start_initcall4_init[];
extern initcall_t __stop_initcall4_init[];
extern initcall_t __start_initcall5_init[];
extern initcall_t __stop_initcall5_init[];
extern initcall_t __start_initcall6_init[];
extern initcall_t __stop_initcall6_init[];
extern initcall_t __start_initcall7_init[];
extern initcall_t __stop_initcall7_init[];

static void do_level(initcall_t *start, initcall_t *stop, const char *name) {
    printk("INITCALL: Running %s initcalls", name);
    for (initcall_t *fn = start; fn < stop; fn++) {
        if (*fn) {
            int ret = (*fn)();
            if (ret)
                printk("  initcall %p returned %d", (void*)*fn, ret);
        }
    }
}

void do_initcalls(void) {
    do_level(__start_initcall0_init, __stop_initcall0_init, "pure");
    do_level(__start_initcall1_init, __stop_initcall1_init, "core");
    do_level(__start_initcall2_init, __stop_initcall2_init, "postcore");
    do_level(__start_initcall3_init, __stop_initcall3_init, "arch");
    do_level(__start_initcall4_init, __stop_initcall4_init, "subsys");
    do_level(__start_initcall5_init, __stop_initcall5_init, "fs");
    do_level(__start_initcall6_init, __stop_initcall6_init, "device");
    do_level(__start_initcall7_init, __stop_initcall7_init, "late");
}
