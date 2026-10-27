#pragma once
#include <stdint.h>

/* ELF64 헤더 */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;        /* ET_EXEC = 2 */
    uint16_t e_machine;     /* EM_X86_64 = 62 */
    uint32_t e_version;
    uint64_t e_entry;       /* 진입점 가상 주소 */
    uint64_t e_phoff;       /* Program Header 테이블 오프셋 */
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;   /* Program Header 크기 (56바이트) */
    uint16_t e_phnum;       /* Program Header 수 */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

/* Program Header */
typedef struct {
    uint32_t p_type;    /* PT_LOAD = 1 */
    uint32_t p_flags;   /* PF_X=1, PF_W=2, PF_R=4 */
    uint64_t p_offset;  /* 파일 내 오프셋 */
    uint64_t p_vaddr;   /* 매핑할 가상 주소 */
    uint64_t p_paddr;
    uint64_t p_filesz;  /* 파일에서 복사할 크기 */
    uint64_t p_memsz;   /* 메모리 크기 (filesz 이후는 0으로 초기화 = BSS) */
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

#define ELF_MAGIC    0x464C457FU  /* "\x7fELF" little-endian */
#define ET_EXEC      2
#define EM_X86_64    62
#define PT_LOAD      1
#define PF_W         0x2

/* ELF 파일 로드: ext2 루트에서 name을 찾아 유저 주소 공간에 매핑
 * 성공 시 0 반환, *entry = 진입점, *rsp = 초기 스택 포인터
 * 실패 시 -1 반환 */
int elf_load(const char *name, uint64_t *entry, uint64_t *rsp);
