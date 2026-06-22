#ifndef TVN_MSG_H
#define TVN_MSG_H

#include <stdint.h>

typedef struct {
    uint32_t size;
    uint8_t data[4096];
} msg_t;

void msg_init(void);
int msg_get(int key, int flags);
int msg_send(int msqid, const void *msgp, uint32_t msgsz, int msgflg);
int msg_recv(int msqid, void *msgp, uint32_t msgsz, int msgflg);
int msg_ctl(int msqid, int cmd, void *buf);

#endif
