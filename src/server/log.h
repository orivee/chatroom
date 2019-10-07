#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <pthread.h>

enum {LOG_ERROR, LOG_INFO, LOG_DEBUG};

#define log_error(...) log_log(LOG_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)


extern void log_set_fp(const char * file);
extern void log_set_quiet(int enable);
extern void log_close_fp();
extern void log_log(int level, const char * file, const char * func, int line, const char * str, ...);

#endif
