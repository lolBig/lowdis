#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "threadpool.h"

#define MAX_EVENTS 10
#define BUF_SIZE 8
#define SVR_PORT 12306
#define SVR_ADDR "127.0.0.1"

static int epoll_fd;
static int sync_fd;
static char input_buf[1024];
static uint64_t input_buf_size;

void addfd_to_epoll(int epoll_fd, int fd) {
  struct epoll_event ep_event;
  ep_event.data.fd = fd;
  ep_event.events = EPOLLIN;
  SASSERT(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ep_event) != -1);
}

void process(void *data) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(SVR_ADDR);
  serv_addr.sin_port = htons(SVR_PORT);
  int old_flags = fcntl(sock, F_GETFL);
  SASSERT(fcntl(sock, F_SETFL, old_flags | O_NONBLOCK) != -1);
  connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  char buffer[BUF_SIZE];

  struct epoll_event events[MAX_EVENTS];
  addfd_to_epoll(epoll_fd, sock);

  int number;
  while (1) {
    number = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    SASSERT(number > 0);
    for (int i = 0; i < number; ++i) {
      int new_fd = events[i].data.fd;
      if (events[i].events & EPOLLIN) {
        int r = read(events[i].data.fd, buffer, sizeof(buffer));
        if (r == 0) {
          LOG_INFO("server closed");
          send(sync_fd, "", 1, 0);
          close(sock);
          return;
        } else if (r < 0) {
          int err = errno;
          LOG_ERROR("recv error: %ld %d %d %s", sizeof(buffer), err, strerror(err));
          if (err == EAGAIN || err == EWOULDBLOCK) {
            continue;
          }
          LOG_ERROR("unhandled error");
          send(sync_fd, "", 1, 0);
          close(sock);
          return;
        }
        if (new_fd == sync_fd) {
          LOG_INFO("sending %ld bytes", input_buf_size);
          send(sock, input_buf, input_buf_size, 0);
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
  LOG_INFO("starting ...");
  SASSERT((epoll_fd = epoll_create1(0)) > 0);
  SASSERT((sync_fd = eventfd(0, EFD_NONBLOCK)) > 0);
  addfd_to_epoll(epoll_fd, sync_fd);

  threadpool_init(4);
  threadpool_post_task(process, NULL);
  while (1) {
    scanf("%s", input_buf);
    if (strcmp(input_buf, "") == 0) {
      break;
    }
    input_buf_size = strlen(input_buf);
    SASSERT(write(sync_fd, &input_buf_size, 8) == 8);
  }
  SASSERT(close(sync_fd) == 0);
  SASSERT(close(epoll_fd) == 0);

  return 0;
}
