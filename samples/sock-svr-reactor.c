#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "common.h"

#define MAX_EVENTS 10
#define BUF_SIZE 5
#define LISTEN_PORT 12306
#define LISTEN_ADDR "127.0.0.1"
#define CONN_MAX 5

static void addfd_to_epoll(int epoll_fd, int fd, int epoll_type, int block_type) {
  struct epoll_event ep_event;
  ep_event.data.fd = fd;
  ep_event.events = EPOLLIN;
  ep_event.events |= epoll_type;
  int old_flags = fcntl(fd, F_GETFL);
  fcntl(fd, F_SETFL, old_flags | block_type);
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ep_event);
}

static void process_lt(int cli_sock) {
  char buffer[BUF_SIZE];
  int r;
  memset(buffer, 0, BUF_SIZE);
  r = recv(cli_sock, buffer, BUF_SIZE, 0);
  if (r == 0) {
    LOG_INFO("client closed");
    close(cli_sock);
  } else if (r < 0) {
    LOG_SERROR;
    close(cli_sock);
  } else {
    LOG_INFO("client message: %s, %d", buffer, r);
    buffer[r] = '\0';
    r = send(cli_sock, buffer, r, 0);
    SASSERT(r > 0);
    LOG_INFO("response %d bytes", r);
  }
}

static void process_et(int cli_sock) {
  char buffer[BUF_SIZE];
  int r;
  memset(buffer, 0, BUF_SIZE);
  r = recv(cli_sock, buffer, BUF_SIZE, 0);
  if (r == 0) {
    LOG_INFO("client closed");
    close(cli_sock);
  } else if (r < 0) {
    LOG_SERROR;
    close(cli_sock);
  } else {
    LOG_INFO("client message: %s, %d", buffer, r);
    buffer[r] = '\0';
    r = send(cli_sock, buffer, r, 0);
    SASSERT(r > 0);
    LOG_INFO("response %d bytes", r);
  }
}

static void process_et_loop(int cli_sock) {
  char buffer[BUF_SIZE];
  int r;
  while (true) {
    r = recv(cli_sock, buffer, sizeof(buffer), 0);
    if (r == 0) {
      LOG_INFO("client closed");
      close(cli_sock);
      break;
    } else if (r < 0) {
      LOG_SERROR;
      close(cli_sock);
      break;
    } else {
      LOG_INFO("client message: %s, %d", buffer, r);
      buffer[r] = '\0';
      send(cli_sock, buffer, r, 0);
    }
  }
}

static void process(int epoll_fd, struct epoll_event *events, int number,
  int serv_sock, int epoll_type, int block_type) {
  struct sockaddr_in cli_addr;
  socklen_t cli_len = sizeof(cli_addr);
  int newfd, cli_sock;
  for (int i = 0; i < number; ++i) {
    newfd = events[i].data.fd;
    if (newfd == serv_sock) {
      LOG_INFO("new client");
      cli_sock = accept(serv_sock, (struct sockaddr *) &cli_addr, &cli_len);
      addfd_to_epoll(epoll_fd, cli_sock, epoll_type, block_type);
    } else if (events[i].events & EPOLLIN) {
      if (epoll_type == EPOLLET) {
        LOG_INFO("et start ...");
        process_et(newfd);
        // process_et_loop(newfd);
      } else {
        LOG_INFO("lt start ...");
        process_lt(newfd);
      }
    } else {
      SASSERT("other events ...");
    }
  }
  
}

int main(int argc, char **argv) {
  LOG_INFO("starting process ...");
  int serv_sock, epoll_fd, number, reuse = 1, r = 0;
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(LISTEN_ADDR);
  serv_addr.sin_port = htons(LISTEN_PORT);

  serv_sock = socket(PF_INET, SOCK_STREAM, 0);
  SASSERT(serv_sock > 0);
  r = setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  SASSERT(r == 0);
  bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_in));
  listen(serv_sock, CONN_MAX);

  struct epoll_event events[MAX_EVENTS];
  epoll_fd = epoll_create1(0);
  SASSERT(epoll_fd > 0);

  // LT
  addfd_to_epoll(epoll_fd, serv_sock, 0, O_NONBLOCK);

  // ET
  // addfd_to_epoll(epoll_fd, serv_sock, EPOLLET, O_NONBLOCK);

  LOG_INFO("listening on %s:%d", LISTEN_ADDR, LISTEN_PORT);
  while (1) {
    number = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    SASSERT(number > 0);
    process(epoll_fd, events, number, serv_sock, 0, O_NONBLOCK);
  }

  LOG_INFO("process exit ...");

  return 0;
}
