#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "kernel/idt.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "lib/printf.h"

/* [실습] 두 커널 스레드가 타이머 선점으로 번갈아 실행된다 */
static void busy_wait(void) {
    for (volatile int i = 0; i < 20000000; i++)
        ;
}

static void thread_a(void) {
    for (;;) { printf("A"); busy_wait(); }
}

static void thread_b(void) {
    for (;;) { printf("B"); busy_wait(); }
}

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
    timer_init(100);
    keyboard_init();
    mouse_init();
    __asm__ volatile("sti");

    pmm_init(info);
    vmm_init();
    heap_init();

    process_t *idle = process_create_idle();
    scheduler_init(idle);
    scheduler_add(process_create(thread_a));
    scheduler_add(process_create(thread_b));

    schedule();

    for (;;) __asm__ volatile("sti; hlt");
}
