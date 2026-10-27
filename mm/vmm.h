#pragma once
#include <stdint.h>

#define VMM_PRESENT  (1UL << 0)
#define VMM_WRITABLE (1UL << 1)
#define VMM_USER     (1UL << 2)

void      vmm_init(void);
void      vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void      vmm_unmap_page(uint64_t *pml4, uint64_t virt);
uint64_t *vmm_get_pml4(void);
uint64_t *vmm_fork(uint64_t *parent_pml4);
