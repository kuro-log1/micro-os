#pragma once
#include <stdint.h>

typedef enum {
    FILE_TYPE_NONE   = 0,
    FILE_TYPE_STDIN  = 1,
    FILE_TYPE_STDOUT = 2,
    FILE_TYPE_EXT2   = 3,
} file_type_t;

typedef struct {
    file_type_t type;
    uint32_t    inode_no;
    uint32_t    offset;
    int         refcount;
} file_t;

#define FILE_TABLE_SIZE 64

void    file_table_init(void);
file_t *file_alloc(void);
void    file_free(file_t *f);
