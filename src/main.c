#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "vfs.h"
#include "vtp.h"


static vtp_socket_t socket = NULL;

void signal_handler(int signal)
{
   printf("shutdown server\n");
   vtp_stop(&socket);
}

int main(int argc, char* argv[]) {
   struct sigaction sighandler;
   sighandler.sa_handler = signal_handler;
   sigemptyset(&sighandler.sa_mask);
   sighandler.sa_flags= 0;
   sigaction(SIGINT, &sighandler, NULL);

   /*vfsn_t *root;
   char buf[255];

   root = vfs_create(NULL, "hello", VFS_DIR);
   if (!root) {
      printf("root already exits\n");
      return 1;
   }
   for (char c = 'a'; c < 'f'; c++) {
      char name[2];
      memset(name, 0, sizeof(name));
      name[0] = c;
      vfsn_t *node = vfs_create(root, name, VFS_FILE);
      if (!node) {
         printf("node with name %s already exists\n", name);
         continue;
      }
      char *text = "hello world!";
      vfs_write(node, text, strlen(text)+1);
      vfs_close(node);
   }
   vfs_close(vfs_create(root, "dir", VFS_DIR));
   if (vfs_create(root, "dir", VFS_DIR)) {
      printf("error node could be created\n");
   } else {
      printf("node exits\n");
   }

   vfs_name(root, buf, 255);
   printf("root: %s\n", buf);
   vfsn_t *child = vfs_child(root);
   vfsn_t *last = child;
   while (child) {
      char buffer[1024];
      memset(buffer, 0, sizeof(buffer));
      vfs_read(child, buffer, sizeof(buffer));
      vfs_name(child, buf, 255);
      printf("child: %s %i\n%s\n\n", buf, vfs_is_file(child), buffer);
      child = vfs_next(child);
      vfs_close(last);
      last = child;
   }

   vfs_delete(root);
   vfs_close(root);*/

   // start socket
   int result = vtp_start(&socket, 8000);
   printf("socket returned %i\n",  result);

   return 0;
}
