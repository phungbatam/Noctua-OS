#ifndef TVN_SHM_H
#define TVN_SHM_H

#include <stdint.h>

void shm_init(void);
int shm_create(int key, uint32_t size);
void *shm_attach(int id);
int shm_detach(void *addr);
int shm_delete(int id);

#endif
