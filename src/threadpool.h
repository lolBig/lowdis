#ifndef LOWDIS_THREADPOOL_H
#define LOWDIS_THREADPOOL_H

#include "common.h"

typedef void (*threadpool_func)(void *data);

void threadpool_init(int size);

void threadpool_post_task(threadpool_func func, void *data);

#endif 