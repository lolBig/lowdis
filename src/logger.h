#ifndef LOWDIS_LOGGER_H_
#define LOWDIS_LOGGER_H_

#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef enum {
  LOG_LEVEL_VERBOSE = 0,
  LOG_LEVEL_INFO = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_ERROR = 3,
  LOG_LEVEL_FATAL = 4,
} log_level;

#define LOG_VERBOSE(...)  do_log(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)     do_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)     do_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)    do_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...)    do_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

extern
void do_log(log_level level, const char *file, int line, const char *fmt, ...);

#endif
