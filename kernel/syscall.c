#include "kernel/syscall.h"
#include "kernel/idt.h"
#include "lib/printf.h"
#include <stdint.h>

extern void isr128(void);

void syscall_init(void) {
    idt_set_user_gate(0x80, isr128);
    printf("[syscall] int 0x80 gate registered (DPL=3)\n");
}

void syscall_handler(registers_t *r) {
    printf("[syscall] rax=%lu (unimplemented)\n", r->rax);
}
