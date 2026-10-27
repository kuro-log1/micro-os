#pragma once
#include <stdint.h>

/* multiboot2 info 헤더 (8바이트) 다음에 태그들이 연속으로 붙음 */
struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

/* 메모리 맵 태그 (type = 6) */
struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* 이후 mb2_mmap_entry 배열 */
} __attribute__((packed));

/* 메모리 맵 엔트리 */
struct mb2_mmap_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;      /* 1 = 사용 가능한 RAM */
    uint32_t reserved;
} __attribute__((packed));

#define MB2_TAG_END       0
#define MB2_TAG_MMAP      6
#define MB2_MEM_AVAILABLE 1
