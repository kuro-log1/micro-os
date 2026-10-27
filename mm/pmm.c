#include "mm/pmm.h"
#include "kernel/multiboot2.h"
#include "lib/printf.h"
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE  4096UL
#define MAX_PAGES  (4UL * 1024 * 1024 * 1024 / PAGE_SIZE)  /* 4GB = 1M 페이지 */

/* 비트맵: 1 = 사용 가능, 0 = 사용 중 / 예약 */
static uint8_t bitmap[MAX_PAGES / 8];  /* 128KB */

static uint64_t total_pages = 0;
static uint64_t free_pages  = 0;

/* linker.ld에서 정의 — 커널 끝 주소 (4K 정렬) */
extern char _kernel_end[];

static inline void page_set(uint64_t p)   { bitmap[p / 8] |=  (1 << (p % 8)); }
static inline void page_clear(uint64_t p) { bitmap[p / 8] &= ~(1 << (p % 8)); }
static inline int  page_test(uint64_t p)  { return (bitmap[p / 8] >> (p % 8)) & 1; }

void pmm_init(uint64_t mb2_info) {
    uint64_t kernel_end_page = (uint64_t)_kernel_end / PAGE_SIZE;

    /* multiboot2 태그 탐색 */
    struct mb2_tag *tag = (struct mb2_tag *)(mb2_info + 8);

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_MMAP) {
            struct mb2_tag_mmap *mmap = (struct mb2_tag_mmap *)tag;
            uint8_t *entry = (uint8_t *)(mmap + 1);
            uint8_t *end   = (uint8_t *)mmap + mmap->size;

            while (entry < end) {
                struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)entry;

                if (e->type == MB2_MEM_AVAILABLE) {
                    uint64_t p_start = (e->base_addr + PAGE_SIZE - 1) / PAGE_SIZE;
                    uint64_t p_end   = (e->base_addr + e->length)     / PAGE_SIZE;

                    for (uint64_t p = p_start; p < p_end && p < MAX_PAGES; p++) {
                        total_pages++;
                        /* 커널이 차지한 페이지는 예약 유지 */
                        if (p >= kernel_end_page) {
                            page_set(p);
                            free_pages++;
                        }
                    }
                }

                entry += mmap->entry_size;
            }
        }

        /* 태그는 8바이트 정렬 */
        tag = (struct mb2_tag *)(((uint64_t)tag + tag->size + 7) & ~7UL);
    }

    klog("[pmm] total: %lu MB  free: %lu MB  (%lu pages)\n",
         total_pages * PAGE_SIZE / (1024 * 1024),
         free_pages  * PAGE_SIZE / (1024 * 1024),
         free_pages);
}

void *pmm_alloc_page(void) {
    for (uint64_t i = 0; i < MAX_PAGES; i++) {
        if (page_test(i)) {
            page_clear(i);
            free_pages--;
            return (void *)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free_page(void *addr) {
    uint64_t p = (uint64_t)addr / PAGE_SIZE;
    if (p < MAX_PAGES && !page_test(p)) {
        page_set(p);
        free_pages++;
    }
}

void pmm_dump_stats(void) {
    klog("[pmm] free: %lu pages (%lu MB)\n",
         free_pages, free_pages * PAGE_SIZE / (1024 * 1024));
}
