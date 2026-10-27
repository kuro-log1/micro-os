#pragma once
#include <stdint.h>
#include "kernel/file.h"

#define FD_MAX 16

typedef enum { PROC_READY, PROC_RUNNING, PROC_DEAD } proc_state_t;

typedef struct process {
    uint64_t       rsp;      /* 저장된 커널 스택 포인터 (swtch용) */
    uint64_t       user_rip; /* 유저 진입점 (유저 프로세스만) */
    uint64_t       user_rsp; /* 유저 스택 상단 (유저 프로세스만) */
    uint64_t      *pml4;
    void          *stack;
    proc_state_t   state;
    int            pid;
    struct process *next;
    void         (*entry)(void);
    file_t        *fd_table[FD_MAX];
} process_t;

process_t *process_create(void (*entry)(void));
process_t *process_create_idle(void);
process_t *process_create_user(uint64_t user_rip, uint64_t user_rsp);
