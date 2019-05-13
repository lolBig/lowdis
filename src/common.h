#ifndef LOWDIS_COMMON_H
#define LOWDIS_COMMON_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include "logger.h"

#define ASSERT(exp, ...) \
  do { if (!(exp)) { LOG_FATAL(__VA_ARGS__); assert(0); } } while (0)

#define SASSERT(exp, ...) \
  do { if (!(exp)) { LOG_FATAL("%s", strerror(errno)); assert(0); } } while (0)

#define LOG_SERROR do { LOG_ERROR("%s", strerror(errno)); } while (0)

#endif //LOWDIS_COMMON_H
