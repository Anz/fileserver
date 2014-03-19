#ifndef VTP
#define VTP

#include "vfs.h"

typedef struct {
   int sockfd;
   vfsn_t* root;
} vtps_t;

int vtp_start(vtps_t *vtps, int port);
void vtp_stop(vtps_t *vtps);

#endif
