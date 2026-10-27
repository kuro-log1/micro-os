#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "kernel/idt.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "lib/printf.h"

void kernel_main(uint64_t magic, uint64_t info) {
    (void)magic;

    vga_init();
    serial_init();

    printf("============================\n");
    printf("  micro-os booted!\n");
    printf("============================\n");

    idt_init();
    pic_init();
    irq_init();
    timer_init(100);
    keyboard_init();
    mouse_init();
    __asm__ volatile("sti");

    pmm_init(info);
    vmm_init();

    heap_init();

    /* [실습] 블록 분할(kmalloc)과 병합(kfree) 관찰 */
    void *a = kmalloc(100);
    void *b = kmalloc(200);
    printf("[test] a=%p b=%p\n", a, b);
    heap_dump();
    kfree(b);
    kfree(a);
    heap_dump();

    /* 키보드 IRQ가 링 버퍼에 넣은 글자를 꺼내 화면에 에코 */
    for (;;)
        printf("%c", keyboard_getchar());
}
