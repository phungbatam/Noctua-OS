#include "signal.h"
#include "screen.h"
#include "string.h"

static const char *sig_names[] = {
    "", "SIGKILL", "SIGINT", "SIGSEGV", "SIGPIPE", "SIGTERM"
};

void signal_send(task_t *t, int sig) {
    if (!t || sig <= 0 || sig >= NSIG) return;

    /* SIGKILL cannot be blocked or ignored */
    if (sig == SIGKILL) {
        t->state = TASK_ZOMBIE;
        t->exit_code = 128 + SIGKILL;
        return;
    }

    /* Check if blocked */
    if (t->sig_blocked & (1 << sig)) return;

    t->sig_pending |= (1 << sig);
}

void signal_check(void) {
    task_t *curr = task_current();
    if (!curr || !curr->sig_pending) return;

    /* Duyệt qua các signal đang pending */
    for (int sig = 1; sig < NSIG; sig++) {
        if (!(curr->sig_pending & (1 << sig))) continue;
        curr->sig_pending &= ~(1 << sig);

        void (*handler)(int) = curr->sig_handlers[sig];

        if (handler == SIG_IGN) {
            /* Ignore signal */
            continue;
        }

        if (handler == SIG_DFL) {
            /* Default action */
            switch (sig) {
            case SIGKILL:
            case SIGSEGV:
            case SIGTERM:
                curr->state = TASK_ZOMBIE;
                curr->exit_code = 128 + sig;
                return;
            case SIGINT:
                /* Default: terminate */
                curr->state = TASK_ZOMBIE;
                curr->exit_code = 128 + SIGINT;
                return;
            case SIGPIPE:
                /* Default: ignore */
                break;
            }
            continue;
        }

        /* Call user handler */
        handler(sig);
    }
}

void signal_register(int sig, void (*handler)(int)) {
    task_t *curr = task_current();
    if (!curr || sig <= 0 || sig >= NSIG) return;
    curr->sig_handlers[sig] = handler;
}
