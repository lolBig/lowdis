#include "threadpool.h"
#include "logger.h"

void thread_test(void *data) {
  int i = (int) data;
  LOG_INFO("thread %d", i);
}

int main()
{
  threadpool_init(4);
  for (int i = 0; i < 8; ++i) {
    threadpool_post_task(thread_test, (void *)i);
  }

  LOG_ERROR("done");
  getchar();
  return 0;
}
