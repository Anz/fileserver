#include "vts.h"
#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>


static void* vtp_worker(void* data)
{
   struct vtp_worker *worker = (struct vtp_worker*)data;

   // handle virtual transfer protocol
   vtp_handle(worker->fd, worker->root);

   // exit worker thread
   return NULL;
}

int vts_init(vtp_socket_t *sock, int port, int max_clients)
{
   // init socket
   memset(sock, 0, sizeof(*sock));
   sock->workers = calloc(sizeof(struct vtp_worker), max_clients);
   sock->max_clients = max_clients;

   // check memory allocation
   if (!sock->workers) {
      return 1;
   }

   // setup socket
   sock->sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sock->sockfd < 0) {
      vts_release(sock);
      return 1;
   }

   // set reuseable address
   int optvalue = 1;
   setsockopt(sock->sockfd, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

   // setup socket address
   struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = INADDR_ANY,
      .sin_port = htons(port)
   };

   // bind socket to address
   if (bind(sock->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
      vts_release(sock);
      return 1;
   }

   // start listening
   if (listen(sock->sockfd, 5)) {
      vts_release(sock);
      return 1;
   }

   // create filesystem
   sock->root = vfs_create(NULL, "/", VFS_DIR);
   if (!sock->root) {
      vts_release(sock);
      return 1;
   }

   return 0;
}

void vts_release(vtp_socket_t *sock)
{
   // close server socket
   close(sock->sockfd);

   // release memory
   free(sock->workers);

   // delete filesystem
   vfs_delete(sock->root);
   vfs_close(sock->root);

   // clear memory
   memset(sock, 0, sizeof(*sock));
}

int vtp_start(vtp_socket_t* sock)
{ 
   int thread_num = 0;

   // server loop until filesystem gets deleted
   while (1) {
      // wait for clients
      int clientfd = accept(sock->sockfd, NULL, 0);
      
      // server socket closed, shutdown server
      if (clientfd < 0) {
         break;
      }

      // set client data
      struct vtp_worker *worker = &sock->workers[thread_num];
      worker->fd = clientfd;
      worker->root = vfs_open(sock->root);
      pthread_create(&sock->workers[thread_num].thread, NULL, vtp_worker, worker);
      thread_num++;
   }

   // wait for all threads to finish
   for (int i = 0; i < thread_num; i++) {
      pthread_join(sock->workers[i].thread, NULL);
   }

   return 0;
}

void vtp_stop(vtp_socket_t* sock)
{
   if (!sock)
      return;

   // close all sockets
   close(sock->sockfd);
   for (int i = 0; i < sock->max_clients; i++) {
      close(sock->workers[i].fd);
   }
}
