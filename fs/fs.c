#include "fs/fs.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/printf.h"

#define NAME_MAX     32
#define CHILDREN_MAX 16

typedef enum { FS_DIR, FS_FILE } fs_type_t;

struct fs_node {
    char       name[NAME_MAX];
    fs_type_t  type;
    fs_node_t *parent;
    fs_node_t *children[CHILDREN_MAX];
    int        nchildren;
};

static fs_node_t *root;
static fs_node_t *cwd;

static fs_node_t *alloc_node(fs_node_t *parent, const char *name, fs_type_t type) {
    fs_node_t *n = kmalloc(sizeof(fs_node_t));
    int i = 0;
    while (name[i] && i < NAME_MAX - 1) { n->name[i] = name[i]; i++; }
    n->name[i] = '\0';
    n->type      = type;
    n->parent    = parent;
    n->nchildren = 0;
    for (int j = 0; j < CHILDREN_MAX; j++) n->children[j] = NULL;
    return n;
}

void fs_init(void) {
    root = alloc_node(NULL, "/", FS_DIR);
    root->parent = root;
    cwd = root;
}

fs_node_t *fs_cwd(void) { return cwd; }

fs_node_t *fs_find(fs_node_t *dir, const char *name) {
    for (int i = 0; i < dir->nchildren; i++)
        if (strcmp(dir->children[i]->name, name) == 0)
            return dir->children[i];
    return NULL;
}

fs_node_t *fs_mkdir(fs_node_t *parent, const char *name) {
    if (parent->nchildren >= CHILDREN_MAX) { printf("directory full\n"); return NULL; }
    if (fs_find(parent, name))             { printf("already exists\n"); return NULL; }
    fs_node_t *n = alloc_node(parent, name, FS_DIR);
    parent->children[parent->nchildren++] = n;
    return n;
}

fs_node_t *fs_touch(fs_node_t *parent, const char *name) {
    if (parent->nchildren >= CHILDREN_MAX) { printf("directory full\n"); return NULL; }
    if (fs_find(parent, name))             { printf("already exists\n"); return NULL; }
    fs_node_t *n = alloc_node(parent, name, FS_FILE);
    parent->children[parent->nchildren++] = n;
    return n;
}

int fs_is_dir(fs_node_t *node) { return node->type == FS_DIR; }

void fs_ls(fs_node_t *dir) {
    if (dir->nchildren == 0) return;
    for (int i = 0; i < dir->nchildren; i++) {
        fs_node_t *c = dir->children[i];
        if (c->type == FS_DIR)
            printf("%s/\n", c->name);
        else
            printf("%s\n", c->name);
    }
}

static void print_path(fs_node_t *node) {
    if (node == root) { printf("/"); return; }
    print_path(node->parent);
    if (node->parent != root) printf("/");
    printf("%s", node->name);
}

void fs_pwd(void) {
    print_path(cwd);
    printf("\n");
}

int fs_cd(const char *name) {
    if (strcmp(name, "..") == 0) {
        cwd = cwd->parent;
        return 0;
    }
    fs_node_t *n = fs_find(cwd, name);
    if (!n)              { printf("no such directory\n"); return -1; }
    if (!fs_is_dir(n))   { printf("not a directory\n");  return -1; }
    cwd = n;
    return 0;
}
