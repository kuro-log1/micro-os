#pragma once
#include "proc/process.h"

void       scheduler_init(process_t *idle);
void       scheduler_add(process_t *p);
void       scheduler_remove(process_t *p);
process_t *scheduler_find(int pid);
void       schedule(void);
process_t *current_process(void);
