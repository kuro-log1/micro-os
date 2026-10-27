#include "kernel/mmap.h"
#include "kernel/multiboot2.h"
#include "lib/printf.h"
#include <stdint.h>

static const char *mem_type_name(uint32_t t) {
    switch (t) {
    case 1: return "usable";
    case 2: return "reserved";
    case 3: return "ACPI reclaimable";
    case 4: return "ACPI NVS";
    case 5: return "bad RAM";
    default: return "unknown";
    }
}

void mmap_log(uint64_t mb2_info) {
    struct mb2_tag *tag = (struct mb2_tag *)(mb2_info + 8);

    while (tag->type != MB2_TAG_END) {
        if (tag->type == MB2_TAG_MMAP) {
            struct mb2_tag_mmap *mmap = (struct mb2_tag_mmap *)tag;
            uint8_t *entry = (uint8_t *)(mmap + 1);
            uint8_t *end   = (uint8_t *)mmap + mmap->size;

            klog("[mem] BIOS memory map:\n");
            while (entry < end) {
                struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)entry;
                uint64_t start = e->base_addr;
                uint64_t last  = e->base_addr + e->length - 1;
                uint64_t kb    = e->length / 1024;

                if (kb >= 1024)
                    klog("[mem]   [%08lx-%08lx] %s (%lu MB)\n",
                         (unsigned long)start, (unsigned long)last,
                         mem_type_name(e->type), (unsigned long)(kb / 1024));
                else if (kb > 0)
                    klog("[mem]   [%08lx-%08lx] %s (%lu KB)\n",
                         (unsigned long)start, (unsigned long)last,
                         mem_type_name(e->type), (unsigned long)kb);
                else
                    klog("[mem]   [%08lx-%08lx] %s\n",
                         (unsigned long)start, (unsigned long)last,
                         mem_type_name(e->type));

                entry += mmap->entry_size;
            }
            return;
        }
        tag = (struct mb2_tag *)(((uint64_t)tag + tag->size + 7) & ~7UL);
    }
}
