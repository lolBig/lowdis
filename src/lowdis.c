#include "threadpool.h"

void *on_new_client(void *data)
{
  int cli_sock = *(int*)data;
  LOG_INFO("new client");
  char buffer[40];
  while (true) {
    int r = read(cli_sock, buffer, sizeof(buffer));
    LOG_INFO("client message: %s, %d", buffer, r);
    write(cli_sock, buffer, r);
  }
  close(cli_sock);
  free(data);
  pthread_exit(0);
}

int main(int argc, char **argv)
{
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
  while (1)
  {
    int *cli_sock = (int*) malloc(sizeof(int));
    *cli_sock = accept(serv_sock, (struct sockaddr *)&cli_addr, &cli_len);
    pthread_t pid;
    pthread_create(&pid, NULL, on_new_client, cli_sock);
    pthread_detach(pid);
  }
  close(serv_sock);

  return 0;
}
