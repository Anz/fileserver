#ifndef LOG_H
#define LOH_H

void log_set(int fd);
int log_get(void);
void log_info(char* fmt, ...);
void log_dbg(char* fmt, ...);
void log_warn(char* fmt, ...);
void log_err(char* fmt, ...);

#endif
