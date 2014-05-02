/*
* Copyright (C) 2014 Roger Knecht
* License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "log.h"
#include "vts.h"

static vts_socket_t socket;

static void print_usage(void)
{
   puts("usage: fileserver -p port [-c maxclients] [-l trace|debug|info|warn|error]");
}

static void signal_handler(int signal)
{
   vts_stop(&socket);
}

int main(int argc, char* argv[])
{
   int port = -1, maxclients = 50;

   // setup logger
   log_set(STDOUT_FILENO);

   int c;
   while((c = getopt(argc, argv, "p:c:l:")) != -1) {
      switch(c) {
         case 'p': port = atoi(optarg); break;
         case 'c': maxclients = atoi(optarg); break;
         case 'l':
            if (strcmp("trace", optarg) == 0) log_level_set(LOG_TRACE);
            else if (strcmp("debug", optarg) == 0) log_level_set(LOG_DBG);
            else if (strcmp("info", optarg) == 0) log_level_set(LOG_INFO);
            else if (strcmp("warn", optarg) == 0) log_level_set(LOG_WARN);
            else if (strcmp("error", optarg) == 0) log_level_set(LOG_ERR);
            break;
      }
   }
   
   // parse args
   if (port < 0) {
      print_usage();
      return 1;
   }

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
   int retval = vts_start(&socket);

   // release socket
   vts_release(&socket);

   return retval;
}
