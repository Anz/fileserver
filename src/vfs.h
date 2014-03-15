#ifndef VFS
#define VFS

typedef struct node {
   char flags;
   char name[256];
   char data[1024];
   size_t data_size;
   struct node *parent
   struct node *child;
   struct node *sil_prev;
   struct node *sil_next;
} node_t;

void vfs_file(node_t *node, char *name);
void vfs_dir(node_t *node, char *name);
void vfs_delete(node_t *node);
void vfs_open(node_t *node);
void vfs_close(node_t *node);
void vfs_lock(node_t *node);
void vfs_unlock(node_t *node);

#endif
