#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "threadpool.h"

#define MAX_EVENTS 10
#define BUF_SIZE 8
#define INPUT_BUF_SIZE 1024
#define SVR_PORT 12306
#define SVR_ADDR "127.0.0.1"

static int epoll_fd;
static int sync_fd;
static char input_buf[INPUT_BUF_SIZE];
static uint64_t input_buf_size;
static bool is_connected = false;

static void set_epoll_event(int fd, int events, int flag) {
  struct epoll_event ep_event;
  ep_event.data.fd = fd;
  ep_event.events = events;
  SASSERT(epoll_ctl(epoll_fd, flag, fd, &ep_event) != -1);
}

static void connect_server(void *data) {
  int r;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(SVR_ADDR);
  addr.sin_port = htons(SVR_PORT);
  int old_flags = fcntl(sock, F_GETFL);
  SASSERT(fcntl(sock, F_SETFL, old_flags | O_NONBLOCK) != -1);
  r = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (r == 0) {
    is_connected = true;
  } else {
    SASSERT(errno == EINPROGRESS);
  }
  char buffer[BUF_SIZE];

  struct epoll_event events[MAX_EVENTS];
  set_epoll_event(sock, EPOLLOUT | EPOLLIN, EPOLL_CTL_ADD);

  int number;
  while (1) {
    number = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (number == 0) {
      SASSERT(errno == EINTR);
      continue;
    }
    for (int i = 0; i < number; ++i) {
      int fd = events[i].data.fd;
      if (events[i].events & EPOLLIN) {
        int r = read(events[i].data.fd, buffer, sizeof(buffer));
        if (r == 0) {
          LOG_ERROR("server closed");
          SASSERT(close(sock) == 0);
          exit(0);
          return;
        } else if (r < 0) {
          int err = errno;
          if (err == EAGAIN || err == EINTR) {
            continue;
          }
          LOG_ERROR("server error");
          LOG_ERROR("code: %d, message: %s", err, strerror(err));
          SASSERT(close(sock) == 0);
          exit(0);
          return;
        }
        if (fd == sync_fd) {
          LOG_INFO("sending %ld bytes", input_buf_size);
          send(sock, input_buf, input_buf_size, 0);
        } else {
          buffer[r] = '\0';
          LOG_INFO("message form server: %s, %d", buffer, r);
        }
      } else if (events[i].events & EPOLLOUT) {
        if (fd == sync_fd) {
          continue;
        }
        int err;
        socklen_t err_size = sizeof(err);
        SASSERT(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_size) == 0);
        if (err) {
          err = errno;
          LOG_ERROR("connect error: %d %s", err, strerror(err));
          set_epoll_event(fd, 0, EPOLL_CTL_DEL);
          SASSERT(close(fd));
          exit(0);
        } else {
          is_connected = true;
          set_epoll_event(fd, EPOLLIN, EPOLL_CTL_MOD);
          LOG_INFO("connected to server, port %d", htons(addr.sin_port));
        }
      } else if (events[i].events & EPOLLERR) {
        SASSERT(0, "epoll error");
      } else {
        SASSERT(0, "unhandled epoll event");
      }
    }
  }
}

int main() {
  LOG_INFO("starting ...");

  SASSERT((epoll_fd = epoll_create1(0)) > 0);
  SASSERT((sync_fd = eventfd(0, EFD_NONBLOCK)) > 0);
  set_epoll_event(sync_fd, EPOLLIN, EPOLL_CTL_ADD);

  threadpool_init(4);
  threadpool_post_task(connect_server, NULL);

  while (scanf("%s", input_buf) != EOF) {
    if (!is_connected) {
      LOG_ERROR("server is not connected yet");
      continue;
    }
    input_buf_size = strlen(input_buf);
    SASSERT(write(sync_fd, &input_buf_size, 8) == 8);
  }

  SASSERT(close(sync_fd) == 0);
  SASSERT(close(epoll_fd) == 0);
  LOG_INFO("bye");

  return 0;
}
