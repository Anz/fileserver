#ifndef VTS
#define VTS

#include "vfs.h"
#include <pthread.h>

#define VTS_SOCKET_INIT 0

typedef vfsn_t* vtp_socket_t;

int vtp_start(vtp_socket_t *sock, int port);
void vtp_stop(vtp_socket_t *sock);

#endif
