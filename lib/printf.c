#include "lib/printf.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include <stdarg.h>
#include <stdint.h>

static void emit(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

static void emit_str(const char *s) {
    while (*s) emit(*s++);
}

/* n을 base 진수로 출력. width만큼 '0'으로 왼쪽 채움 */
static void emit_uint(uint64_t n, int base, int width) {
    char buf[64];
    int  i = 0;
    const char digits[] = "0123456789abcdef";

    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            buf[i++] = digits[n % base];
            n /= base;
        }
    }

    /* 왼쪽 zero-padding */
    while (i < width) buf[i++] = '0';

    /* 역순 출력 */
    while (i-- > 0) emit(buf[i]);
}

static void emit_int(int64_t n) {
    if (n < 0) {
        emit('-');
        emit_uint((uint64_t)(-n), 10, 0);
    } else {
        emit_uint((uint64_t)n, 10, 0);
    }
}

void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            emit(*fmt++);
            continue;
        }
        fmt++;  /* '%' 다음 문자 */

        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            emit((char)va_arg(ap, int));
            break;
        case 's':
            emit_str(va_arg(ap, const char *));
            break;
        case 'd':
            emit_int(is_long ? va_arg(ap, long) : va_arg(ap, int));
            break;
        case 'u':
            emit_uint(is_long ? va_arg(ap, unsigned long)
                               : va_arg(ap, unsigned int), 10, 0);
            break;
        case 'x':
            emit_uint(is_long ? va_arg(ap, unsigned long)
                               : va_arg(ap, unsigned int), 16, 0);
            break;
        case 'p':
            emit_str("0x");
            emit_uint((uint64_t)(uintptr_t)va_arg(ap, void *), 16, 16);
            break;
        case '%':
            emit('%');
            break;
        default:
            emit('%');
            emit(*fmt);
            break;
        }
        fmt++;
    }

    va_end(ap);
}
