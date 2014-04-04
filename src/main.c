#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "log.h"
#include "vts.h"

static vtp_socket_t socket;

static void print_usage(void)
{
   puts("usage: fileserver <port> [maxclients]");
}

static void signal_handler(int signal)
{
   vtp_stop(&socket);
}

int main(int argc, char* argv[])
{
   int port, maxclients = 50;

   // setup logger
   log_set(STDOUT_FILENO);
   
   // parse args
   if (argc < 2) {
      print_usage();
      return 1;
   }
   port = atoi(argv[1]);

   if (argc > 2)
      maxclients = atoi(argv[2]);

   // init socket
   if (vts_init(&socket, port, maxclients)) {
      return 1;
   }

   // set signal handler
   struct sigaction sighandler;
   sighandler.sa_handler = signal_handler;
   sigemptyset(&sighandler.sa_mask);
   sighandler.sa_flags= 0;
   sigaction(SIGINT, &sighandler, NULL);

   // start socket
   int retval = vtp_start(&socket);

   // release socket
   vts_release(&socket);

   return retval;
}
