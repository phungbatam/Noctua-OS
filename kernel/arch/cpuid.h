#ifndef CPUID_H
#define CPUID_H

int cpuid_supported(void);
void cpuid_string(int code, unsigned int *a, unsigned int *b, unsigned int *c, unsigned int *d);
void get_cpu_vendor(char *buf);
void get_cpu_brand(char *buf);
unsigned int get_cpu_features(void);

#endif
