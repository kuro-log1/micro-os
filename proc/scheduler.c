#include "proc/scheduler.h"
#include "kernel/tss.h"
#include <stddef.h>

static process_t *current = NULL;

/* proc/swtch.s에 정의 */
extern void swtch(uint64_t *old_rsp_out, uint64_t new_rsp);

void scheduler_init(process_t *idle) {
    current    = idle;
    idle->next = idle;  /* 자기 자신을 가리키는 원형 큐 */
}

void scheduler_add(process_t *p) {
    p->next       = current->next;
    current->next = p;
}

/* 원형 리스트에서 p 제거 */
void scheduler_remove(process_t *p) {
    process_t *prev = current;
    while (prev->next != p) {
        prev = prev->next;
        if (prev == current) return;  /* not found */
    }
    prev->next = p->next;
}

/* pid로 프로세스 찾기 (없으면 NULL) */
process_t *scheduler_find(int pid) {
    process_t *p = current;
    do {
        if (p->pid == pid) return p;
        p = p->next;
    } while (p != current);
    return NULL;
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

    /* 유저 프로세스로 전환 시 TSS.rsp0를 해당 프로세스의 커널 스택 top으로 갱신.
     * 그렇지 않으면 int 0x80 진입 시 잘못된 스택을 사용해 커널 스택이 깨진다. */
    if (next->stack)
        tss_set_rsp0((uint64_t)next->stack + STACK_SIZE);

    /* 주소 공간이 다른 프로세스로 전환할 때 CR3를 교체한다 */
    if (next->pml4 != prev->pml4)
        __asm__ volatile("mov %0, %%cr3" : : "r"((uint64_t)next->pml4) : "memory");

    swtch(&prev->rsp, next->rsp);
}
