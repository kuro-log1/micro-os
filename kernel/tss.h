#pragma once
#include <stdint.h>

void tss_init(void);
void tss_set_rsp0(uint64_t rsp);
