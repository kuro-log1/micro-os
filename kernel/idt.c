#include "kernel/idt.h"
#include "kernel/panic.h"
#include "kernel/pic.h"
#include "lib/printf.h"
#include <stdint.h>

/* 64-bit IDT 게이트 디스크립터 (16바이트) */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;  /* P | DPL | 0 | Type */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_gate_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

#define INT_GATE 0x8E  /* Present=1, DPL=0, Type=E(인터럽트 게이트) */

static idt_gate_t idt[256];
static idtr_t     idtr;

/* idt_asm.s에서 정의된 IRQ 스텁 선언 */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static void (*irq_stubs[16])(void) = {
    irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
    irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15,
};

static irq_handler_t irq_table[16];

/* idt_asm.s에서 정의된 ISR 스텁 선언 */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

static void (*isr_table[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};

static void idt_set_gate(int n, void (*fn)(void)) {
    uint64_t addr = (uint64_t)fn;
    idt[n].offset_low  = addr & 0xFFFF;
    idt[n].selector    = 0x08;  /* 커널 코드 세그먼트 */
    idt[n].ist         = 0;
    idt[n].type_attr   = INT_GATE;
    idt[n].offset_mid  = (addr >> 16) & 0xFFFF;
    idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}


static const char *exc_names[] = {
    "Divide by Zero",        "Debug",               "NMI",
    "Breakpoint",            "Overflow",             "Bound Range Exceeded",
    "Invalid Opcode",        "Device Not Available", "Double Fault",
    "Coprocessor Seg Overrun","Invalid TSS",         "Segment Not Present",
    "Stack Fault",           "General Protection",   "Page Fault",
    "Reserved",              "x87 FP Exception",     "Alignment Check",
    "Machine Check",         "SIMD FP Exception",    "Virtualization",
};

/* idt_asm.s의 isr_common에서 호출됨 */
void exception_handler(registers_t *r) {
    /* #PF(14)는 CR2에 폴트 발생 가상 주소가 담김 */
    if (r->int_no == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        printf("[vmm] Page Fault: addr=%p  err=0x%lx (%s %s %s)\n",
               (void *)cr2, r->err_code,
               (r->err_code & 1) ? "protection"  : "not-present",
               (r->err_code & 2) ? "write"        : "read",
               (r->err_code & 4) ? "user"         : "kernel");
    }
    const char *name = (r->int_no < 21) ? exc_names[r->int_no] : "Reserved";
    panic(r, name);
}

void irq_install_handler(int irq, irq_handler_t fn) {
    irq_table[irq] = fn;
}

void irq_handler(registers_t *r) {
    int irq = (int)r->int_no - 32;
    pic_send_eoi(irq);
    if (irq_table[irq])
        irq_table[irq](r);
}

void irq_init(void) {
    for (int i = 0; i < 16; i++)
        idt_set_gate(32 + i, irq_stubs[i]);
}

void idt_init(void) {
    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_table[i]);

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idtr));
}
