#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "threadpool.h"

#define MAX_EVENTS 10
#define BUF_SIZE 1024 * 1024
#define SCANF_BUF_SISE 1024
#define SVR_PORT 12306
#define SVR_ADDR "127.0.0.1"
#define FILE_BUF_LEN 1024 * 1024

static int epoll_fd;
static int cli_fd;
static int sync_fd;
static int sync_buf_size = sizeof(uint64_t);
static bool is_connected = false;
static char scanf_buf[SCANF_BUF_SISE];
static uint64_t scanf_buf_size;
static char file_buf[FILE_BUF_LEN];
static uint64_t file_size;
static char *file_cursor;
static char *read_buf[BUF_SIZE];
static struct sockaddr_in addr;
static size_t total_received_size = 0;

static void set_epoll_event(int fd, int events, int flag) {
  struct epoll_event ep_event;
  ep_event.data.fd = fd;
  ep_event.events = events;
  SASSERT(epoll_ctl(epoll_fd, flag, fd, &ep_event) != -1);
}

static void send_bytes(int fd) {
  if (file_cursor == file_buf) {
    LOG_INFO("start send %ld bytes", file_size);
  }
  char *file_end = file_buf + file_size;
  ssize_t r;
  while (file_cursor != file_end) {
    size_t write_size = file_size - (file_cursor - file_buf);
    if ((r = write(fd, file_cursor, write_size)) > 0) {
      file_cursor += r;
      LOG_INFO("sending %d bytes", r);
    } else {
      LOG_ERROR("write error: %s", strerror(errno));
      SASSERT(errno == EAGAIN);
      break;
    }
  }
  if (file_cursor == file_end) {
    file_cursor = NULL;
    set_epoll_event(fd, EPOLLIN, EPOLL_CTL_MOD);
    LOG_INFO("finish send %d bytes", file_size);
  } else {
    set_epoll_event(fd, EPOLLIN | EPOLLOUT, EPOLL_CTL_MOD);
    int sent_size = file_cursor - file_buf;
    int left_size = file_end - file_cursor;
    LOG_INFO("sent %d bytes, left %d bytes", sent_size, left_size);
  }
}

static void handle_poll_in(int fd) {
  LOG_INFO("poll in event");
  if (fd == sync_fd) {
    LOG_INFO("start send user input");
    SASSERT(read(sync_fd, &scanf_buf_size, sync_buf_size) == sync_buf_size);
    send_bytes(cli_fd);
    return;
  }
  ssize_t r = read(fd, read_buf, sizeof(read_buf) - 1);
  if (r == 0) {
    LOG_ERROR("server closed");
    SASSERT(close(fd) == 0);
    exit(0);
  } else if (r < 0) {
    if (errno == EAGAIN || errno == EINTR) {
      return;
    }
    LOG_ERROR("server error");
    LOG_ERROR("code: %d, message: %s", errno, strerror(errno));
    SASSERT(close(fd) == 0);
    exit(0);
  }
  read_buf[r] = '\0';
  total_received_size += r;
  LOG_INFO("received %d bytes, total %ld", r, total_received_size);
}

static void handle_poll_out(int fd) {
  LOG_INFO("poll out event");
  SASSERT(fd != sync_fd);
  if (!is_connected) {
    is_connected = true;
    set_epoll_event(fd, EPOLLIN, EPOLL_CTL_MOD);
    int err;
    socklen_t err_size = sizeof(err);
    SASSERT(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_size) == 0);
    if (err) {
      LOG_ERROR("connect error: %d %s", err, strerror(errno));
      set_epoll_event(fd, 0, EPOLL_CTL_DEL);
      SASSERT(close(fd));
      exit(0);
    } else {
      is_connected = true;
      set_epoll_event(fd, EPOLLIN, EPOLL_CTL_MOD);
      LOG_INFO("connected to server, port %d", htons(addr.sin_port));
    }
  } else {
    send_bytes(fd);
  }
}

static void connect_server() {
  ssize_t r;
  cli_fd = socket(AF_INET, SOCK_STREAM, 0);

  int opt_buf = 1024 * 1024;
  socklen_t opt_buf_len = sizeof(opt_buf);
  setsockopt(cli_fd, SOL_SOCKET, SO_SNDBUF, &opt_buf, opt_buf_len);
  getsockopt(cli_fd, SOL_SOCKET, SO_SNDBUF, (void *)&opt_buf, &opt_buf_len);
  LOG_INFO("snd buf len %d", opt_buf);

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(SVR_ADDR);
  addr.sin_port = htons(SVR_PORT);
  int old_flags = fcntl(cli_fd, F_GETFL);
  SASSERT(fcntl(cli_fd, F_SETFL, old_flags | O_NONBLOCK) != -1);
  r = connect(cli_fd, (struct sockaddr *)&addr, sizeof(addr));
  if (r == 0) {
    is_connected = true;
  } else {
    SASSERT(errno == EINPROGRESS);
  }
}

static void cli_lt(void *data) {
  connect_server();

  struct epoll_event events[MAX_EVENTS];
  int flags = EPOLLIN | (is_connected ? 0: EPOLLOUT);
  set_epoll_event(cli_fd, flags, EPOLL_CTL_ADD);

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
        handle_poll_in(fd);
      } else if (events[i].events & EPOLLOUT) {
        handle_poll_out(fd);
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

  FILE *file_ptr = fopen("./assert/index.html", "r");
  file_size = fread(file_buf, 1, FILE_BUF_LEN, file_ptr);
  LOG_INFO("file size: %ld", file_size);
  fclose(file_ptr);

  file_cursor = NULL;

  threadpool_init(4);
  threadpool_post_task(cli_lt, NULL);

  while (scanf("%s", scanf_buf) != EOF) {
    if (!is_connected) {
      LOG_ERROR("server is not connected yet");
      continue;
    }
    if (file_cursor != NULL) {
      LOG_ERROR("EAGAIN");
      continue;
    }
    file_cursor = file_buf;
    scanf_buf_size = strlen(scanf_buf);
    SASSERT(write(sync_fd, &scanf_buf_size, sync_buf_size) == sync_buf_size);
  }

  SASSERT(close(sync_fd) == 0);
  SASSERT(close(epoll_fd) == 0);
  LOG_INFO("bye");

  return 0;
}

