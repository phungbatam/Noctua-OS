#ifndef NOCTUA_INITCALL_H
#define NOCTUA_INITCALL_H

typedef int (*initcall_t)(void);

#define __define_initcall(fn, id) \
    static initcall_t __initcall_##fn##id __attribute__((__used__)) \
    __attribute__((__section__(".initcall" #id ".init"))) = fn

#define pure_initcall(fn)    __define_initcall(fn, 0)
#define core_initcall(fn)    __define_initcall(fn, 1)
#define postcore_initcall(fn) __define_initcall(fn, 2)
#define arch_initcall(fn)    __define_initcall(fn, 3)
#define subsys_initcall(fn)  __define_initcall(fn, 4)
#define fs_initcall(fn)      __define_initcall(fn, 5)
#define device_initcall(fn)  __define_initcall(fn, 6)
#define late_initcall(fn)    __define_initcall(fn, 7)

void do_initcalls(void);

#endif
