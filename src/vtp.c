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

static pthread_mutex_t runlock = PTHREAD_MUTEX_INITIALIZER;
static int running = 1;

struct vtpw {
   int clientfd;
   vfsn_t *root;
};

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
   struct vtpw* vtpw = (struct vtpw*)data;
   vfsn_t *root = vtpw->root;
   int sockfd = vtpw->clientfd;
   free(data);

   char buf[512];
   memset(buf, 0, 512);

   // send welcome
   write(sockfd, WELCOME_TEXT, strlen(WELCOME_TEXT));

   while (fcntl(sockfd, F_GETFL) != -1) {
      write(sockfd, LINE_START, strlen(LINE_START));
      int len;
      while ((len = read(sockfd, buf,512)) <= 0) {
         // check condition
         pthread_mutex_lock(&runlock);
         if (!running) {
            pthread_mutex_unlock(&runlock);
            write(sockfd, "bye\n", 4);
            syncfs(sockfd);
            close(sockfd);
            return NULL;
         }
         pthread_mutex_unlock(&runlock);
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

      // set timeout
      setsockopt(data->clientfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(struct timeval));

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
   pthread_mutex_lock(&runlock);
   running = 0;
   pthread_mutex_unlock(&runlock);
}
