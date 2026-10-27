#pragma once
#include <stdint.h>

typedef enum { PROC_READY, PROC_RUNNING } proc_state_t;

typedef struct process {
    uint64_t       rsp;    /* 저장된 스택 포인터 */
    uint64_t      *pml4;   /* 페이지 테이블 */
    void          *stack;  /* 스택 베이스 (해제용) */
    proc_state_t   state;
    int            pid;
    struct process *next;  /* 스케줄러 원형 큐 */
    void         (*entry)(void); /* 트램폴린에서 호출할 진입점 */
} process_t;

process_t *process_create(void (*entry)(void));
process_t *process_create_idle(void);  /* 현재 커널 컨텍스트를 프로세스 0으로 등록 */
