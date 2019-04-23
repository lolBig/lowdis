#include "threadpool.h"
#include "queue.h"

typedef enum {
  RUNNING = 0,
  STOPPED = 1,
} threadpool_state;

typedef struct {
  threadpool_func func;
  void *func_data;
  QUEUE node; 
} threadpool_task;

typedef struct {
  pthread_t thread;
} threadpool_runner;

static threadpool_runner *task_runners = NULL;
static int threadpool_size = 0;
static QUEUE task_queue;
static pthread_mutex_t task_lock;
static pthread_cond_t task_cond;
static threadpool_state task_state;

static void* threadpool_run_task(void *data) {
  threadpool_runner *runner = (threadpool_runner *) data;
  threadpool_task *task = NULL;
  pthread_mutex_lock(&task_lock);
  while (true) {
    if (task == NULL) {
      pthread_cond_wait(&task_cond, &task_lock);
    }
    if (task_state == STOPPED) {
      pthread_mutex_unlock(&task_lock);
      break;
    }
    QUEUE *head = QUEUE_HEAD(&task_queue);
    QUEUE_REMOVE(head);
    threadpool_task *task = QUEUE_DATA(head, threadpool_task, node);
    pthread_mutex_unlock(&task_lock);
    task->func(task->func_data);
    free(task);
    task = NULL;
    pthread_mutex_lock(&task_lock);
    if (!QUEUE_EMPTY(task_queue)) {
      QUEUE *head = QUEUE_HEAD(&task_queue);
      QUEUE_REMOVE(head);
      task = QUEUE_DATA(head, threadpool_task, node);
    }
  }
  free(runner);
  pthread_exit(NULL);
  return NULL;
}

void threadpool_init(int size) {
  task_state = RUNNING;
  threadpool_size = size;
  QUEUE_INIT(&task_queue);
  task_runners = malloc(sizeof(threadpool_runner) * size);
  pthread_mutex_init(&task_lock, NULL);
  pthread_cond_init(&task_cond, NULL);
  for (int i = 0; i < size; ++i) {
    threadpool_runner *runner = task_runners + i;
    memset(runner, 0, sizeof(threadpool_runner));
    pthread_create(&runner->thread, NULL, threadpool_run_task, runner);
    pthread_detach(runner->thread);
  }
}

void threadpool_append_task(threadpool_func func, void *data) {
  threadpool_task *task = malloc(sizeof(threadpool_task));
  task->func = func;
  task->func_data = data;
  pthread_mutex_lock(&task_lock);
  QUEUE_INIT(&task->node);
  QUEUE_INSERT_TAIL(&task_queue, &task->node);
  pthread_cond_signal(&task_cond);
  pthread_mutex_unlock(&task_lock);
}

void threadpool_stop(threadpool_func func) {

}

void threadpool_stop_all() {
  pthread_mutex_lock(&task_lock);
  task_state = STOPPED;
  task_runners = NULL;
  pthread_cond_broadcast(&task_cond);
  pthread_mutex_unlock(&task_lock);
}
