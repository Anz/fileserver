#include "vfs.h"
#include "log.h"
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////////////
// DEFINES / MACROS
///////////////////////////////////////////////////////////////////////////////
#define VFS_READ  pthread_rwlock_rdlock
#define VFS_WRITE pthread_rwlock_wrlock

#define VFS_SAFE(lock_funtion, node, ...) \
   if (node) lock_funtion(&(node)->lock); \
   { __VA_ARGS__; } \
   if (node) pthread_rwlock_unlock(&(node)->lock);

#define VFS_SAFE_READ(node, ...) VFS_SAFE(VFS_READ, node, __VA_ARGS__)
#define VFS_SAFE_WRITE(node, ...) VFS_SAFE(VFS_WRITE, node, __VA_ARGS__)
#define VFS_SAFE2(lock, node1, node2, ...) VFS_SAFE(lock, node1, VFS_SAFE(lock, node2, __VA_ARGS__))
#define VFS_SAFE3(lock, node1, node2, node3, ...) VFS_SAFE(lock, node1, VFS_SAFE2(lock, node2, node3, __VA_ARGS__))
#define VFS_SAFE4(lock, node1, node2, node3, node4, ...) VFS_SAFE(lock, node1, VFS_SAFE3(lock, node2, node3, node4, __VA_ARGS__))

///////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
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

static int vfs_attach(vfsn_t *parent, vfsn_t *child)
{
   if (!child)
      return 1;

   if (!parent) {
      child->root = child;
      return 0;
   }

   int retval = 0;
   VFS_SAFE_WRITE(parent,
      child->root = parent->root;

      if (!parent->child) {
         child->sil_next = NULL;
         parent->child = child;
      } else {
         vfsn_t *it = vfs_open(parent->child);
         vfsn_t *prev = NULL;
         while (it) {
            if (prev) {
               pthread_rwlock_unlock(&prev->lock);
               vfs_close(prev);
            }
            prev = it;
            pthread_rwlock_wrlock(&it->lock);
            if (strcmp(it->name, child->name) == 0) {
               retval = 2;
            }
            it = vfs_open(it->sil_next);
         }

         if (retval == 0) {
            prev->sil_next = child;
            child->sil_prev = prev;
         }
         pthread_rwlock_unlock(&prev->lock);
         vfs_close(prev);
      }
      if (!retval) {
         child->parent = parent;
      }
   )
   return retval;
}

static void vfs_detach(vfsn_t* node)
{
   // open handles
   vfsn_t *prev = vfs_prev(node), *next = vfs_next(node), *parent = prev ? NULL : vfs_parent(node);
   // lock in order parent, prev, node, next to prevent dead locks!
   VFS_SAFE4(VFS_WRITE, parent, prev, node, next,
      node->root = NULL;

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
   
      node->parent = node->sil_prev = node->sil_next = NULL;
   );

   // close handles
   vfs_close(parent);
   vfs_close(prev);
   vfs_close(next);
}

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
vfsn_t* vfs_open(vfsn_t *node)
{
   if (!node)
      return NULL;

   VFS_SAFE_READ(node, log_dbg("Opening node '%s'", node->name));
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
      if (vfs_attach(parent, node)) {
         vfs_delete(node);
         vfs_close(node);
         return NULL;
      }
   }
   return node;
}

void vfs_delete(vfsn_t *node)
{
   if (!node)
      return;

   VFS_SAFE_READ(node, log_dbg("Delete node '%s'", node->name));
   vfs_flag_set(node, VFS_DEL);
   vfsn_t *it = vfs_child(node);
   vfsn_t *prev = NULL;
   while (it != NULL) {
      vfs_delete(prev);
      vfs_close(prev);
      prev = it;
      it = vfs_next(it);
   }
   vfs_delete(prev);
   vfs_close(prev);
   vfs_detach(node);
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

   VFS_SAFE_READ(node, log_dbg("Closing node '%s'", node->name));
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

int vfs_name_size(vfsn_t *node)
{
   int size;
   VFS_SAFE_READ(node, size = strlen(node->name));
   return size;
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

int vfs_size(vfsn_t *node)
{
   int size;
   VFS_SAFE_READ(node, size = node->data_size);
   return size;
}

vfsn_t* vfs_parent(vfsn_t *node) 
{
   if (!node)
      return NULL;

   vfsn_t* parent = NULL;
   VFS_SAFE_READ(node, parent = vfs_open(node->parent))
   return parent;
}

vfsn_t* vfs_child(vfsn_t *node)
{
   if (!node)
      return NULL;

   vfsn_t* child = NULL;
   VFS_SAFE_READ(node, child = vfs_open(node->child))
   return child;
}

vfsn_t* vfs_prev(vfsn_t *node)
{
   if (!node)
      return NULL;

   vfsn_t* prev = NULL;
   VFS_SAFE_READ(node, prev = vfs_open(node->sil_prev))
   return prev;
}

vfsn_t* vfs_next(vfsn_t *node)
{
   if (!node)
      return NULL;

   vfsn_t* next = NULL;
   VFS_SAFE_READ(node, next = vfs_open(node->sil_next))
   return next;
}

void vfs_parent2(vfsn_t **node)
{
   if (!node || !*node)
      return;

   vfsn_t *current = *node;
   VFS_SAFE_READ(current, *node = vfs_open(current->parent));
   vfs_close(current);
}

void vfs_child2(vfsn_t **node)
{
   if (!node || !*node)
      return;

   vfsn_t *current = *node;
   VFS_SAFE_READ(current, *node = vfs_open(current->child));
   vfs_close(current);
}

void vfs_prev2(vfsn_t **node)
{
   if (!node || !*node)
      return;

   vfsn_t *current = *node;
   VFS_SAFE_READ(current, *node = vfs_open(current->sil_prev));
   vfs_close(current);
}

void vfs_next2(vfsn_t **node)
{
   if (!node || !*node)
      return;

   vfsn_t *current = *node;
   VFS_SAFE_READ(current, *node = vfs_open(current->sil_next));
   vfs_close(current);
}

void vfs_root(vfsn_t **node)
{
   if (!node || !*node)
      return;

   vfsn_t *current = *node;
   VFS_SAFE_READ(current, *node = vfs_open(current->root));
   vfs_close(current);
}
