#include <stdlib.h>
#include <stdio.h>

#include "vfs.h"

int main(int argc, char* argv[]) {
   vfsn_t *root;
   char buf[255];

   root = vfs_create(NULL, "hello", VFS_OPEN|VFS_DIR);
   for (char c = 'a'; c < 'f'; c++) {
      char name[2];
      memset(name, 0, sizeof(name));
      name[0] = c;
      vfs_create(root, name, VFS_FILE);
   }
   vfs_create(root, "dir", VFS_DIR);

   vfs_name(root, buf, 255);
   printf("root: %s\n", buf);
   vfsn_t *child = vfs_child(root);
   vfsn_t *last = child;
   while (child) {
      vfs_name(child, buf, 255);
      printf("child: %s %i\n", buf, vfs_file(child));
      child = vfs_next(child);
      vfs_close(last);
      last = child;
   }

   vfs_delete(root);
   vfs_close(root);

   return 0;
}
