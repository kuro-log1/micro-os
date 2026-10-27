#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "kernel/idt.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "kernel/tss.h"
#include "kernel/syscall.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "shell/shell.h"
#include "fs/fs.h"
#include "lib/printf.h"

void kernel_main(uint64_t magic, uint64_t info) {
    (void)magic;

    vga_init();
    serial_init();

    printf("============================\n");
    printf("  micro-os booted!\n");
    printf("============================\n");

    idt_init();
    pic_init();
    irq_init();
    tss_init();
    syscall_init();
    timer_init(100);
    keyboard_init();
    mouse_init();
    __asm__ volatile("sti");

    /* Step 2 테스트: 커널(ring 0)에서 int 0x80 호출 */
    __asm__ volatile("mov $0x42, %%rax\n\tint $0x80" ::: "rax");

    pmm_init(info);
    vmm_init();
    heap_init();
    fs_init();

    process_t *idle = process_create_idle();
    scheduler_init(idle);
    scheduler_add(process_create(shell_run));

    schedule();

    for (;;) __asm__ volatile("sti; hlt");
}
