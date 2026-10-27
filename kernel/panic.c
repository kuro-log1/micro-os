#include "kernel/panic.h"
#include "lib/printf.h"

__attribute__((noreturn))
void panic(registers_t *r, const char *msg) {
    printf("\n*** KERNEL PANIC: %s ***\n", msg);
    printf("  int=%d  err=0x%lx\n", (int)r->int_no, r->err_code);
    printf("  rip=%p  rflags=0x%lx\n", (void *)r->rip, r->rflags);
    printf("  rax=%p  rbx=%p\n", (void *)r->rax, (void *)r->rbx);
    printf("  rcx=%p  rdx=%p\n", (void *)r->rcx, (void *)r->rdx);
    printf("  rsi=%p  rdi=%p\n", (void *)r->rsi, (void *)r->rdi);
    printf("  r8 =%p  r9 =%p\n", (void *)r->r8,  (void *)r->r9);

    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}
