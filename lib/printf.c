#include "lib/printf.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/timer.h"
#include <stdarg.h>
#include <stdint.h>

static void emit(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

static void semit(char c) {  /* serial only */
    serial_putchar(c);
}

typedef void (*emit_fn)(char);

static void do_str(emit_fn out, const char *s) {
    while (*s) out(*s++);
}

static void do_uint(emit_fn out, uint64_t n, int base, int width) {
    char buf[64];
    int  i = 0;
    const char digits[] = "0123456789abcdef";

    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) { buf[i++] = digits[n % base]; n /= base; }
    }
    while (i < width) buf[i++] = '0';
    while (i-- > 0) out(buf[i]);
}

static void do_int(emit_fn out, int64_t n) {
    if (n < 0) { out('-'); do_uint(out, (uint64_t)(-n), 10, 0); }
    else        { do_uint(out, (uint64_t)n, 10, 0); }
}

static void do_printf(emit_fn out, const char *fmt, va_list ap) {
    while (*fmt) {
        if (*fmt != '%') { out(*fmt++); continue; }
        fmt++;

        /* %08x, %02x 같은 제로 패딩 폭 파싱 */
        int pad_zero = 0, width = 0;
        if (*fmt == '0') { pad_zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); }

        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; }

        switch (*fmt) {
        case 'c': out((char)va_arg(ap, int)); break;
        case 's': do_str(out, va_arg(ap, const char *)); break;
        case 'd': do_int(out, is_long ? va_arg(ap, long) : va_arg(ap, int)); break;
        case 'u': do_uint(out, is_long ? va_arg(ap, unsigned long)
                                       : va_arg(ap, unsigned int), 10, 0); break;
        case 'x': do_uint(out, is_long ? va_arg(ap, unsigned long)
                                       : va_arg(ap, unsigned int), 16,
                          pad_zero ? width : 0); break;
        case 'p': do_str(out, "0x");
                  do_uint(out, (uint64_t)(uintptr_t)va_arg(ap, void *), 16, 16); break;
        case '%': out('%'); break;
        default:  out('%'); out(*fmt); break;
        }
        fmt++;
    }
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    do_printf(emit, fmt, ap);
    va_end(ap);
}

/* [    0.000000] 타임스탬프를 시리얼에 출력 (TSC 마이크로초 단위) */
static void emit_kts(void) {
    uint64_t us   = tsc_elapsed_us();
    uint64_t sec  = us / 1000000;
    uint64_t usec = us % 1000000;

    char sbuf[10];
    int  si = 0;
    uint64_t tmp = sec;
    if (tmp == 0) { sbuf[si++] = '0'; }
    else { while (tmp > 0) { sbuf[si++] = (char)('0' + tmp % 10); tmp /= 10; } }

    semit('[');
    for (int p = si; p < 4; p++) semit(' ');
    for (int p = si - 1; p >= 0; p--) semit(sbuf[p]);
    semit('.');
    do_uint(semit, usec, 10, 6);
    semit(']');
    semit(' ');
}

/* 부팅 로그 — VGA + 시리얼, 타임스탬프 포함 */
void klog(const char *fmt, ...) {
    emit_kts();
    va_list ap;
    va_start(ap, fmt);
    do_printf(emit, fmt, ap);
    va_end(ap);
}

/* 커널 내부 디버그 로그 — 시리얼 전용, 타임스탬프 포함 */
void kprintf(const char *fmt, ...) {
    emit_kts();
    va_list ap;
    va_start(ap, fmt);
    do_printf(semit, fmt, ap);
    va_end(ap);
}
