#include "common.h"
int main() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(12306);
  connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  char buffer[10];
  while (1) {
    scanf("%s", buffer);
    send(sock, buffer, strlen(buffer), 0);
    int r = recv(sock, buffer, sizeof(buffer), 0);
    LOG_INFO("Message form server: %s, %d", buffer, r);
  }

  close(sock);
  return 0;
}
