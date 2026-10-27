#include "kernel/syscall.h"
#include "kernel/idt.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "lib/printf.h"
#include "proc/scheduler.h"
#include "proc/process.h"
#include <stdint.h>

extern void isr128(void);

void syscall_init(void) {
    idt_set_user_gate(0x80, isr128);
    kprintf("[syscall] int 0x80 gate registered (DPL=3)\n");
}

static void sys_write(int fd, const char *buf, uint64_t len) {
    for (uint64_t i = 0; i < len; i++) {
        if (fd == 1 || fd == 2)
            vga_putchar(buf[i]);   /* stdout/stderr → 화면 */
        serial_putchar(buf[i]);    /* 항상 시리얼에도 출력 */
    }
}

static void sys_exit(int code) {
    kprintf("[syscall] pid %d: exit(%d)\n", current_process()->pid, code);
    current_process()->state = PROC_DEAD;
    schedule();
    /* PROC_DEAD이므로 schedule()은 돌아오지 않음 */
    __asm__ volatile("cli; hlt");
}

void syscall_handler(registers_t *r) {
    switch (r->rax) {
    case SYS_WRITE: sys_write((int)r->rdi, (const char *)r->rsi, r->rdx); break;
    case SYS_EXIT:  sys_exit((int)r->rdi);                                 break;
    default:
        kprintf("[syscall] unknown: rax=%lu\n", r->rax);
        break;
    }
}
