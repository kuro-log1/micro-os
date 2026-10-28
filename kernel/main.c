#include <stdint.h>

#define MB2_BOOT_MAGIC 0x36D76289  /* GRUB이 eax로 넘겨 주는 값 */

void kernel_main(uint64_t magic, uint64_t info) {
    (void)info;

    /* 아직 출력 드라이버가 없다: VGA 텍스트 버퍼 두 번째 줄(80칸 뒤)에 직접 쓴다 */
    volatile uint16_t *vga = (uint16_t *)0xB8000 + 80;
    const char *msg = (magic == MB2_BOOT_MAGIC) ? "Hello from 64-bit C! (magic OK)"
                                                : "Hello from 64-bit C! (bad magic)";
    for (int i = 0; msg[i]; i++)
        vga[i] = (0x0F << 8) | msg[i];

    for (;;) __asm__ volatile("hlt");
}
