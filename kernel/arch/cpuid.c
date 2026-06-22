#include "cpuid.h"

int cpuid_supported(void) {
    int id;
    asm volatile("pushfl; popl %0; movl %0, %%ecx; xorl $0x200000, %0; pushl %0; popfl; pushfl; popl %0; xorl %%ecx, %0"
        : "=r"(id) :: "ecx");
    return id != 0;
}

void cpuid_string(int code, unsigned int *a, unsigned int *b, unsigned int *c, unsigned int *d) {
    asm volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(code));
}

void get_cpu_vendor(char *buf) {
    unsigned int a, b, c, d;
    if (!cpuid_supported()) {
        buf[0] = 0;
        return;
    }
    cpuid_string(0, &a, &b, &c, &d);
    *(unsigned int *)(buf + 0) = b;
    *(unsigned int *)(buf + 4) = d;
    *(unsigned int *)(buf + 8) = c;
    buf[12] = 0;
}

void get_cpu_brand(char *buf) {
    unsigned int a[3], b[3], c[3], d[3], ext = 0;
    if (!cpuid_supported()) { buf[0] = 0; return; }
    cpuid_string(0x80000000, &ext, &b[0], &c[0], &d[0]);
    if (ext < 0x80000004) { buf[0] = 0; return; }
    cpuid_string(0x80000002, &a[0], &b[0], &c[0], &d[0]);
    cpuid_string(0x80000003, &a[1], &b[1], &c[1], &d[1]);
    cpuid_string(0x80000004, &a[2], &b[2], &c[2], &d[2]);
    for (int i = 0; i < 3; i++) {
        *(unsigned int *)(buf + i * 16 + 0) = a[i];
        *(unsigned int *)(buf + i * 16 + 4) = b[i];
        *(unsigned int *)(buf + i * 16 + 8) = c[i];
        *(unsigned int *)(buf + i * 16 + 12) = d[i];
    }
    buf[48] = 0;
    for (int i = 0; buf[i]; i++)
        if (buf[i] == '\t' || buf[i] == '\n') buf[i] = ' ';
}

unsigned int get_cpu_features(void) {
    unsigned int a, b, c, d;
    if (!cpuid_supported()) return 0;
    cpuid_string(1, &a, &b, &c, &d);
    return d;
}
