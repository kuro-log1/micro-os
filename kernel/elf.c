#include "kernel/elf.h"
#include "fs/ext2.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include <stdint.h>
#include <stddef.h>

/* 유저 스택: 0x7FF00000 ~ 0x7FF04000 (16KB), RSP = 상단 */
#define USER_STACK_BASE 0x7FF00000UL
#define USER_STACK_SIZE 0x4000UL     /* 4 pages = 16 KB */

static uint64_t map_user_stack(void) {
    for (uint64_t va = USER_STACK_BASE;
         va < USER_STACK_BASE + USER_STACK_SIZE;
         va += 0x1000) {
        void *phys = pmm_alloc_page();
        if (!phys) return 0;
        vmm_map_page(vmm_get_pml4(), va, (uint64_t)phys,
                     VMM_PRESENT | VMM_WRITABLE | VMM_USER);
    }
    return USER_STACK_BASE + USER_STACK_SIZE;  /* 스택 최상단 */
}

int elf_load(const char *name, uint64_t *entry, uint64_t *rsp) {
    /* ── 1. ext2에서 파일 찾기 ── */
    uint32_t ino = ext2_dir_lookup(EXT2_ROOT_INO, name);
    if (ino == 0) {
        klog("[elf] /%s: not found\n", name);
        return -1;
    }

    ext2_inode_t inode;
    if (ext2_read_inode(ino, &inode) < 0) return -1;

    uint32_t fsize = inode.i_size;
    uint8_t *buf   = kmalloc(fsize);
    if (!buf) {
        klog("[elf] kmalloc(%u) failed\n", fsize);
        return -1;
    }

    int n = ext2_read_file(ino, buf, fsize);
    if (n <= 0) {
        klog("[elf] read error\n");
        kfree(buf);
        return -1;
    }

    /* ── 2. ELF 검증 ── */
    Elf64_Ehdr *eh = (Elf64_Ehdr *)buf;
    uint32_t magic;
    memcpy(&magic, eh->e_ident, 4);

    if (magic != ELF_MAGIC || eh->e_ident[4] != 2 /* ELFCLASS64 */ ||
        eh->e_machine != EM_X86_64 || eh->e_type != ET_EXEC) {
        klog("[elf] /%s: invalid ELF (magic=%x class=%d machine=%d)\n",
             name, magic, eh->e_ident[4], eh->e_machine);
        kfree(buf);
        return -1;
    }

    /* ── 3. PT_LOAD 세그먼트 매핑 ── */
    Elf64_Phdr *ph = (Elf64_Phdr *)(buf + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;

        uint64_t vstart = ph[i].p_vaddr & ~0xFFFUL;
        uint64_t vend   = (ph[i].p_vaddr + ph[i].p_memsz + 0xFFFUL) & ~0xFFFUL;
        uint32_t flags  = VMM_PRESENT | VMM_USER;
        if (ph[i].p_flags & PF_W) flags |= VMM_WRITABLE;

        klog("[elf]   seg[%d] vaddr=%lx  filesz=%u  memsz=%u  %s%s\n",
             i, (unsigned long)ph[i].p_vaddr,
             ph[i].p_filesz, ph[i].p_memsz,
             (ph[i].p_flags & 0x4) ? "R" : "-",
             (ph[i].p_flags & PF_W) ? "W" : "-");

        /* 페이지 단위로 물리 메모리 할당 + 매핑 */
        for (uint64_t va = vstart; va < vend; va += 0x1000) {
            void *phys = pmm_alloc_page();
            if (!phys) {
                klog("[elf] pmm_alloc_page failed\n");
                kfree(buf);
                return -1;
            }
            vmm_map_page(vmm_get_pml4(), va, (uint64_t)phys, flags);
        }

        /* 파일 데이터 복사 */
        memcpy((void *)ph[i].p_vaddr, buf + ph[i].p_offset, ph[i].p_filesz);

        /* BSS 영역 0 초기화 (p_memsz > p_filesz인 부분) */
        if (ph[i].p_memsz > ph[i].p_filesz)
            memset((void *)(ph[i].p_vaddr + ph[i].p_filesz), 0,
                   ph[i].p_memsz - ph[i].p_filesz);
    }

    kfree(buf);

    /* ── 4. 유저 스택 할당 ── */
    uint64_t stack_top = map_user_stack();
    if (stack_top == 0) {
        klog("[elf] stack alloc failed\n");
        return -1;
    }

    *entry = eh->e_entry;
    *rsp   = stack_top;

    klog("[elf] /%s loaded  entry=%lx  rsp=%lx\n",
         name, (unsigned long)*entry, (unsigned long)*rsp);
    return 0;
}
