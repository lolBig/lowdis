#include "threadpool.h"

#define BUF_SIZE 5

void on_new_client(void *data) {
  int cli_sock = *(int*)data;
  LOG_INFO("new client");
  char buffer[BUF_SIZE];
  int r;
  while (true) {
    r = recv(cli_sock, buffer, sizeof(buffer), 0);
    if (r == 0) {
      LOG_INFO("client closed");
      break;
    } else if (r < 0) {
      LOG_SERROR;
      break;
    } else {
      LOG_INFO("client message: %s, %d", buffer, r);
      buffer[r] = '\0';
      send(cli_sock, buffer, r, 0);
    }
  }
  close(cli_sock);
  free(data);
}

int main(int argc, char **argv) {
  threadpool_init(4);
  int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT(serv_sock > 0, "create socket error %d", serv_sock);
  int reuse = 1;
  SASSERT(setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) == 0);
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(12306);
  SASSERT(bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0);
  SASSERT(listen(serv_sock, 20) == 0);

  struct sockaddr_in cli_addr;
  socklen_t cli_len = sizeof(cli_addr);
  while (true) {
    int *cli_sock = (int*) malloc(sizeof(int));
    *cli_sock = accept(serv_sock, (struct sockaddr *)&cli_addr, &cli_len);
    threadpool_post_task(on_new_client, cli_sock);
  }
  close(serv_sock);

  return 0;
}
