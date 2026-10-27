#include "kernel/syscall.h"
#include "kernel/file.h"
#include "kernel/idt.h"
#include "kernel/elf.h"
#include "kernel/usermode.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/keyboard.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "proc/scheduler.h"
#include "proc/process.h"
#include "fs/ext2.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include <stdint.h>

extern void isr128(void);

void syscall_init(void) {
    idt_set_user_gate(0x80, isr128);
    kprintf("[syscall] int 0x80 gate registered (DPL=3)\n");
}

/* ── 내부 헬퍼 ── */

static file_t *fd_get(int fd) {
    process_t *p = current_process();
    if (fd < 0 || fd >= FD_MAX) return 0;
    return p->fd_table[fd];
}

static int fd_alloc(file_t *f) {
    process_t *p = current_process();
    for (int i = 0; i < FD_MAX; i++) {
        if (!p->fd_table[i]) {
            p->fd_table[i] = f;
            return i;
        }
    }
    return -1;
}

/* ── 경로 파싱: /foo/bar → dir_ino + "bar" ── */
static uint32_t path_lookup(const char *path) {
    if (!path || path[0] != '/') return 0;

    uint32_t ino = EXT2_ROOT_INO;
    const char *p = path + 1;

    while (*p) {
        char component[256];
        int i = 0;
        while (*p && *p != '/') component[i++] = *p++;
        component[i] = '\0';
        if (*p == '/') p++;

        if (i == 0) continue;

        ino = ext2_dir_lookup(ino, component);
        if (!ino) return 0;
    }
    return ino;
}

/* ── 시스템 콜 구현 ── */

static int sys_open(const char *path, int flags) {
    (void)flags;
    uint32_t ino = path_lookup(path);
    if (!ino) return -1;

    file_t *f = file_alloc();
    if (!f) return -1;

    f->type     = FILE_TYPE_EXT2;
    f->inode_no = ino;
    f->offset   = 0;

    int fd = fd_alloc(f);
    if (fd < 0) { file_free(f); return -1; }
    return fd;
}

static int sys_close(int fd) {
    file_t *f = fd_get(fd);
    if (!f) return -1;
    file_free(f);
    current_process()->fd_table[fd] = 0;
    return 0;
}

static int64_t sys_read(int fd, char *buf, uint64_t n) {
    file_t *f = fd_get(fd);
    if (!f) return -1;

    if (f->type == FILE_TYPE_STDIN) {
        for (uint64_t i = 0; i < n; i++) {
            buf[i] = keyboard_getchar();
            if (buf[i] == '\n') return (int64_t)(i + 1);
        }
        return (int64_t)n;
    }

    if (f->type == FILE_TYPE_EXT2) {
        ext2_inode_t inode;
        if (ext2_read_inode(f->inode_no, &inode) < 0) return -1;

        uint32_t remaining = inode.i_size - f->offset;
        if (n > remaining) n = remaining;
        if (n == 0) return 0;

        /* 파일 전체를 읽어서 offset부터 n바이트만 복사 (간단한 구현) */
        uint8_t *tmp = (uint8_t *)buf;
        uint32_t got = ext2_read_file_at(f->inode_no, tmp, f->offset, (uint32_t)n);
        f->offset += got;
        return (int64_t)got;
    }

    return -1;
}

static int64_t sys_write(int fd, const char *buf, uint64_t len) {
    file_t *f = fd_get(fd);
    if (!f) return -1;

    if (f->type == FILE_TYPE_STDOUT) {
        for (uint64_t i = 0; i < len; i++) {
            vga_putchar(buf[i]);
            serial_putchar(buf[i]);
        }
        return (int64_t)len;
    }

    if (f->type == FILE_TYPE_EXT2) {
        /* 파일 쓰기: ext2_write_file은 새 파일 생성 전용이므로 추후 확장 */
        return -1;
    }

    return -1;
}

static int64_t sys_lseek(int fd, int64_t offset, int whence) {
    file_t *f = fd_get(fd);
    if (!f || f->type != FILE_TYPE_EXT2) return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(f->inode_no, &inode) < 0) return -1;

    int64_t new_off;
    switch (whence) {
    case 0: new_off = offset; break;                              /* SEEK_SET */
    case 1: new_off = (int64_t)f->offset + offset; break;        /* SEEK_CUR */
    case 2: new_off = (int64_t)inode.i_size + offset; break;     /* SEEK_END */
    default: return -1;
    }

    if (new_off < 0) return -1;
    f->offset = (uint32_t)new_off;
    return new_off;
}

static void sys_exit(int code) {
    process_t *p = current_process();
    kprintf("[exit] pid=%d exit(code=%d): closing fds\n", p->pid, code);

    /* 열린 fd 전부 닫기 */
    for (int i = 0; i < FD_MAX; i++) {
        if (p->fd_table[i]) {
            file_free(p->fd_table[i]);
            p->fd_table[i] = 0;
        }
    }

    p->exit_code = code;
    p->state     = PROC_DEAD;
    kprintf("[exit] pid=%d marked PROC_DEAD, yielding\n", p->pid);
    schedule();
    __asm__ volatile("cli; hlt");
}

static int sys_wait(int pid) {
    process_t *child = scheduler_find(pid);
    if (!child) {
        kprintf("[wait] pid=%d not found\n", pid);
        return -1;
    }
    kprintf("[wait] pid=%d waiting for child pid=%d\n",
            current_process()->pid, pid);

    /* 자식이 종료될 때까지 yield */
    while (child->state != PROC_DEAD)
        schedule();

    int code = child->exit_code;
    kprintf("[wait] child pid=%d exited(code=%d), cleaning up\n", pid, code);

    /* 좀비 정리: 리스트에서 제거 후 메모리 해제 */
    scheduler_remove(child);
    vmm_free_user(child->pml4);
    if (child->pml4 != vmm_get_pml4())
        pmm_free_page(child->pml4);
    if (child->stack)
        kfree(child->stack);
    kfree(child);
    kprintf("[wait] zombie pid=%d reaped\n", pid);

    return code;
}

static void sys_exec(const char *user_path) {
    /* path를 커널 스택으로 복사 — vmm_free_user 후엔 유저 메모리가 사라짐 */
    char name[256];
    int  i = 0;
    const char *p = user_path;
    if (*p == '/') p++;
    while (i < 255 && *p) name[i++] = *p++;
    name[i] = '\0';

    process_t *proc = current_process();
    kprintf("[exec] pid=%d exec(/%s): freeing user space\n", proc->pid, name);

    /* 현재 유저 주소 공간 해제 (TLB 플러시 포함) */
    vmm_free_user(proc->pml4);
    kprintf("[exec] pid=%d user space freed, loading ELF\n", proc->pid);

    /* ELF 로드 → 새 유저 페이지 매핑 */
    uint64_t entry, rsp;
    if (elf_load(name, &entry, &rsp) < 0) {
        kprintf("[exec] pid=%d /%s not found — marking dead\n", proc->pid, name);
        proc->state = PROC_DEAD;
        schedule();
        __builtin_unreachable();
    }

    proc->user_rip = entry;
    proc->user_rsp = rsp;
    kprintf("[exec] pid=%d ELF loaded: entry=%p rsp=%p\n",
            proc->pid, (void *)entry, (void *)rsp);

    __asm__ volatile("sti");
    jump_to_usermode(entry, rsp);
    __builtin_unreachable();
}

static int sys_fork(registers_t *r) {
    process_t *child = process_fork(r);
    if (!child) return -1;
    kprintf("[fork] parent pid=%d → child pid=%d\n",
            current_process()->pid, child->pid);
    return child->pid;
}

void syscall_handler(registers_t *r) {
    switch (r->rax) {
    case SYS_READ:   r->rax = (uint64_t)sys_read ((int)r->rdi, (char *)r->rsi, r->rdx); break;
    case SYS_WRITE:  r->rax = (uint64_t)sys_write((int)r->rdi, (const char *)r->rsi, r->rdx); break;
    case SYS_OPEN:   r->rax = (uint64_t)sys_open ((const char *)r->rdi, (int)r->rsi); break;
    case SYS_CLOSE:  r->rax = (uint64_t)sys_close((int)r->rdi); break;
    case SYS_LSEEK:  r->rax = (uint64_t)sys_lseek((int)r->rdi, (int64_t)r->rsi, (int)r->rdx); break;
    case SYS_FORK:   r->rax = (uint64_t)sys_fork (r); break;
    case SYS_EXEC:   sys_exec((const char *)r->rdi); break;
    case SYS_WAIT:   r->rax = (uint64_t)sys_wait ((int)r->rdi); break;
    case SYS_EXIT:   sys_exit((int)r->rdi); break;
    default:
        kprintf("[syscall] unknown: rax=%lu\n", r->rax);
        break;
    }
}
