#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

static int logfd = 0;

static void log_write(char* level, char* fmt, va_list args)
{
   dprintf(logfd, "%s: ", level);
   vdprintf(logfd, fmt, args);
   dprintf(logfd, "\n");
   sync();
}

void log_set(int fd)
{
   logfd = fd;
}

int log_get(void)
{
   return logfd;
}

void log_info(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write("INFO", fmt, args);
   va_end(args);
}

void log_dbg(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write("DEBUG", fmt, args);
   va_end(args);
}

void log_warn(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write("WARN", fmt, args);
   va_end(args);
}

void log_err(char* fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   log_write("ERROR", fmt, args);
   va_end(args);
}
