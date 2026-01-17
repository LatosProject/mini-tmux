#include "main.h"
#include "client.h"
#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct client client;
char *socket_path;

int main() {
  uid_t uid = getuid();

  // 创建目录
  char dir[100] = {0};
  snprintf(dir, sizeof(dir), "%smini-tmux-%d", MINI_TMUX_SOCK, uid);
  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    perror("mkdir failed");
    return -1;
  }

  // socket_path 指向目录内的文件
  static char buf[100] = {0};
  snprintf(buf, sizeof(buf), "%s/default", dir);
  socket_path = buf;
  client_init(&client);
  if (client_main(&client) < 0) {
    return -1;
  }
  return 0;
}