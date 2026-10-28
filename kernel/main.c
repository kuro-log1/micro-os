#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "lib/printf.h"

void kernel_main(uint64_t magic, uint64_t info) {
    (void)magic;
    (void)info;

    vga_init();
    serial_init();

    printf("============================\n");
    printf("  micro-os booted!\n");
    printf("============================\n");

    for (;;) __asm__ volatile("hlt");
}
