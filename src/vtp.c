#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>

#define WELCOME_TEXT "hello client and welcome to multithreading fileserver\n"
#define LINE_START "> "
#define NOSUCHFILE "NOSUCHFILE\n"
#define MSG_NOSUCHCMD "NOSUCHCMD\n"

struct vtp_cmd {
   char* name;
   void (*func)(int fd);
};

static struct timeval timeout = {
   .tv_sec = 3,
   .tv_usec = 0,
};


static void vtp_list(int fd)
{
   write(fd, "list\n", 5);
}

static void vtp_cmd_exit(int fd) {
   close(fd);
}

static struct vtp_cmd cmds[] = {
   { "list", vtp_list },
   { "create", vtp_list },
   { "exit", vtp_cmd_exit },
   { }
};

static void* vtp_worker(void* data)
{
   struct vtp_thread* thread = (struct vtp_thread*)data;
   vfsn_t *root = thread->root;
   int sockfd = thread->sockfd;

   char buf[512];
   memset(buf, 0, 512);

   // send welcome
   write(sockfd, WELCOME_TEXT, strlen(WELCOME_TEXT));

   while (fcntl(sockfd, F_GETFL) != -1) {
      write(sockfd, LINE_START, strlen(LINE_START));
      int len;
      while ((len = read(sockfd, buf,512)) <= 0) {
         // check condition
         pthread_mutex_lock(&thread->socket->lock);
         if (!thread->socket->running) {
            pthread_mutex_unlock(&thread->socket->lock);
            close(sockfd);
            return NULL;
         }
         pthread_mutex_unlock(&thread->socket->lock);
      }
      buf[len-2] = '\0';
      struct vtp_cmd *cmd;
      for (cmd = &cmds[0]; cmd->name; cmd++) {
         if (strncmp(cmd->name, buf, len) == 0) {
            cmd->func(sockfd);
            break;
         }
      }
      if (cmd->name == NULL) {
         write(sockfd, MSG_NOSUCHCMD, strlen(MSG_NOSUCHCMD));
      }
   }

   return NULL;
}

int vtp_start(vtps_t *vtps, int port)
{
   // init structure
   memset(vtps, 0, sizeof(*vtps));
   pthread_mutex_init(&vtps->lock, NULL);
   vtps->running = 1;

   // setup socket
   vtps->sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (vtps->sockfd < 0) {
      return 1;
   }
   setsockopt(vtps->sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval));
   
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

   int running = vtps->running;

   while (running) {
      // wait for clients
      int clientfd = accept(vtps->sockfd, (struct sockaddr*)&client_addr, &len);
      if (clientfd < 0) {
         continue;
      }

      // set timeout time
      setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval));

      // alloc client structure
      struct vtp_thread *thread = malloc(sizeof(struct vtp_thread));
      if (!thread) {
         close(clientfd);
         continue;
      }

      // set client data
      memset(thread, 0, sizeof(*thread));
      thread->socket = vtps;
      thread->sockfd = clientfd;
      thread->root = vfs_open(vtps->root);
      thread->cwd = vfs_open(vtps->root);
      pthread_mutex_lock(&vtps->lock);
      thread->next = vtps->thread;
      vtps->thread = thread;
      pthread_mutex_unlock(&vtps->lock);
      pthread_create(&thread->thread, NULL, vtp_worker, thread);

      // update running var
      pthread_mutex_lock(&vtps->lock);
      running = vtps->running;
      pthread_mutex_unlock(&vtps->lock);
   }

   // cleanup
   close(vtps->sockfd);
   /*struct vtp_thread *prev = NULL, *it = vtps->thread;
   while (it) {
      pthread_join(it->thread, NULL);
      free(prev);
      prev = it;
      it = it->next;
   }*/

   pthread_mutex_destroy(&vtps->lock);
   return 0;
}

void vtp_stop(vtps_t *socket)
{
   pthread_mutex_lock(&socket->lock);
   socket->running = 0;
   pthread_mutex_unlock(&socket->lock);
}
