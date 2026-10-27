#pragma once
#include "proc/process.h"

void       scheduler_init(process_t *idle);
void       scheduler_add(process_t *p);
void       schedule(void);
process_t *current_process(void);
