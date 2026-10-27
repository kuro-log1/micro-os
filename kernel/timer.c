#include "kernel/timer.h"
#include "kernel/idt.h"
#include "kernel/io.h"
#include "proc/scheduler.h"
#include <stdint.h>

static volatile uint64_t ticks = 0;

/* TSC 보정값 */
static uint64_t tsc_boot   = 0;  /* tick 0 시점의 추정 TSC 값 */
static uint64_t tsc_per_us = 0;  /* TSC 클럭 / 마이크로초 (0 = 미보정) */

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* sti 이후 호출: PIT 1틱(10ms)을 기준으로 TSC 주파수 측정 */
void tsc_calibrate(void) {
    /* 현재 tick 경계에 맞춤 */
    uint64_t t0 = ticks;
    while (ticks == t0) ;
    uint64_t r0 = rdtsc();
    uint64_t t1 = ticks;
    while (ticks == t1) ;
    uint64_t r1 = rdtsc();

    uint64_t tsc_per_tick = r1 - r0;   /* 10ms(100Hz) 동안의 TSC 틱 수 */
    tsc_per_us = tsc_per_tick / 10000; /* TSC ticks per μs */

    /* tick 0 시점의 TSC 역산: r1은 (t1+1)번째 tick에서 읽혔음 */
    tsc_boot = r1 - (uint64_t)(t1 + 1) * tsc_per_tick;
}

uint64_t tsc_elapsed_us(void) {
    if (tsc_per_us == 0)
        return ticks * 10000;  /* fallback: 1tick=10ms=10000μs */
    return (rdtsc() - tsc_boot) / tsc_per_us;
}

static void timer_handler(registers_t *r) {
    (void)r;
    ticks++;
    /* 10틱(100ms)마다 컨텍스트 스위치 */
    if (ticks % 10 == 0)
        schedule();
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
