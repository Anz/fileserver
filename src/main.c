#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "vts.h"

static vtp_socket_t socket = VTS_SOCKET_INIT;

void signal_handler(int signal)
{
   printf("shutdown server\n");
   vtp_stop(&socket);
}

int main(int argc, char* argv[]) {
   int port = 8000, maxclients = 50;
   
   // parse args
   if (argc > 1)
      port = atoi(argv[1]);

   if (argc > 2)
      maxclients = atoi(argv[2]);

   // set signal handler
   struct sigaction sighandler;
   sighandler.sa_handler = signal_handler;
   sigemptyset(&sighandler.sa_mask);
   sighandler.sa_flags= 0;
   sigaction(SIGINT, &sighandler, NULL);

   // start socket
   int result = vtp_start(&socket, port, maxclients);
   printf("socket returned %i\n",  result);

   return 0;
}
