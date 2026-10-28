#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"

/* printf가 생기기 전까지 쓰는 임시 출력: 화면과 시리얼에 같은 글자를 보낸다 */
static void puts_both(const char *s) {
    for (; *s; s++) {
        vga_putchar(*s);
        serial_putchar(*s);
    }
}

void kernel_main(uint64_t magic, uint64_t info) {
    (void)magic;
    (void)info;

    vga_init();
    serial_init();

    puts_both("vga + serial ready\n");
    puts_both("line 2: newline works\n");

    for (;;) __asm__ volatile("hlt");
}
