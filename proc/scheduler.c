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

    if (prev == next) return;  /* 프로세스가 하나뿐 */

    prev->state = PROC_READY;
    current     = next;
    next->state = PROC_RUNNING;

    swtch(&prev->rsp, next->rsp);
    /* 이 줄은 나중에 다시 이 프로세스로 스위치백 됐을 때 실행됨 */
}
