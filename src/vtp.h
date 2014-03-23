#ifndef VTP
#define VTP

#include "vfs.h"
#include <pthread.h>

typedef struct vtps {
   pthread_rwlock_t openlk;
   pthread_mutex_t runlock;
   int running;
   int sockfd;
   vfsn_t* root;
} vtps_t;

typedef struct vtps* vtp_socket_t;

int vtp_start(vtp_socket_t *sock, int port);
void vtp_stop(vtp_socket_t *sock);

#endif
