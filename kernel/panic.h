#pragma once
#include "kernel/idt.h"

__attribute__((noreturn))
void panic(registers_t *r, const char *msg);
