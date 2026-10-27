#include "kernel/timer.h"
#include "kernel/idt.h"
#include "kernel/io.h"
#include "lib/printf.h"
#include <stdint.h>

static volatile uint64_t ticks = 0;

static void timer_handler(registers_t *r) {
    (void)r;
    ticks++;
    /* 100Hz 기준 100틱 = 1초 (Step 08에서 스케줄러 호출로 바뀜) */
    if (ticks % 100 == 0)
        printf("[timer] %lu s\n", ticks / 100);
}

void timer_init(uint32_t hz) {
    irq_install_handler(0, timer_handler);

    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);                       /* 채널0, lobyte/hibyte, mode3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)(divisor >> 8));
}

uint64_t timer_ticks(void) {
    return ticks;
}
