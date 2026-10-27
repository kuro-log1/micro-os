#pragma once

void printf(const char *fmt, ...);   /* VGA + 시리얼 */
void klog(const char *fmt, ...);     /* VGA + 시리얼 + 타임스탬프 (부팅 로그) */
void kprintf(const char *fmt, ...);  /* 시리얼 전용 + 타임스탬프 (커널 디버그) */
