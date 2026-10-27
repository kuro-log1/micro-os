#include "shell/shell.h"
#include "drivers/keyboard.h"
#include "drivers/vga.h"
#include "fs/fs.h"
#include "fs/ext2.h"
#include "kernel/elf.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "lib/printf.h"
#include "lib/string.h"

#define LINE_MAX  256
#define ARGC_MAX   16

static void readline(char *buf) {
    int i = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            buf[i] = '\0';
            printf("\n");
            return;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
        } else if (i < LINE_MAX - 1) {
            buf[i++] = c;
            printf("%c", c);
        }
    }
}

static int split(char *line, char **argv) {
    int argc = 0;
    char *p = line;
    while (*p && argc < ARGC_MAX) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* 유저 ELF를 로드해 실행하고 종료까지 기다린 뒤 정리한다 */
static void shell_spawn(const char *name) {
    uint64_t entry, rsp;
    if (elf_load(name, &entry, &rsp) < 0) {
        printf("%s: not found\n", name);
        return;
    }

    process_t *child = process_create_user(entry, rsp);
    scheduler_add(child);

    /* 자식이 종료될 때까지 yield */
    while (child->state != PROC_DEAD)
        schedule();

    /* zombie 정리 */
    scheduler_remove(child);
    vmm_free_user(child->pml4);
    if (child->stack) kfree(child->stack);
    kfree(child);
}

void shell_run(void) {
    vga_clear();
    printf("micro-os shell. 'help' for commands.\n");

    char  line[LINE_MAX];
    char *argv[ARGC_MAX];

    while (1) {
        printf("> ");
        readline(line);

        int argc = split(line, argv);
        if (argc == 0) continue;

        if (strcmp(argv[0], "help") == 0) {
            printf("  help            this message\n");
            printf("  clear           clear screen\n");
            printf("  echo [args]     print arguments\n");
            printf("  pwd             print working directory\n");
            printf("  ls              list directory\n");
            printf("  mkdir <name>    create directory\n");
            printf("  touch <name>    create file\n");
            printf("  cd <name>       change directory\n");
            printf("  cat <file>      print file contents (disk)\n");
            printf("  write <file> <text>  write text to disk file\n");
        } else if (strcmp(argv[0], "clear") == 0) {
            vga_clear();
        } else if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; i < argc; i++) {
                if (i > 1) printf(" ");
                printf("%s", argv[i]);
            }
            printf("\n");
        } else if (strcmp(argv[0], "pwd") == 0) {
            fs_pwd();
        } else if (strcmp(argv[0], "ls") == 0) {
            ext2_dir_ls(EXT2_ROOT_INO);
        } else if (strcmp(argv[0], "mkdir") == 0) {
            if (argc < 2) printf("usage: mkdir <name>\n");
            else fs_mkdir(fs_cwd(), argv[1]);
        } else if (strcmp(argv[0], "touch") == 0) {
            if (argc < 2) printf("usage: touch <name>\n");
            else fs_touch(fs_cwd(), argv[1]);
        } else if (strcmp(argv[0], "cd") == 0) {
            if (argc < 2) printf("usage: cd <name>\n");
            else fs_cd(argv[1]);
        } else if (strcmp(argv[0], "cat") == 0) {
            if (argc < 2) {
                printf("usage: cat <file>\n");
            } else {
                uint32_t ino = ext2_dir_lookup(EXT2_ROOT_INO, argv[1]);
                if (ino == 0) {
                    printf("cat: %s: no such file\n", argv[1]);
                } else {
                    static char filebuf[4096];
                    int n = ext2_read_file(ino, filebuf, sizeof(filebuf) - 1);
                    if (n <= 0) {
                        printf("cat: read error\n");
                    } else {
                        filebuf[n] = '\0';
                        printf("%s", filebuf);
                    }
                }
            }
        } else if (strcmp(argv[0], "write") == 0) {
            if (argc < 3) {
                printf("usage: write <file> <text>\n");
            } else {
                if (ext2_write_file(EXT2_ROOT_INO, argv[1],
                                    argv[2], strlen(argv[2])) == 0)
                    printf("wrote '%s'\n", argv[1]);
                else
                    printf("write failed\n");
            }
        } else {
            /* 빌트인이 아니면 ext2에서 찾아 실행 */
            shell_spawn(argv[0]);
        }
    }
}
