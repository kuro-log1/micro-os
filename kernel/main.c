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
#include "kernel/usermode.h"
#include "kernel/cpuid.h"
#include "kernel/mmap.h"
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

    klog("[boot] micro-os starting...\n");
    cpuid_log();

    idt_init();
    pic_init();
    irq_init();
    tss_init();
    syscall_init();
    timer_init(100);
    keyboard_init();
    mouse_init();
    __asm__ volatile("sti");
    tsc_calibrate();  /* PIT 1틱(10ms)으로 TSC 주파수 보정 */

    mmap_log(info);
    pmm_init(info);
    vmm_init();
    heap_init();
    usermode_setup();
    fs_init();

    process_t *idle = process_create_idle();
    scheduler_init(idle);
    scheduler_add(process_create(shell_run));

    schedule();

    for (;;) __asm__ volatile("sti; hlt");
}
