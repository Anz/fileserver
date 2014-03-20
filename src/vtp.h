#ifndef VTP
#define VTP

#include "vfs.h"
#include <pthread.h>

struct vtps;

struct vtp_thread {
   struct vtps *socket;
   int sockfd;
   vfsn_t *root;
   vfsn_t *cwd;
   pthread_t thread;
   struct vtp_thread *next;
};

typedef struct vtps {
   pthread_mutex_t lock;
   int running;
   int sockfd;
   vfsn_t* root;
   struct vtp_thread *thread;
} vtps_t;

int vtp_start(vtps_t *vtps, int port);
void vtp_stop(vtps_t *vtps);

#endif
