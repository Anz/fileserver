#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

struct vtpw {
   int clientfd;
   vfsn_t *root;
};

static void* vtp_worker(void* data)
{
   struct vtpw* vtpw = (struct vtpw*)data;

   write(vtpw->clientfd, "hello client", 13);
   close(vtpw->clientfd);
   return NULL;
}

int vtp_start(vtps_t *vtps, int port)
{
   // setup socket
   vtps->sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (vtps->sockfd < 0) {
      return 1;
   }
   
   // setup socket address
   struct sockaddr_in serv_addr;
   memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(port);

   // bind socket to address
   if (bind(vtps->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      return 2;
   }

   // start listening
   if (listen(vtps->sockfd, 5)) {
      return 3;
   }

   // wait for clients
   struct sockaddr_in client_addr;
   socklen_t len = sizeof(client_addr);

   while (1) {
      struct vtpw* data = malloc(sizeof(struct vtpw));
      memset(data, 0, sizeof(struct vtpw));
      data->root = vtps->root;
      data->clientfd = accept(vtps->sockfd, (struct sockaddr*)&client_addr, &len);
      if (data->clientfd < 0) {
         free(data);
         return 4;
      }

      // create thread
      pthread_t thread;
      pthread_create(&thread, NULL, vtp_worker, data);  
   }

   // close socket
   close(vtps->sockfd);
}

void vtp_stop(vtps_t *socket)
{
}
