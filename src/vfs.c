#include "vfs.h"
#include <stdlib.h>

#define VFS_SAFE_WRITE(node, ...) \
   if (node) { \
      pthread_rwlock_wrlock(&node->lock); \
      { __VA_ARGS__; } \
      pthread_rwlock_unlock(&node->lock); \
   }

#define VFS_SAFE_READ(node, ...) \
   if (node) { \
      pthread_rwlock_rdlock(&node->lock); \
      { __VA_ARGS__; } \
      pthread_rwlock_unlock(&node->lock); \
   }


static vfsn_t* vfs_open(vfsn_t *node)
{
   if (!node)
      return NULL;
   pthread_rwlock_rdlock(&node->openlk);
   return node;
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
   vfsn_t *parent = vfs_parent(node);
   vfsn_t *prev = vfs_prev(node);
   vfsn_t *next = vfs_next(node);

   if (prev && next) {
      VFS_SAFE_WRITE(prev,
         VFS_SAFE_WRITE(node, 
            VFS_SAFE_WRITE(next,
               prev->sil_next = next;
               next->sil_prev = prev;
               node->parent = node->sil_prev = node->sil_next = NULL;
            )
         )
      )
   } else if (prev) {
      VFS_SAFE_WRITE(prev,
         VFS_SAFE_WRITE(node, 
            prev->sil_next = NULL;
            node->parent = node->sil_prev = node->sil_next = NULL;
         )
      )
   } else if (next && parent) {
      VFS_SAFE_WRITE(parent,
         VFS_SAFE_WRITE(node, 
            VFS_SAFE_WRITE(next,
               parent->child = next;
               next->sil_prev = NULL;
               node->parent = node->sil_prev = node->sil_next = NULL;
             )
         )
      )
   } else if (next) {
      VFS_SAFE_WRITE(node, 
         VFS_SAFE_WRITE(next,
            next->sil_prev = NULL;
            node->parent = node->sil_prev = node->sil_next = NULL;
         )
      )
   } else if (parent) {
      VFS_SAFE_WRITE(parent, 
         parent->child = NULL;
         node->parent = node->sil_prev = node->sil_next = NULL;
      );
   }

   // close handles
   vfs_close(parent);
   vfs_close(prev);
   vfs_close(next);
}

vfsn_t* vfs_create(vfsn_t *parent, char* name, char flags)
{
   vfsn_t *node = malloc(sizeof(vfsn_t));
   if (node) {
      memset(node, 0, sizeof(vfsn_t));
      pthread_rwlock_init(&node->openlk, NULL);
      pthread_rwlock_init(&node->lock, NULL);
      node->name = strdup(name);
      node->flags |= flags;
      
      if (flags & VFS_OPEN)
         vfs_open(node);
      vfs_attach(parent, node);
   }
   return node;
}

void vfs_delete(vfsn_t *node)
{
   vfs_detach(node);
   VFS_SAFE_WRITE(node, node->flags |= VFS_DEL);
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

void vfs_close(vfsn_t *node)
{
   if (!node)
      return;

   int deleted;
   VFS_SAFE_READ(node, deleted = node->flags & VFS_DEL);
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

int vfs_file(vfsn_t* node)
{
   int retval;
   VFS_SAFE_READ(node, retval = node->flags & VFS_FILE);
   return retval;
}

int vfs_dir(vfsn_t* node)
{
   return !vfs_file(node);
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

