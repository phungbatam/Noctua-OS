#ifndef TVN_SIGNAL_H
#define TVN_SIGNAL_H

#include <stdint.h>
#include "task.h"

/* TVN_OS Signal handling
 * Đơn giản hơn Linux: không có sigframe, sigreturn,
 * chỉ hỗ trợ SIG_DFL, SIG_IGN, SIGKILL cơ bản */

/* Gửi signal tới task */
void signal_send(task_t *t, int sig);

/* Kiểm tra và xử lý signal cho task hiện tại */
void signal_check(void);

/* Đăng ký handler cho signal */
void signal_register(int sig, void (*handler)(int));

#endif
