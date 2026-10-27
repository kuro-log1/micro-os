#include "proc/process.h"
#include "proc/scheduler.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "kernel/usermode.h"
#include "kernel/file.h"
#include "lib/string.h"
#include <stdint.h>
#include <stddef.h>

#define STACK_SIZE 8192      /* 프로세스당 8KB 커널 스택 */
/* ring3 syscall 프레임: registers_t(20 words) + user_rsp + user_ss = 22 uint64_t */
#define RING3_FRAME_WORDS 22

static int next_pid = 0;

extern void fork_return(void);

static void proc_init_fds(process_t *p) {
    memset(p->fd_table, 0, sizeof(p->fd_table));

    file_t *stdin_f  = file_alloc();
    stdin_f->type    = FILE_TYPE_STDIN;
    p->fd_table[0]   = stdin_f;

    file_t *stdout_f = file_alloc();
    stdout_f->type   = FILE_TYPE_STDOUT;
    p->fd_table[1]   = stdout_f;

    file_t *stderr_f = file_alloc();
    stderr_f->type   = FILE_TYPE_STDOUT;
    p->fd_table[2]   = stderr_f;
}

/*
 * 인터럽트 컨텍스트에서 새 프로세스로 처음 스위칭될 때
 * IRET 없이 ret으로 진입하므로 IF가 0인 상태다.
 * sti로 인터럽트를 다시 활성화한 뒤 실제 entry를 호출한다.
 */
static void proc_trampoline(void) {
    __asm__ volatile("sti");
    current_process()->entry();
}

process_t *process_create(void (*entry)(void)) {
    process_t *p   = kmalloc(sizeof(process_t));
    uint8_t   *stk = kmalloc(STACK_SIZE);

    /*
     * 초기 스택 프레임 설정:
     * swtch가 r15~rbp를 팝한 뒤 ret하면 proc_trampoline으로 점프.
     * push 순서는 swtch의 pop 순서와 반대여야 한다.
     */
    uint64_t *sp = (uint64_t *)(stk + STACK_SIZE);
    *--sp = (uint64_t)proc_trampoline;
    *--sp = 0;  /* rbp */
    *--sp = 0;  /* rbx */
    *--sp = 0;  /* r12 */
    *--sp = 0;  /* r13 */
    *--sp = 0;  /* r14 */
    *--sp = 0;  /* r15 */

    p->rsp        = (uint64_t)sp;
    p->user_rip   = 0;
    p->user_rsp   = 0;
    p->pml4       = vmm_get_pml4();
    p->stack      = stk;
    p->state      = PROC_READY;
    p->pid        = next_pid++;
    p->entry      = entry;
    p->next       = NULL;
    p->saved_regs = NULL;
    p->exit_code  = 0;
    proc_init_fds(p);
    return p;
}

process_t *process_create_idle(void) {
    process_t *p = kmalloc(sizeof(process_t));
    p->rsp        = 0;
    p->user_rip   = 0;
    p->user_rsp   = 0;
    p->pml4       = vmm_get_pml4();
    p->stack      = NULL;
    p->state      = PROC_RUNNING;
    p->pid        = next_pid++;
    p->entry      = NULL;
    p->next       = NULL;
    p->saved_regs = NULL;
    p->exit_code  = 0;
    proc_init_fds(p);
    return p;
}

/* 유저 프로세스 트램폴린: ring 0 → ring 3 전환 */
static void user_proc_trampoline(void) {
    __asm__ volatile("sti");
    process_t *p = current_process();
    jump_to_usermode(p->user_rip, p->user_rsp);
    /* 돌아오지 않음: exit 시스템콜이 schedule()을 호출 */
}

process_t *process_create_user(uint64_t user_rip, uint64_t user_rsp) {
    process_t *p   = kmalloc(sizeof(process_t));
    uint8_t   *stk = kmalloc(STACK_SIZE);

    uint64_t *sp = (uint64_t *)(stk + STACK_SIZE);
    *--sp = (uint64_t)user_proc_trampoline;
    *--sp = 0;  /* rbp */
    *--sp = 0;  /* rbx */
    *--sp = 0;  /* r12 */
    *--sp = 0;  /* r13 */
    *--sp = 0;  /* r14 */
    *--sp = 0;  /* r15 */

    p->rsp        = (uint64_t)sp;
    p->user_rip   = user_rip;
    p->user_rsp   = user_rsp;
    p->pml4       = vmm_get_pml4();
    p->stack      = stk;
    p->state      = PROC_READY;
    p->pid        = next_pid++;
    p->entry      = NULL;
    p->next       = NULL;
    p->saved_regs = NULL;
    p->exit_code  = 0;
    proc_init_fds(p);
    return p;
}

process_t *process_fork(registers_t *r) {
    process_t *cur   = current_process();
    process_t *child = kmalloc(sizeof(process_t));
    uint8_t   *stk   = kmalloc(STACK_SIZE);

    /* PCB 전체 복사 후 자식 전용 필드 덮어쓰기 */
    *child           = *cur;
    child->stack     = stk;
    child->pid       = next_pid++;
    child->state     = PROC_READY;
    child->next      = NULL;
    child->exit_code = 0;

    /* fd_table: 포인터 공유 + refcount 증가 */
    for (int i = 0; i < FD_MAX; i++) {
        if (child->fd_table[i])
            child->fd_table[i]->refcount++;
    }

    /* 자식 커널 스택 최상단에 ring3 프레임 복사
     * registers_t(20 words) + user_rsp + user_ss = RING3_FRAME_WORDS uint64_t */
    uint64_t *sp  = (uint64_t *)(stk + STACK_SIZE);
    uint64_t *src = (uint64_t *)r;
    for (int i = RING3_FRAME_WORDS - 1; i >= 0; i--)
        *--sp = src[i];

    child->saved_regs      = (registers_t *)sp;
    child->saved_regs->rax = 0;  /* 자식은 fork()에서 0 반환 */

    /* swtch 프레임: fork_return으로 복귀하도록 설정 */
    *--sp = (uint64_t)fork_return;
    *--sp = 0;  /* rbp */
    *--sp = 0;  /* rbx */
    *--sp = 0;  /* r12 */
    *--sp = 0;  /* r13 */
    *--sp = 0;  /* r14 */
    *--sp = 0;  /* r15 */
    child->rsp = (uint64_t)sp;

    /* 유저 주소 공간 복제 */
    child->pml4 = vmm_fork(cur->pml4);

    scheduler_add(child);
    return child;
}
