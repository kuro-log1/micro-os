#include <stdint.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "kernel/idt.h"
#include "kernel/pic.h"
#include "kernel/timer.h"
#include "mm/pmm.h"
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

    /* [실습] 4KB 프레임 할당/해제 — 해제한 프레임이 다시 나오는지 확인 */
    void *a = pmm_alloc_page();
    void *b = pmm_alloc_page();
    printf("[test] alloc a=%p b=%p\n", a, b);
    pmm_free_page(a);
    printf("[test] free a, alloc again=%p\n", pmm_alloc_page());
    pmm_dump_stats();

    /* 키보드 IRQ가 링 버퍼에 넣은 글자를 꺼내 화면에 에코 */
    for (;;)
        printf("%c", keyboard_getchar());
}
