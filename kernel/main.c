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

    /* [실습] 아직 아무것도 없는 가상 주소(16GB)에 새 프레임을 매핑해 읽고 쓰기 */
    uint64_t *pml4 = vmm_get_pml4();
    uint64_t  virt = 0x0000000400000000UL;
    uint64_t  phys = (uint64_t)pmm_alloc_page();
    vmm_map_page(pml4, virt, phys, VMM_WRITABLE);
    volatile uint64_t *v = (uint64_t *)virt;
    *v = 0xC0FFEE;
    printf("[test] virt=%p -> phys=%p  value=0x%lx\n", (void *)virt, (void *)phys, *v);

    /* [실습] 주석을 풀면 매핑 해제 후 접근 → Page Fault (not-present read kernel) */
    /* vmm_unmap_page(pml4, virt); printf("%lx\n", *v); */

    /* 키보드 IRQ가 링 버퍼에 넣은 글자를 꺼내 화면에 에코 */
    for (;;)
        printf("%c", keyboard_getchar());
}
