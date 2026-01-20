#include "main.h"
#include "client.h"
#include "log.h"
#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct client client;
char *socket_path;
int detached_session_id = -1;
int list_sessions = 0;

static void print_help(const char *prog) {
  printf("mini-tmux - a minimal terminal multiplexer\n\n");
  printf("        Version: 0.1.0 By Latos\n\n");
  printf("Usage: %s [options]\n\n", prog);
  printf("Options:\n");
  printf("  -l         List all sessions\n");
  printf("  -s <id>    Attach to detached session by id\n");
  printf("  -h         Show this help message\n\n");
  printf("Key bindings:\n");
  printf("  Ctrl+B d   Detach from current session\n\n");
  printf("Examples:\n");
  printf("  %s           Start a new session\n", prog);
  printf("  %s -l        List all sessions\n", prog);
  printf("  %s -s 0      Attach to session 0\n", prog);
}

int main(int argc, char *argv[]) {
  if (argc == 2 &&
      (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    print_help(argv[0]);
    return 0;
  }
  if (argc == 2 && (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "-L") == 0)) {
    list_sessions = 1;
  }
  if (argc == 3 && (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "-S") == 0)) {
    detached_session_id = strtol(argv[2], NULL, 10);
    log_info("attaching to session id=%d\n", detached_session_id);
  }
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