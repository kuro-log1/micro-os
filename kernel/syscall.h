#pragma once
#include "kernel/idt.h"

#define SYS_WRITE  1
#define SYS_EXIT   60

void syscall_init(void);
void syscall_handler(registers_t *r);
