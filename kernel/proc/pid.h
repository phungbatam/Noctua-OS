#ifndef TVN_PID_H
#define TVN_PID_H

#include <stdint.h>

#define PID_MAX 32768
#define PID_MIN 1

void pid_init(void);
int  pid_alloc(void);
void pid_free(int pid);
int  pid_is_used(int pid);

#endif
