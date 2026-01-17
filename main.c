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

  char buf[100] = {0};
  snprintf(buf, sizeof(buf), "%smini-tmux-%d", MINI_TMUX_SOCK, uid);
  socket_path = buf;
  if (mkdir(socket_path, 0755) != 0 && errno != EEXIST) {
    perror("mkdir failed");
    return -1;
  }
  client_init(&client);
  if (client_main(&client) < 0) {
    return -1;
  }
  return 0;
}