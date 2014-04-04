#ifndef VTS
#define VTS

#include "vfs.h"
#include <pthread.h>

#define VTS_SOCKET_INIT 0

struct vts_worker {
   pthread_mutex_t lock;
   pthread_t thread;
   int fd;
   vfsn_t *root;
};

typedef struct {
   int sockfd;
   int max_clients;
   struct vts_worker *workers;
   vfsn_t *root;
} vts_socket_t;

/*
 * Inits vts socket. Must be called before vtp_start.
 */
int vts_init(vts_socket_t *sock, int port, int max_clients);

/*
 * Releases vts socket.
 */
void vts_release(vts_socket_t *sock);


/*
 * Handles client connection and blocks until vts_stop gets called.
 */
int vts_start(vts_socket_t *sock);

/*
 * Stops given socket.
 */
void vts_stop(vts_socket_t *sock);

#endif
