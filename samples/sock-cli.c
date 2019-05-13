#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "threadpool.h"

#define MAX_EVENTS 10
#define BUF_SIZE 5
#define SVR_PORT 12306
#define SVR_ADDR "127.0.0.1"

void addfd_to_epoll(int epoll_fd, int fd) {
  struct epoll_event ep_event;
  ep_event.data.fd = fd;
  ep_event.events = EPOLLIN;
  int old_flags = fcntl(fd, F_GETFL);
  fcntl(fd, old_flags | O_NONBLOCK);
}

void process(void *data) {
  int sync_fd = *((int *)data);
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  int epoll_fd;
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SVR_ADDR);
  serv_addr.sin_port = htons(SVR_PORT);
  connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  char buffer[BUF_SIZE];

  struct epoll_event events[MAX_EVENTS];
  epoll_fd = epoll_create(0);
  SASSERT(epoll_fd > 0);
  addfd_to_epoll(epoll_fd, sync_fd);
  addfd_to_epoll(epoll_fd, sock);

  int number;
  char buffer[BUF_SIZE];
  while (1) {
    number = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    SASSERT(number > 0);
    for (int i = 0; i < number; ++i) {
      int new_fd = events[i].data.fd;
      if (events[i].events & EPOLLIN) {
        int r = recv(sock, buffer, sizeof(buffer), 0);
        if (r == 0) {
          LOG_INFO("server closed");
          send(sync_fd, "", 1, 0);
          close(sock);
          return;
        } else if (r < 0) {
          LOG_SERROR;
          send(sync_fd, "", 1, 0);
          close(sock);
          return;
        }
        if (new_fd == sync_fd) {
          send(sock, buffer, r, 0);
        } else {
          buffer[r] = '\0';
          LOG_INFO("Message form server: %s, %d", buffer, r);
        }
      } else {
        SASSERT("new event");
      }
    }
  }
}

int main() {
  threadpool_init(4);
  int r;
  int sync_fd = eventfd(0, 0);
  SASSERT(sync_fd > 0);
  threadpool_post_task(process, &sync_fd);
  char buffer[1024];
  size_t buffer_len;
  while (1) {
    scanf("%s", buffer);
    if (strcmp(buffer, "") == 0) {
      break;
    }
    buffer_len = strlen(buffer);
    r = send(sync_fd, buffer, buffer_len, 0);
    SASSERT(r == buffer_len);
  }
  r = close(sync_fd);
  SASSERT(r == 0);

  return 0;
}
