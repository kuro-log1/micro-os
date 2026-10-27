#include "kernel/file.h"
#include "lib/string.h"

static file_t file_pool[FILE_TABLE_SIZE];

void file_table_init(void) {
    memset(file_pool, 0, sizeof(file_pool));
}

file_t *file_alloc(void) {
    for (int i = 0; i < FILE_TABLE_SIZE; i++) {
        if (file_pool[i].refcount == 0) {
            memset(&file_pool[i], 0, sizeof(file_t));
            file_pool[i].refcount = 1;
            return &file_pool[i];
        }
    }
    return 0;
}

void file_free(file_t *f) {
    if (!f) return;
    f->refcount--;
    if (f->refcount == 0)
        memset(f, 0, sizeof(file_t));
}
