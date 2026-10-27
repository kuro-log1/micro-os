/* 1부터 5까지 세는 유저 프로그램 */

static long sys_write(int fd, const char *buf, long len) {
    long ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(1L), "D"((long)fd), "S"(buf), "d"(len)
        : "memory");
    return ret;
}

static void sys_exit(int code) {
    __asm__ volatile("int $0x80"
        : : "a"(60L), "D"((long)code) : "memory");
    __builtin_unreachable();
}

static void print(const char *s) {
    long len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

static void print_int(int n) {
    char buf[2];
    buf[0] = '0' + n;
    buf[1] = '\n';
    sys_write(1, buf, 2);
}

void _start(void) {
    print("counting: ");
    for (int i = 1; i <= 5; i++)
        print_int(i);
    print("done!\n");
    sys_exit(0);
}
