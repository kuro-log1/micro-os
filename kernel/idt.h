#pragma once
#include <stdint.h>

/* 예외 발생 시 스택에 저장되는 레지스터 프레임
 * isr_common이 push한 순서와 1:1 매핑 */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    /* CPU가 자동으로 push하는 부분 */
    uint64_t rip, cs, rflags;
} __attribute__((packed)) registers_t;

void idt_init(void);
void idt_set_user_gate(int n, void (*fn)(void));

typedef void (*irq_handler_t)(registers_t *);
void irq_init(void);
void irq_install_handler(int irq, irq_handler_t fn);
