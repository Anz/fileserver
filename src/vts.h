#ifndef VTS
#define VTS

#include "vfs.h"
#include <pthread.h>

#define VTS_SOCKET_INIT 0

struct vtp_worker {
   pthread_t thread;
   int fd;
   vfsn_t *root;
};

typedef struct {
   int sockfd;
   int max_clients;
   struct vtp_worker *workers;
   vfsn_t *root;
} vtp_socket_t;

/*
 * Inits vts socket. Must be called before vtp_start.
 */
int vts_init(vtp_socket_t *sock, int port, int max_clients);

/*
 * Releases vts socket.
 */
void vts_release(vtp_socket_t *sock);


/*
 * Handles client connection and blocks until vts_stop gets called.
 */
int vtp_start(vtp_socket_t *sock);

/*
 * Stops given socket.
 */
void vtp_stop(vtp_socket_t *sock);

#endif
