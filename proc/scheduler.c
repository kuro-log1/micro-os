#include "proc/scheduler.h"
#include <stddef.h>

static process_t *current = NULL;

/* proc/swtch.s에 정의 */
extern void swtch(uint64_t *old_rsp_out, uint64_t new_rsp);

void scheduler_init(process_t *idle) {
    current    = idle;
    idle->next = idle;  /* 자기 자신을 가리키는 원형 큐 */
}

void scheduler_add(process_t *p) {
    /* current 바로 다음에 삽입 */
    p->next       = current->next;
    current->next = p;
}

process_t *current_process(void) {
    return current;
}

void schedule(void) {
    process_t *prev = current;
    process_t *next = current->next;

    /* PROC_DEAD 프로세스는 건너뜀 */
    while (next != prev && next->state == PROC_DEAD)
        next = next->next;

    if (prev == next) return;

    if (prev->state != PROC_DEAD)
        prev->state = PROC_READY;
    current     = next;
    next->state = PROC_RUNNING;

    swtch(&prev->rsp, next->rsp);
}
