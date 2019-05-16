#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "common.h"

#define MAX_EVENTS 10
#define BUF_SIZE 1024 * 1024
#define LISTEN_PORT 12306
#define LISTEN_ADDR "127.0.0.1"
#define CONN_MAX 5

typedef void (*epoll_event_handler)(struct epoll_event *);
typedef void (*epoll_accept_handler)(struct epoll_event *,
  struct sockaddr_in *, socklen_t *);

static int epoll_type = EPOLLET;
static int block_type = O_NONBLOCK;
static int epoll_fd;
static int serv_fd;
static epoll_accept_handler handle_connection;
static epoll_event_handler handle_cli_event;
static int cli_count = 0;

typedef struct {
  char ip[INET_ADDRSTRLEN];
  uint16_t port;
  struct sockaddr_in addr;
  int fd;
} addr_info_t;

static addr_info_t* get_addr_info(int fd, struct sockaddr_in *addr) {
  addr_info_t *info = (addr_info_t *) malloc(sizeof(addr_info_t));
  memset(info, 0, sizeof(addr_info_t));
  info->fd = fd;
  info->addr = *addr;
  char *ip = inet_ntoa(addr->sin_addr);
  memcpy(info->ip, ip, strlen(ip));
  info->port = htons(addr->sin_port);
  return info;
}
static void addfd_to_epoll(struct epoll_event *event, int fd, struct sockaddr_in *addr) {
  event->events = EPOLLIN | epoll_type;
  event->data.ptr = get_addr_info(fd, addr);
  if (block_type == O_NONBLOCK) {
    int old_flags = fcntl(fd, F_GETFL);
    SASSERT(fcntl(fd, F_SETFL, old_flags | block_type) != -1);
  }
  SASSERT(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, event) != -1);
}

static void removefd_from_epoll(struct epoll_event *event) {
  addr_info_t *info = event->data.ptr;
  SASSERT(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, info->fd, event) == 0);
  SASSERT(close(info->fd) == 0);
  --cli_count;
  LOG_INFO("client %s:%d closed, total %d", info->ip, info->port, cli_count);
  free(info);
}

static void process_in_lt(struct epoll_event *event) {
  addr_info_t *cli_info = (addr_info_t *)event->data.ptr;
  char buffer[BUF_SIZE];
  int r;
  memset(buffer, 0, BUF_SIZE);
  r = read(cli_info->fd, buffer, BUF_SIZE);
  if (r > 0) {
    buffer[r] = '\0';
    SASSERT(write(cli_info->fd, buffer, r) == r);
    LOG_INFO("lt response %d bytes", r);
  } else {
    if (r < 0) {
      int err = errno;
      LOG_ERROR("code: %d, message: %s", err, strerror(err));
    }
    removefd_from_epoll(event);
    SASSERT(close(cli_info->fd) == 0);
  }
}

static void process_in_et(struct epoll_event *event) {
  addr_info_t *cli_info = (addr_info_t *)event->data.ptr;
  char buffer[BUF_SIZE];
  int r;
  while (true) {
    r = read(cli_info->fd, buffer, sizeof(buffer) - 1);
    if (r == 0) {
      removefd_from_epoll(event);
      break;
    } else if (r < 0) {
      int err = errno;
      if (err == EAGAIN || err == EINTR) {
        break;
      }
      const char *errstr = strerror(errno);
      LOG_ERROR("read %s:%d error: %s", cli_info->ip, cli_info->port, errstr);
      removefd_from_epoll(event);
      break;
    } else {
      buffer[r] = '\0';
      SASSERT(write(cli_info->fd, buffer, r) == r);
      LOG_INFO("et response %d bytes", r);
    }
  }
}

static void process_accept_lt(struct epoll_event *event,
  struct sockaddr_in *cli_addr, socklen_t *cli_len) {
  int cli_fd = accept(serv_fd, (struct sockaddr *)cli_addr, cli_len);
  SASSERT(cli_fd > 0);
  addfd_to_epoll(event, cli_fd, cli_addr);
  addr_info_t *cli_info = (addr_info_t *)event->data.ptr;
  ++cli_count;
  LOG_INFO("new lt client: %s:%d, total %d", cli_info->ip, cli_info->port, cli_count);
}

static void process_accept_et(struct epoll_event *event,
  struct sockaddr_in *cli_addr, socklen_t *cli_len) {
  int cli_fd;
  while (1) {
    cli_fd = accept(serv_fd, (struct sockaddr *)cli_addr, cli_len);
    if (cli_fd == -1) {
      int err = errno;
      if (err != EAGAIN && err != EINTR) {
        LOG_ERROR("accept error: %d, %s", err, strerror(err));
      }
      break;
    }
    addfd_to_epoll(event, cli_fd, cli_addr);
    addr_info_t *cli_info = (addr_info_t *)event->data.ptr;
    ++cli_count;
    LOG_INFO("new et client: %s:%d, total %d", cli_info->ip, cli_info->port, cli_count);
  }
}

int main(int argc, char **argv) {
  LOG_INFO("starting process ...");
  if (epoll_type == EPOLLET) {
    handle_cli_event = process_in_et;
    handle_connection = process_accept_et;
    LOG_INFO("et trigger");
  } else {
    handle_cli_event = process_in_lt;
    handle_connection = process_accept_lt;
    LOG_INFO("lt trigger");
  }
  if (block_type == O_NONBLOCK) {
    LOG_INFO("NONBLOCK mode");
  } else {
    LOG_INFO("BLOCK mode");
  }

  int reuse = 1;
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(LISTEN_ADDR);
  serv_addr.sin_port = htons(LISTEN_PORT);

  SASSERT((serv_fd = socket(PF_INET, SOCK_STREAM, 0)) > 0);
  SASSERT(setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0);
  SASSERT(setsockopt(serv_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == 0);
  SASSERT(bind(serv_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == 0);
  SASSERT(listen(serv_fd, CONN_MAX) == 0);

  struct epoll_event events[MAX_EVENTS];
  SASSERT((epoll_fd = epoll_create1(0)) > 0);

  struct epoll_event serv_event;
  addfd_to_epoll(&serv_event, serv_fd, &serv_addr);
  addr_info_t *serv_info = (addr_info_t *)serv_event.data.ptr;

  LOG_INFO("listening on %s:%d", serv_info->ip, serv_info->port);
  int number;
  struct sockaddr_in cli_addr;
  socklen_t cli_len = sizeof(struct sockaddr_in);
  while (1) {
    SASSERT((number = epoll_wait(epoll_fd, events, MAX_EVENTS, -1)) > 0);
    for (int i = 0; i < number; ++i) {
      if (events[i].events & EPOLLIN) {
        if (((addr_info_t *)events[i].data.ptr)->fd == serv_fd) {
          handle_connection(&events[i], &cli_addr, &cli_len);
        } else {
          sleep(5);
          handle_cli_event(&events[i]);
        }
      } else {
        SASSERT("other events ...");
      }
    }
  }

  LOG_INFO("process exit ...");

  return 0;
}

