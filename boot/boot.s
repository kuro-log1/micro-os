/* ============================================================
 * boot.s — GRUB multiboot2 진입점
 *
 * 흐름:
 *   GRUB (32-bit 보호 모드)
 *     → 페이지 테이블 설정 (1GB 아이덴티티 매핑)
 *     → PAE + 롱 모드 + 페이징 활성화
 *     → 64-bit GDT 로드 후 64-bit 코드로 점프
 *     → kernel_main(magic, info) 호출
 * ============================================================ */

/* ---------- multiboot2 헤더 ---------- */
.set MB2_MAGIC,  0xE85250D6
.set MB2_ARCH,   0           /* i386 */

.section .multiboot2
.align 8
mb2_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long mb2_end - mb2_start
    .long -(MB2_MAGIC + MB2_ARCH + (mb2_end - mb2_start))
    /* 종료 태그 */
    .short 0
    .short 0
    .long  8
mb2_end:

/* ---------- 64-bit GDT ---------- */
.section .data
.align 16
gdt64:
    .quad 0x0000000000000000    /* 0x00: null */
    .quad 0x00AF9A000000FFFF    /* 0x08: 커널 코드 (L=1, P=1, DPL=0) */
    .quad 0x00CF92000000FFFF    /* 0x10: 커널 데이터 (P=1, DPL=0, RW=1) */
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

/* GRUB이 전달한 값 저장 공간 */
mb_magic: .long 0
mb_info:  .long 0

/* ---------- 페이지 테이블 & 스택 (BSS) ---------- */
.section .bss
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pd:   .skip 4096

.align 16
stack_bottom: .skip 16384      /* 16KB 커널 스택 */
stack_top:

/* ============================================================
 * 32-bit 진입점
 * GRUB이 여기로 점프: eax = magic, ebx = multiboot2 info 주소
 * ============================================================ */
.section .text
.code32
.global _start
_start:
    cli

    /* multiboot 값 저장 (이후 레지스터 재사용 전에) */
    mov %eax, mb_magic
    mov %ebx, mb_info

    /* 32-bit 스택 설정 */
    mov $stack_top, %esp
    xor %ebp, %ebp

    /* ---------- 페이지 테이블 설정 ----------
     * PML4[0] → PDPT, PDPT[0] → PD
     * PD: 512개 × 2MB = 1GB 아이덴티티 매핑
     */
    mov $pdpt, %eax
    or  $0x3, %eax          /* Present | Writable */
    mov %eax, pml4

    mov $pd, %eax
    or  $0x3, %eax
    mov %eax, pdpt

    /* PD 512개 엔트리 채우기 */
    mov $pd, %edi
    mov $0x83, %eax         /* addr=0 | Present | Writable | LargePage(2MB) */
    xor %edx, %edx          /* 상위 32비트 = 0 */
    mov $512, %ecx
.fill_pd:
    mov %eax, (%edi)
    mov %edx, 4(%edi)
    add $8, %edi
    add $0x200000, %eax     /* 다음 2MB */
    loop .fill_pd

    /* ---------- 64-bit 모드 진입 ---------- */

    /* PAE 활성화 (CR4.PAE) */
    mov %cr4, %eax
    or  $0x20, %eax
    mov %eax, %cr4

    /* CR3 = PML4 주소 */
    mov $pml4, %eax
    mov %eax, %cr3

    /* EFER.LME = 1 (롱 모드 활성화) */
    mov $0xC0000080, %ecx
    rdmsr
    or  $0x100, %eax
    wrmsr

    /* CR0.PG = 1 (페이징 활성화) */
    mov %cr0, %eax
    or  $0x80000000, %eax
    mov %eax, %cr0

    /* 64-bit GDT 로드 */
    lgdt gdt64_ptr

    /* 코드 세그먼트 0x08로 64-bit 코드로 점프 */
    ljmpl $0x08, $start64

/* ============================================================
 * 64-bit 코드
 * ============================================================ */
.code64
start64:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* 64-bit 스택 재설정 */
    lea stack_top(%rip), %rsp
    xor %rbp, %rbp

    /* kernel_main(uint64_t magic, uint64_t info) 호출
     * System V AMD64 ABI: rdi = 1번째 인자, rsi = 2번째 인자 */
    movl mb_magic(%rip), %edi
    movl mb_info(%rip), %esi

    call kernel_main

    /* kernel_main이 리턴하면 여기서 멈춤 */
    cli
.hang:
    hlt
    jmp .hang
