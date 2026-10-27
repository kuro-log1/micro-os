/* ring 3 유저 프로세스 — 시스템 콜은 int 0x80 사용 */

typedef long ssize_t;

static ssize_t sys_write(int fd, const char *buf, long len) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(1L), "D"((long)fd), "S"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static int sys_open(const char *path, int flags) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(2L), "D"(path), "S"((long)flags)
        : "memory"
    );
    return (int)ret;
}

static ssize_t sys_read(int fd, char *buf, long len) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(0L), "D"((long)fd), "S"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static int sys_close(int fd) {
    long ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(3L), "D"((long)fd)
        : "memory"
    );
    return (int)ret;
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

static void print(const char *s) {
    long len = 0;
    while (s[len]) len++;
    sys_write(1, s, len);
}

void _start(void) {
    print("Hello from /init (ring 3 ELF)!\n");

    /* sys_open / sys_read 테스트 */
    int fd = sys_open("/hello.txt", 0);
    if (fd >= 0) {
        char buf[128];
        ssize_t n = sys_read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            print("[init] /hello.txt: ");
            print(buf);
        }
        sys_close(fd);
    } else {
        print("[init] /hello.txt not found\n");
    }

    sys_exit(0);
}
