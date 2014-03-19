#include "vfs.h"
#include <stdlib.h>

#define VFS_SAFE_WRITE(node, ...) \
   if (node) pthread_rwlock_wrlock(&node->lock); \
   { __VA_ARGS__; } \
   if (node) pthread_rwlock_unlock(&node->lock); 

#define VFS_SAFE_WRITE2(node1, node2, ...) VFS_SAFE_WRITE(node1, VFS_SAFE_WRITE(node2, __VA_ARGS__))
#define VFS_SAFE_WRITE3(node1, node2, node3, ...) VFS_SAFE_WRITE(node1, VFS_SAFE_WRITE2(node2, node3, __VA_ARGS__))
#define VFS_SAFE_WRITE4(node1, node2, node3, node4, ...) VFS_SAFE_WRITE(node1, VFS_SAFE_WRITE3(node2, node3, node4, __VA_ARGS__))

#define VFS_SAFE_READ(node, ...) \
   if (node) { \
      pthread_rwlock_rdlock(&node->lock); \
      { __VA_ARGS__; } \
      pthread_rwlock_unlock(&node->lock); \
   }


static int vfs_flag_checked(vfsn_t *node, char flag)
{
   int retval;
   VFS_SAFE_READ(node, retval = (node->flags & flag) ? 1 : 0);
   return retval;
}

static void vfs_flag_set(vfsn_t *node, char flag)
{
   VFS_SAFE_WRITE(node, node->flags |= flag);
}

static void vfs_attach(vfsn_t *parent, vfsn_t *child)
{
   if (!parent || !child)
      return;

   VFS_SAFE_WRITE(parent,
      child->sil_next = parent->child;
      parent->child = child;
   )
}

static void vfs_detach(vfsn_t* node)
{
   // open handles
   vfsn_t *prev = vfs_prev(node), *next = vfs_next(node), *parent = prev ? NULL : vfs_parent(node);

   // lock in order parent, prev, node, next to prevent dead locks!
   VFS_SAFE_WRITE4(parent, prev, node, next,
      // link prev or parent to next
      if (prev) {
         prev->sil_next = next;
      } else if (parent) {
         parent->child = next;
      }
      
      // link next to prev
      if (next) {
         next->sil_prev = prev;
      }

      // unset node references
      node->parent = node->sil_prev = node->sil_next = NULL;
   );

   // close handles
   vfs_close(parent);
   vfs_close(prev);
   vfs_close(next);
}

vfsn_t* vfs_open(vfsn_t *node)
{
   if (!node)
      return NULL;
   pthread_rwlock_rdlock(&node->openlk);
   return node;
}

vfsn_t* vfs_create(vfsn_t *parent, char* name, char flags)
{
   vfsn_t *node = malloc(sizeof(vfsn_t));
   if (node) {
      memset(node, 0, sizeof(vfsn_t));
      pthread_rwlock_init(&node->openlk, NULL);
      pthread_rwlock_init(&node->lock, NULL);
      node->name = strdup(name);
      vfs_flag_set(node, flags); 
      vfs_open(node);
      vfs_attach(parent, node);
   }
   return node;
}

void vfs_delete(vfsn_t *node)
{
   vfs_detach(node);
   vfs_flag_set(node, VFS_DEL);
   VFS_SAFE_READ(node,
      vfsn_t *it = vfs_child(node);
      vfsn_t *prev = it;
      while (it != NULL) {
         it = vfs_next(it);
         vfs_delete(prev);
         vfs_close(prev);
         prev = it;
      }
      vfs_close(prev);
   )
}

size_t vfs_read(vfsn_t *node, void *data, size_t size) {
   size_t read = 0;
   VFS_SAFE_READ(node,
      if (node->flags & VFS_FILE) {
         read = (size < node->data_size) ? size : node->data_size;
         memcpy(data, node->data, read);
      }
   );
   return read;
}

int vfs_write(vfsn_t *node, void *data, size_t size) {
   int retval = 1;
   VFS_SAFE_WRITE(node,
      if (node->flags & VFS_FILE) {
         free(node->data);
         node->data = malloc(size);
         if (node->data) {
            memcpy(node->data, data, size);
            node->data_size = size;
            retval = 0;
         } else {
            retval = 2;
         }
         
      }
   );
   return retval;
}

void vfs_close(vfsn_t *node)
{
   if (!node)
      return;

   int deleted = vfs_flag_checked(node, VFS_DEL);
   pthread_rwlock_unlock(&node->openlk);

   if (deleted && pthread_rwlock_trywrlock(&node->openlk) == 0) {
      pthread_rwlock_destroy(&node->openlk);
      pthread_rwlock_destroy(&node->lock);
      free(node->name);
      free(node->data);
      free(node);
   }
}

void vfs_name(vfsn_t *node, char* str, size_t len)
{
   VFS_SAFE_READ(node, strncpy(str, node->name, len));
}

int vfs_is_file(vfsn_t* node)
{
   return vfs_flag_checked(node, VFS_FILE);
}

int vfs_is_dir(vfsn_t* node)
{
   return !vfs_is_file(node);
}

int vfs_is_deleted(vfsn_t *node) {
   return vfs_flag_checked(node, VFS_DEL);
}

vfsn_t* vfs_parent(vfsn_t *node) 
{
   vfsn_t* parent = NULL;
   VFS_SAFE_READ(node, parent = vfs_open(node->parent))
   return parent;
}

vfsn_t* vfs_child(vfsn_t *node)
{
   vfsn_t* child = NULL;
   VFS_SAFE_READ(node, child = vfs_open(node->child))
   return child;
}

vfsn_t* vfs_prev(vfsn_t *node)
{
   vfsn_t* prev = NULL;
   VFS_SAFE_READ(node, prev = vfs_open(node->sil_prev))
   return prev;
}

vfsn_t* vfs_next(vfsn_t *node)
{
   vfsn_t* next = NULL;
   VFS_SAFE_READ(node, next = vfs_open(node->sil_next))
   return next;
}

