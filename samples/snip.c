
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_once_t once = PTHREAD_ONCE_INIT;

void once_run(void)
{
  printf("once_run in thread %d\n", (unsigned int)pthread_self());
  sleep(1);
}

void *run(void *arg)
{
  pthread_t tid = pthread_self();
  printf("thread %d enter\n", (unsigned int)tid);
  pthread_once(&once, once_run);
  printf("thread %d return\n", (unsigned int)tid);
  pthread_exit(NULL);
}

int main(void)
{
  pthread_t tid1, tid2, tid3;
  pthread_create(&tid1, NULL, run, NULL);
  pthread_create(&tid2, NULL, run, NULL);
  void *r;
  pthread_join(tid1, &r);
  pthread_join(tid2, &r);
  sleep (2);
  pthread_create(&tid3, NULL, run, NULL);
  pthread_join(tid3, &r);
  return 0;
}
