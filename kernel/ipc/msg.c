#include "msg.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../proc/task.h"

#define MAX_MSG_QUEUES 16
#define MAX_MSG_SIZE 4096
#define MSG_QUEUE_SIZE 64

typedef struct {
    int id;
    int key;
    int refcount;
    msg_t messages[MSG_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} msg_queue_t;

static msg_queue_t msg_queues[MAX_MSG_QUEUES];
static int msg_queue_count = 0;
static int next_msg_id = 1;

void msg_init(void) {
    memset(msg_queues, 0, sizeof(msg_queues));
    msg_queue_count = 0;
    next_msg_id = 1;
}

static msg_queue_t *msg_find_by_id(int id) {
    for (int i = 0; i < msg_queue_count; i++) {
        if (msg_queues[i].id == id) {
            return &msg_queues[i];
        }
    }
    return NULL;
}

static msg_queue_t *msg_find_by_key(int key) {
    for (int i = 0; i < msg_queue_count; i++) {
        if (msg_queues[i].key == key && msg_queues[i].key != 0) {
            return &msg_queues[i];
        }
    }
    return NULL;
}

int msg_get(int key, int flags) {
    (void)flags;
    
    /* Check if queue with this key already exists */
    if (key != 0) {
        msg_queue_t *existing = msg_find_by_key(key);
        if (existing) {
            existing->refcount++;
            return existing->id;
        }
    }
    
    if (msg_queue_count >= MAX_MSG_QUEUES) {
        return -1;
    }
    
    /* Create new message queue */
    msg_queue_t *queue = &msg_queues[msg_queue_count++];
    queue->id = next_msg_id++;
    queue->key = key;
    queue->refcount = 1;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    memset(queue->messages, 0, sizeof(queue->messages));
    
    return queue->id;
}

int msg_send(int msqid, const void *msgp, uint32_t msgsz, int msgflg) {
    (void)msgflg;
    
    msg_queue_t *queue = msg_find_by_id(msqid);
    if (!queue) {
        return -1;
    }
    
    if (queue->count >= MSG_QUEUE_SIZE) {
        return -1; /* Queue full */
    }
    
    if (msgsz > MAX_MSG_SIZE) {
        return -1; /* Message too large */
    }
    
    /* Copy message to queue */
    msg_t *msg = &queue->messages[queue->tail];
    msg->size = msgsz;
    memcpy(msg->data, msgp, msgsz);
    
    queue->tail = (queue->tail + 1) % MSG_QUEUE_SIZE;
    queue->count++;
    
    return 0;
}

int msg_recv(int msqid, void *msgp, uint32_t msgsz, int msgflg) {
    (void)msgflg;
    
    msg_queue_t *queue = msg_find_by_id(msqid);
    if (!queue) {
        return -1;
    }
    
    if (queue->count == 0) {
        return -1; /* Queue empty */
    }
    
    /* Get message from queue */
    msg_t *msg = &queue->messages[queue->head];
    
    if (msgsz < msg->size) {
        return -1; /* Buffer too small */
    }
    
    memcpy(msgp, msg->data, msg->size);
    
    queue->head = (queue->head + 1) % MSG_QUEUE_SIZE;
    queue->count--;
    
    return msg->size;
}

int msg_ctl(int msqid, int cmd, void *buf) {
    (void)buf;
    
    msg_queue_t *queue = msg_find_by_id(msqid);
    if (!queue) {
        return -1;
    }
    
    switch (cmd) {
        case 1: /* IPC_RMID - remove queue */
            queue->refcount--;
            if (queue->refcount <= 0) {
                /* Remove from array */
                for (int i = 0; i < msg_queue_count; i++) {
                    if (&msg_queues[i] == queue) {
                        for (int j = i; j < msg_queue_count - 1; j++) {
                            msg_queues[j] = msg_queues[j + 1];
                        }
                        msg_queue_count--;
                        break;
                    }
                }
            }
            return 0;
        default:
            return -1;
    }
}
