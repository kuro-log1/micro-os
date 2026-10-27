#include "kernel/tss.h"
#include "lib/printf.h"
#include <stdint.h>
#include <stddef.h>

/* Intel 64-bit TSS 레이아웃 (104바이트) */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss64_t;

static tss64_t tss __attribute__((aligned(16)));

/* ring 3 → ring 0 인터럽트(int 0x80) 발생 시 CPU가 이 스택으로 전환 */
static uint8_t syscall_stack[4096] __attribute__((aligned(16)));

/* boot.s .data 섹션의 GDT 배열 */
extern uint64_t gdt64[];

void tss_init(void) {
    /* TSS 0으로 초기화 */
    for (size_t i = 0; i < sizeof(tss); i++)
        ((uint8_t *)&tss)[i] = 0;

    tss.rsp0       = (uint64_t)(syscall_stack + sizeof(syscall_stack));
    tss.iomap_base = (uint16_t)sizeof(tss64_t); /* I/O 퍼미션 비트맵 없음 */

    /*
     * GDT[5/6]에 16바이트 TSS 시스템 디스크립터 설치
     *
     * Low 8바이트:
     *   [15:0]  limit[15:0]
     *   [39:16] base[23:0]
     *   [47:40] 0x89 = P=1, DPL=0, S=0, Type=9(64-bit TSS Available)
     *   [51:48] limit[19:16]
     *   [63:56] base[31:24]
     * High 8바이트:
     *   [31:0]  base[63:32]
     */
    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = (uint32_t)(sizeof(tss64_t) - 1);

    gdt64[5] = (uint64_t)(limit & 0xFFFF)
             | ((base & 0xFFFFFFUL) << 16)
             | ((uint64_t)0x89 << 40)
             | (((uint64_t)(limit >> 16) & 0xF) << 48)
             | (((base >> 24) & 0xFFUL) << 56);
    gdt64[6] = (base >> 32) & 0xFFFFFFFFUL;

    __asm__ volatile("ltr %0" : : "r"((uint16_t)0x28));
    klog("[tss] loaded at %p  rsp0=%p\n", (void *)&tss,
         (void *)tss.rsp0);
}

void tss_set_rsp0(uint64_t rsp) {
    tss.rsp0 = rsp;
}
