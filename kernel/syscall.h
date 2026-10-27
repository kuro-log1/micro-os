#pragma once
#include "kernel/idt.h"

#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_OPEN   2
#define SYS_CLOSE  3
#define SYS_LSEEK  8
#define SYS_EXIT   60

void syscall_init(void);
void syscall_handler(registers_t *r);
