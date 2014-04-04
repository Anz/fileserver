#ifndef VTS
#define VTS

#include "vfs.h"
#include <pthread.h>

#define VTS_SOCKET_INIT 0

typedef vfsn_t* vtp_socket_t;

/*
 * Starts socket on given address, handles client connection and blocks until vts_stop gets called.
 */
int vtp_start(vtp_socket_t *sock, int port, int max_clients);

/*
 * Stops given socket.
 */
void vtp_stop(vtp_socket_t *sock);

#endif
