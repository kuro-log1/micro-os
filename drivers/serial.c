#include "drivers/serial.h"
#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);  /* 인터럽트 비활성화 */
    outb(COM1 + 3, 0x80);  /* DLAB 활성화 */
    outb(COM1 + 0, 0x01);  /* 보레이트 divisor 하위: 115200bps */
    outb(COM1 + 1, 0x00);  /* 보레이트 divisor 상위 */
    outb(COM1 + 3, 0x03);  /* 8비트, 패리티 없음, 1 스톱 비트 */
    outb(COM1 + 2, 0xC7);  /* FIFO 활성화 */
    outb(COM1 + 4, 0x0B);  /* RTS/DSR 설정 */
}

static int serial_ready(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_ready());
    outb(COM1, (uint8_t)c);
}
