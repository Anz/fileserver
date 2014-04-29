#ifndef LOG_H
#define LOH_H

#define LOG_TRACE 0
#define LOG_DBG   1
#define LOG_INFO  2
#define LOG_WARN  3
#define LOG_ERR   4

void log_level_set(int level);
void log_set(int fd);
int log_get(void);
void log_trace(char* fmt, ...);
void log_dbg(char* fmt, ...);
void log_info(char* fmt, ...);
void log_warn(char* fmt, ...);
void log_err(char* fmt, ...);

#endif
