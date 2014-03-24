#include "vts.h"
#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>


struct vtp_worker {
   int fd;
   vfsn_t *root;
};

static void* vtp_worker(void* data)
{
   struct vtp_worker *worker = (struct vtp_worker*)data;
   int sockfd = worker->fd;
   vfsn_t *root = worker->root;
   free(data);
   
   // set timeout time
   struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
   setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval));

   vtp_handle(sockfd, root);
   return NULL;
}

static int vtp_socket(int port)
{
   // setup socket
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
      return 0;
   }

   // setup socket address
   struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = INADDR_ANY,
      .sin_port = htons(port)
   };

   // bind socket to address
   if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      return 0;
   }

   // start listening
   if (listen(sockfd, 5)) {
      return 0;
   }

   return sockfd;
}

int vtp_start(vtp_socket_t* sock, int port)
{
   int sockfd = vtp_socket(port);
   if (sockfd < 0)
      return 1;

    vfsn_t *root = vfs_create(NULL, "hello", VFS_DIR);
   if (!root) {
      close(sockfd);
      return 1;
   }
   *sock = root;

   // wait for clients
   struct sockaddr_in client_addr;
   socklen_t len = sizeof(client_addr);

   fd_set set;

   while (!vfs_is_deleted(root)) {
      FD_ZERO(&set);
      FD_SET(sockfd, &set);
      struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
      if (select(sockfd+1, &set, (fd_set*)0, (fd_set*)0, &timeout) <= 0) {
         continue;
      }

      // wait for clients
      int clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &len);
      if (clientfd < 0) {
         continue;
      }

      struct vtp_worker *worker = malloc(sizeof(struct vtp_worker));
      if (!worker) {
         close(clientfd);
         continue;
      }

      // set client data
      worker->fd = clientfd;
      worker->root = vfs_open(root);
      pthread_t thread;
      pthread_create(&thread, NULL, vtp_worker, worker);
   }

   // release socket
   close(sockfd);
   vfs_close(root);
   return 0;
}

void vtp_stop(vtp_socket_t* sock)
{
   if (!sock || !*sock)
      return;

   vfsn_t *root = *sock;
   vfs_delete(root);
}
