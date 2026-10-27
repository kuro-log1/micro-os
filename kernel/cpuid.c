#include "kernel/cpuid.h"
#include "lib/printf.h"
#include <stdint.h>

void cpuid_log(void) {
    uint32_t eax, ebx, ecx, edx;

    /* Leaf 0: vendor string (EBX, EDX, ECX 순서) */
    __asm__ volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));

    char vendor[13];
    *(uint32_t *)(vendor + 0) = ebx;
    *(uint32_t *)(vendor + 4) = edx;
    *(uint32_t *)(vendor + 8) = ecx;
    vendor[12] = '\0';

    /* Leaf 0x80000000: brand string 지원 여부 확인 */
    __asm__ volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));

    if (eax >= 0x80000004U) {
        /* Leaf 0x80000002-4: 48바이트 brand string */
        char brand[49];
        uint32_t *p = (uint32_t *)brand;
        for (int i = 0; i < 3; i++) {
            __asm__ volatile("cpuid"
                : "=a"(p[0]), "=b"(p[1]), "=c"(p[2]), "=d"(p[3])
                : "a"(0x80000002U + (uint32_t)i));
            p += 4;
        }
        brand[48] = '\0';

        /* 앞 공백 제거 */
        char *b = brand;
        while (*b == ' ') b++;

        klog("[cpu] %s  %s\n", vendor, b);
    } else {
        klog("[cpu] %s\n", vendor);
    }
}
