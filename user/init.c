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

static int sys_wait(int pid) {
    long ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret) : "a"(61L), "D"((long)pid) : "memory");
    return (int)ret;
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
    print("[init] fork+exec+wait test\n");

    int pid = sys_fork();

    if (pid == 0) {
        /* 자식: /hello exec */
        print("[child] execing /hello\n");
        sys_exec("/hello");
        print("[child] exec failed\n");
        sys_exit(1);
    }

    /* 부모: 자식 종료까지 대기 */
    print("[parent] waiting for child pid=");
    /* 간단한 정수 출력 */
    char buf[4];
    buf[0] = '0' + (pid % 10);
    buf[1] = '\n';
    sys_write(1, buf, 2);

    int code = sys_wait(pid);
    (void)code;

    print("[parent] child exited, done!\n");
    sys_exit(0);
}
