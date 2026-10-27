#include "mm/heap.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "lib/printf.h"
#include <stdint.h>
#include <stddef.h>

#define HEAP_BASE  0x0000000300000000UL  /* 12GB 가상 주소 */
#define HEAP_PAGES 256                   /* 256 × 4KB = 1MB */
#define ALIGN8(x)  (((x) + 7UL) & ~7UL)

/*
 * 블록 헤더 (24바이트)
 * [header][  data  ][header][  data  ] ...
 */
typedef struct block {
    uint64_t      size;  /* 데이터 크기 (헤더 제외) */
    uint64_t      used;  /* 1 = 사용 중, 0 = 빈 블록 */
    struct block *next;  /* 다음 블록 */
} block_t;

static block_t *heap_start = NULL;

void heap_init(void) {
    uint64_t *pml4 = vmm_get_pml4();

    for (int i = 0; i < HEAP_PAGES; i++) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        uint64_t virt = HEAP_BASE + (uint64_t)i * 4096;
        vmm_map_page(pml4, virt, phys, VMM_WRITABLE);
    }

    heap_start       = (block_t *)HEAP_BASE;
    heap_start->size = HEAP_PAGES * 4096 - sizeof(block_t);
    heap_start->used = 0;
    heap_start->next = NULL;

    klog("[heap] %u MB at %p\n",
         HEAP_PAGES * 4096 / 1024 / 1024, (void *)HEAP_BASE);
}

void *kmalloc(uint64_t size) {
    size = ALIGN8(size);

    for (block_t *b = heap_start; b; b = b->next) {
        if (b->used || b->size < size)
            continue;

        /* 남은 공간이 충분하면 블록 분할 */
        if (b->size >= size + sizeof(block_t) + 8) {
            block_t *split  = (block_t *)((uint8_t *)(b + 1) + size);
            split->size     = b->size - size - sizeof(block_t);
            split->used     = 0;
            split->next     = b->next;
            b->size         = size;
            b->next         = split;
        }

        b->used = 1;
        return (void *)(b + 1);
    }

    return NULL;  /* OOM */
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_t *b = (block_t *)ptr - 1;
    b->used = 0;

    /* 바로 뒤 블록이 빈 경우 병합 */
    if (b->next && !b->next->used) {
        b->size += sizeof(block_t) + b->next->size;
        b->next  = b->next->next;
    }
}

void heap_dump(void) {
    uint64_t used = 0, free = 0, blocks = 0;
    for (block_t *b = heap_start; b; b = b->next) {
        blocks++;
        if (b->used) used += b->size;
        else         free += b->size;
    }
    kprintf("[heap] blocks=%lu  used=%lu B  free=%lu B\n", blocks, used, free);
}
