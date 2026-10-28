/* ============================================================
 * boot.s — GRUB multiboot2 진입점
 *
 * 흐름:
 *   GRUB (32-bit 보호 모드)
 *     → GRUB이 넘겨 준 값 저장, 32-bit 스택 설정
 *     → VGA 텍스트 버퍼에 "OK" 를 직접 쓰고 멈춘다
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

.section .data
/* GRUB이 전달한 값 저장 공간 */
mb_magic: .long 0
mb_info:  .long 0

/* ---------- 스택 (BSS) ---------- */
.section .bss
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

    /* 확인용: 화면 왼쪽 위에 "OK" (글자 바이트 | 색 바이트 0x2F = 초록 바탕 흰 글자) */
    movl $0x2F4B2F4F, 0xB8000

.hang32:
    hlt
    jmp .hang32
