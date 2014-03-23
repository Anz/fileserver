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

struct vtp_worker {
   int fd;
   vtps_t *socket;
};

struct vtp_cmd {
   char* name;
   void (*func)(int fd);
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

static void vtp_release(vtps_t *socket)
{
   pthread_rwlock_wrlock(&socket->openlk);
   pthread_mutex_destroy(&socket->runlock);
   pthread_rwlock_destroy(&socket->openlk);
   vfs_delete(socket->root);
   vfs_close(socket->root);
   free(socket);
}

static vtps_t* vtp_init(int port)
{
   vtps_t* vtps = malloc(sizeof(vtps_t));
   if (!vtps)
      return NULL;
   memset(vtps, 0, sizeof(*vtps));

   // setup virtual file system
   vtps->root = vfs_create(NULL, "hello", VFS_DIR);
   if (!vtps->root) {
      vtp_release(vtps);
      return NULL;
   }
 
   // init socket lock
   if (pthread_rwlock_init(&vtps->openlk, NULL)) {
      vtp_release(vtps);
      return NULL;
   }

   // init socket lock
   if (pthread_mutex_init(&vtps->runlock, NULL)) {
      vtp_release(vtps);
      return NULL;
   }

   // setup socket
   vtps->sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (vtps->sockfd < 0) {
      vtp_release(vtps);
      return NULL;
   }

   // setup socket address
   struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = INADDR_ANY,
      .sin_port = htons(port)
   };

   // bind socket to address
   if (bind(vtps->sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      vtp_release(vtps);
      return NULL;
   }

   // start listening
   if (listen(vtps->sockfd, 5)) {
      vtp_release(vtps);
      return NULL;
   }

   // set running
   vtps->running = 1;

   return vtps;
}

static int vtp_is_running(vtps_t *socket)
{
   int running;
   pthread_mutex_lock(&socket->runlock);
   running = socket->running;
   pthread_mutex_unlock(&socket->runlock);
   return running;
}

static void* vtp_worker(void* data)
{
   struct vtp_worker *worker = (struct vtp_worker*)data;
   int sockfd = worker->fd;
   vtps_t *socket = worker->socket;
   worker = NULL;
   free(data);
   
   pthread_rwlock_rdlock(&socket->openlk);
   vfsn_t *root = vfs_open(socket->root);

   // set timeout time
   struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
   setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(struct timeval));

   char buf[512];
   memset(buf, 0, 512);

   // send welcome
   write(sockfd, WELCOME_TEXT, strlen(WELCOME_TEXT));

   while (fcntl(sockfd, F_GETFL) != -1) {
      write(sockfd, LINE_START, strlen(LINE_START));
      int len;
      while ((len = read(sockfd, buf,512)) <= 0) {
         if (!vtp_is_running(socket)) {
            close(sockfd);
            return NULL;
         }
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

   vfs_close(root);
   pthread_rwlock_unlock(&socket->openlk);
   return NULL;
}

int vtp_start(vtp_socket_t* sock, int port)
{
   vtps_t* vtps = vtp_init(port);
   if (!vtps) 
      return 1;
   *sock = vtps;

   // wait for clients
   struct sockaddr_in client_addr;
   socklen_t len = sizeof(client_addr);

   fd_set set;

   while (vtp_is_running(vtps)) {
      FD_ZERO(&set);
      FD_SET(vtps->sockfd, &set);
      struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
      if (select(vtps->sockfd+1, &set, (fd_set*)0, (fd_set*)0, &timeout) <= 0) {
         continue;
      }

      // wait for clients
      int clientfd = accept(vtps->sockfd, (struct sockaddr*)&client_addr, &len);
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
      worker->socket = vtps;
      pthread_t thread;
      pthread_create(&thread, NULL, vtp_worker, worker);
   }

   // release socket
   close(vtps->sockfd);
   vtp_release(vtps);
   return 0;
}

void vtp_stop(vtp_socket_t* sock)
{
   if (!sock || !*sock)
      return;

   vtps_t *socket = *sock;
   pthread_mutex_lock(&socket->runlock);
   socket->running = 0;
   pthread_mutex_unlock(&socket->runlock);
}
