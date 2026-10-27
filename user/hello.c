/* ring 3 유저 프로세스 — /init이 exec해서 실행 */

static void sys_write(int fd, const char *buf, long len) {
    __asm__ volatile("int $0x80"
        : : "a"(1L), "D"((long)fd), "S"(buf), "d"(len) : "memory");
}

static void sys_exit(int code) {
    __asm__ volatile("int $0x80"
        : : "a"(60L), "D"((long)code) : "memory");
    __builtin_unreachable();
}

void _start(void) {
    const char msg[] = "Hello from exec'd /hello!\n";
    long len = sizeof(msg) - 1;
    sys_write(1, msg, len);
    sys_exit(0);
}
