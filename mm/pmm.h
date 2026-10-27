#pragma once
#include <stdint.h>

void  pmm_init(uint64_t mb2_info);
void *pmm_alloc_page(void);
void  pmm_free_page(void *addr);
void  pmm_dump_stats(void);
