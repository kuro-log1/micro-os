#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/idt.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "lib/printf.h"

void kernel_main(uint64_t magic, uint64_t info) {
    (void)magic;
    (void)info;

    vga_init();
    serial_init();

    printf("============================\n");
    printf("  micro-os booted!\n");
    printf("============================\n");

    idt_init();
    pic_init();
    irq_init();
    timer_init(100);
    __asm__ volatile("sti");

    for (;;) __asm__ volatile("sti; hlt");
}
