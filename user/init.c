/* ring 3 유저 프로세스 */

typedef long ssize_t;

static int sys_fork(void) {
    long ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret) : "a"(57L) : "memory");
    return (int)ret;
}

static void sys_exec(const char *path) {
    __asm__ volatile("int $0x80"
        : : "a"(59L), "D"(path) : "memory");
}

static ssize_t sys_write(int fd, const char *buf, long len) {
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

void _start(void) {
    int pid = sys_fork();

    if (pid == 0) {
        /* 자식: /hello를 exec */
        print("[child] execing /hello\n");
        sys_exec("/hello");
        print("[child] exec failed!\n");
        sys_exit(1);
    }

    /* 부모: 자식 fork 확인 후 종료 (wait은 Step 3에서) */
    print("[parent] forked child, exiting\n");
    sys_exit(0);
}
