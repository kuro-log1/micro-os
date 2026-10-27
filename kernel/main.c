#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/idt.h"
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

    /* [실습] 주석을 풀면 #DE(0번) 예외 → KERNEL PANIC 화면 */
    /* volatile int zero = 0; printf("%d\n", 1 / zero); */

    for (;;) __asm__ volatile("hlt");
}
