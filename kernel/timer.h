#pragma once
#include <stdint.h>

void     timer_init(uint32_t hz);
uint64_t timer_ticks(void);

/* TSC 기반 고해상도 타임스탬프 */
void     tsc_calibrate(void);   /* sti 이후 한 번 호출 — PIT로 TSC 주파수 측정 */
uint64_t tsc_elapsed_us(void);  /* 부팅 기준 경과 시간 (마이크로초) */
