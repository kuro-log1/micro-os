#pragma once

typedef struct fs_node fs_node_t;

void        fs_init(void);
fs_node_t  *fs_cwd(void);
fs_node_t  *fs_find(fs_node_t *dir, const char *name);
fs_node_t  *fs_mkdir(fs_node_t *parent, const char *name);
fs_node_t  *fs_touch(fs_node_t *parent, const char *name);
int         fs_is_dir(fs_node_t *node);
void        fs_ls(fs_node_t *dir);
void        fs_pwd(void);
int         fs_cd(const char *name);
