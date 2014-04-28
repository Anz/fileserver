/*
* Copyright (C) 2014 Roger Knecht
* License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
*/
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
   struct vfsn *root, *parent, *child, *sil_prev, *sil_next;
} vfsn_t;

/*
 * Opens given node.
 */
vfsn_t* vfs_open(vfsn_t *node);

/*
 * Creates node with given names as child of the parent node. If the parent is 
 * null, the node is the root node. Flags can be specified as followed:
 *    - VFS_FILE  Creates a file.
 *    - VFS_DIR   Creates a directory
 */
vfsn_t* vfs_create(vfsn_t *parent, char *name, char flags);

/*
 * Deletes given node. Memory of the node gets freed after the last user closes
 * handle via vfs_close. Node is still valid after this operations and must be
 * closed manually.
 */
void vfs_delete(vfsn_t *node);

/*
 * Moves node to a new parent with an different name.
 */
void vfs_move(vfsn_t *node, vfsn_t *newparent, char* name);

/*
 * Reads number of bytes specified by size or less from node into data. Returns
 * number of byte read.
 */
size_t vfs_read(vfsn_t *node, void *data, size_t size);

/*
 * Writes number of bytes specified by size into node from data. The current
 * value of the node gets overwritten.
 */
int vfs_write(vfsn_t *node, void *data, size_t size);

/*
 * Closes handle to node.
 */ 
void vfs_close(vfsn_t *node);

/*
 * Reads number of bytes specified by len or less of name of the node into str.
 */
void vfs_name(vfsn_t *node, char* str, size_t len);

/*
 * Returns the lenght of the filename.
 */
int vfs_name_size(vfsn_t *node);

/*
 * Returns 1 if the node is a file, otherwise 0.
 */
int vfs_is_file(vfsn_t *node);

/*
 * Returns 1 if the node is directory, otherwise 0.
 */
int vfs_is_dir(vfsn_t *node);

/*
 * Returns 1 if the node is marked as deleted, otherwise 1.
 */
int vfs_is_deleted(vfsn_t *node);

/*
 * Returns the number of bytes of node data.
 */
int vfs_size(vfsn_t *node);

/*
 * Changes node pointer from current to parent node. Only the given pointer must be closed manually-
 */
vfsn_t* vfs_parent(vfsn_t **node);

/*
 * Changes node pointer from current to first child node. Only the given pointer must be closed manually-
 */
vfsn_t* vfs_child(vfsn_t **node);

/*
 * Changes node pointer from current to previous silbling node. Only the given pointer must be closed manually-
 */
vfsn_t* vfs_prev(vfsn_t **node);

/*
 * Changes node pointer from current to next silbling node. Only the given pointer must be closed manually-
 */
vfsn_t* vfs_next(vfsn_t **node);

/*
 * Changes node pointer from current to root node. Only the given pointer must be closed manually-
 */
vfsn_t* vfs_root(vfsn_t **node);

#endif
