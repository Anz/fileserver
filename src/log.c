#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>

static int logfd = 0;
static int log_level = LOG_INFO;
static pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;

static char *log_levels[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERR" };

static void log_write(int level, char* fmt, va_list args)
{
   pthread_mutex_lock(&fd_lock);
   if (level < log_level) {
      pthread_mutex_unlock(&fd_lock);
      return;
   }

   dprintf(logfd, "%s: ", log_levels[level]);
   vdprintf(logfd, fmt, args);
   dprintf(logfd, "\n");
   sync();
   pthread_mutex_unlock(&fd_lock);
}

void log_level_set(int level)
{
   pthread_mutex_lock(&fd_lock);
   log_level = level;
   pthread_mutex_unlock(&fd_lock);
}

void log_set(int fd)
{
   pthread_mutex_lock(&fd_lock);
   logfd = fd;
   pthread_mutex_unlock(&fd_lock);
}

int log_get(void)
{
   pthread_mutex_lock(&fd_lock);
   int fd =  logfd;
   pthread_mutex_unlock(&fd_lock);
   return fd;
}

void log_trace(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write(LOG_TRACE, fmt, args);
   va_end(args);
}

void log_dbg(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write(LOG_DBG, fmt, args);
   va_end(args);
}


void log_info(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write(LOG_INFO, fmt, args);
   va_end(args);
}

void log_warn(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write(LOG_WARN, fmt, args);
   va_end(args);
}

void log_err(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write(LOG_ERR, fmt, args);
   va_end(args);
}
