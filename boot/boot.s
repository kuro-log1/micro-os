/* ============================================================
 * boot.s — GRUB multiboot2 진입점
 *
 * 흐름:
 *   GRUB (32-bit 보호 모드)
 *     → _start: 아직 아무것도 하지 않고 멈춘다
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

/* ============================================================
 * 32-bit 진입점
 * GRUB이 여기로 점프: eax = magic, ebx = multiboot2 info 주소
 * ============================================================ */
.section .text
.code32
.global _start
_start:
    cli
.hang32:
    hlt
    jmp .hang32
