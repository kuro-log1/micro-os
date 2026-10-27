/* ring 3 유저 프로세스 — 시스템 콜은 int 0x80 사용 */

static void sys_write(const char *buf, long len) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(1L), "D"(1L), "S"(buf), "d"(len)
        : "memory"
    );
}

static void sys_exit(int code) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(60L), "D"((long)code)
        : "memory"
    );
    __builtin_unreachable();
}

void _start(void) {
    const char msg[] = "Hello from /init (ring 3 ELF)!\n";
    sys_write(msg, sizeof(msg) - 1);
    sys_exit(0);
}
