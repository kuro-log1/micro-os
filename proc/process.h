#pragma once
#include <stdint.h>
#include "kernel/file.h"
#include "kernel/idt.h"

#define FD_MAX     16
#define STACK_SIZE 8192

typedef enum { PROC_READY, PROC_RUNNING, PROC_DEAD } proc_state_t;

typedef struct process {
    uint64_t       rsp;        /* 저장된 커널 스택 포인터 (swtch용) */
    uint64_t       user_rip;   /* 유저 진입점 (유저 프로세스만) */
    uint64_t       user_rsp;   /* 유저 스택 상단 (유저 프로세스만) */
    uint64_t      *pml4;
    void          *stack;
    proc_state_t   state;
    int            pid;
    struct process *next;
    void         (*entry)(void);
    file_t        *fd_table[FD_MAX];
    registers_t   *saved_regs; /* fork: 자식이 복귀할 레지스터 프레임 */
    int            exit_code;  /* sys_wait: exit() 시 종료 코드 */
} process_t;

process_t  *process_create(void (*entry)(void));
process_t  *process_create_idle(void);
process_t  *process_create_user(uint64_t user_rip, uint64_t user_rsp);
process_t  *process_fork(registers_t *r);
