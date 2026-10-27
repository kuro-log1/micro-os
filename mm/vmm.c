#include "mm/vmm.h"
#include "mm/pmm.h"
#include "lib/printf.h"
#include <stdint.h>

/* 가상 주소에서 각 레벨 인덱스 추출 */
#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)

/* 페이지 테이블 엔트리에서 물리 주소 추출 (하위 12비트 플래그 제거) */
#define ENTRY_ADDR(e) ((e) & ~0xFFFUL)

/* 4KB 페이지 테이블 페이지를 0으로 초기화 (512개 엔트리) */
static void clear_page(uint64_t *p) {
    for (int i = 0; i < 512; i++)
        p[i] = 0;
}

uint64_t *vmm_get_pml4(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return (uint64_t *)ENTRY_ADDR(cr3);
}

void vmm_init(void) {
    printf("[vmm] kernel PML4 at %p\n", (void *)vmm_get_pml4());
}

void vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4i = PML4_IDX(virt);
    uint64_t pdpti = PDPT_IDX(virt);
    uint64_t pdi   = PD_IDX(virt);
    uint64_t pti   = PT_IDX(virt);

    /* PML4 → PDPT */
    if (!(pml4[pml4i] & VMM_PRESENT)) {
        uint64_t *pdpt = pmm_alloc_page();
        clear_page(pdpt);
        pml4[pml4i] = (uint64_t)pdpt | VMM_PRESENT | VMM_WRITABLE;
    }
    uint64_t *pdpt = (uint64_t *)ENTRY_ADDR(pml4[pml4i]);

    /* PDPT → PD */
    if (!(pdpt[pdpti] & VMM_PRESENT)) {
        uint64_t *pd = pmm_alloc_page();
        clear_page(pd);
        pdpt[pdpti] = (uint64_t)pd | VMM_PRESENT | VMM_WRITABLE;
    }
    uint64_t *pd = (uint64_t *)ENTRY_ADDR(pdpt[pdpti]);

    /* PD → PT (PS 비트 세트 = 2MB 대형 페이지가 이미 있으면 거절) */
    if (pd[pdi] & (1UL << 7)) {
        printf("[vmm] ERROR: 2MB large page exists at virt=%p\n", (void *)virt);
        return;
    }
    if (!(pd[pdi] & VMM_PRESENT)) {
        uint64_t *pt = pmm_alloc_page();
        clear_page(pt);
        pd[pdi] = (uint64_t)pt | VMM_PRESENT | VMM_WRITABLE;
    }
    uint64_t *pt = (uint64_t *)ENTRY_ADDR(pd[pdi]);

    /* PT → 실제 페이지 */
    pt[pti] = phys | flags | VMM_PRESENT;

    /* TLB에서 이 가상 주소 항목 무효화 */
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void vmm_unmap_page(uint64_t *pml4, uint64_t virt) {
    uint64_t pml4i = PML4_IDX(virt);
    uint64_t pdpti = PDPT_IDX(virt);
    uint64_t pdi   = PD_IDX(virt);
    uint64_t pti   = PT_IDX(virt);

    if (!(pml4[pml4i] & VMM_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)ENTRY_ADDR(pml4[pml4i]);

    if (!(pdpt[pdpti] & VMM_PRESENT)) return;
    uint64_t *pd = (uint64_t *)ENTRY_ADDR(pdpt[pdpti]);

    if (!(pd[pdi] & VMM_PRESENT)) return;
    uint64_t *pt = (uint64_t *)ENTRY_ADDR(pd[pdi]);

    pt[pti] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
