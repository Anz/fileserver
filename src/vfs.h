#ifndef VFS
#define VFS

#include <string.h>
#include <pthread.h>

#define VFS_DEL   0x80
#define VFS_FILE  0x01
#define VFS_DIR   0x02

typedef struct vfsn {
   pthread_rwlock_t openlk, lock;
   char *name, flags;
   void *data;
   size_t data_size;
   struct vfsn *parent, *child, *sil_prev, *sil_next;
} vfsn_t;

vfsn_t* vfs_open(vfsn_t *node);
vfsn_t* vfs_create(vfsn_t *parent, char *name, char flags);
void vfs_delete(vfsn_t *node);
size_t vfs_read(vfsn_t *node, void *data, size_t size);
int vfs_write(vfsn_t *node, void *data, size_t size);
void vfs_close(vfsn_t *node);
void vfs_name(vfsn_t *node, char* str, size_t len);
int vfs_is_file(vfsn_t *node);
int vfs_is_dir(vfsn_t *node);
int vfs_is_deleted(vfsn_t *node);
int vfs_size(vfsn_t *node);
vfsn_t* vfs_parent(vfsn_t *node);
vfsn_t* vfs_child(vfsn_t *node);
vfsn_t* vfs_prev(vfsn_t *node);
vfsn_t* vfs_next(vfsn_t *node);

#endif
