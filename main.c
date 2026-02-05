#include "main.h"
#include "client.h"
#include "i18n.h"
#include "log.h"
#include "spawn.h"
#include "version.h"
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
int kill_session_id = -1;

static void print_help(const char *prog) {
  printf("%s", TR(MSG_HELP_TITLE));
  printf(TR(MSG_HELP_VERSION), MINI_TMUX_VERSION);
  printf(TR(MSG_HELP_USAGE), prog);
  printf("%s", TR(MSG_HELP_OPTIONS));
  printf("%s", TR(MSG_HELP_OPT_LIST));
  printf("%s", TR(MSG_HELP_OPT_ATTACH));
  printf("%s", TR(MSG_HELP_OPT_KILL));
  printf("%s", TR(MSG_HELP_OPT_HELP));
  printf("%s", TR(MSG_HELP_KEYBINDINGS));
  printf("%s", TR(MSG_HELP_KEY_DETACH));
  printf("%s", TR(MSG_HELP_KEY_SPLIT));
  printf("%s", TR(MSG_HELP_KEY_NEXT));
  printf("%s", TR(MSG_HELP_KEY_SCROLL_UP));
  printf("%s", TR(MSG_HELP_KEY_SCROLL_DOWN));
  printf("%s", TR(MSG_HELP_EXAMPLES));
  printf(TR(MSG_HELP_EX_NEW), prog);
  printf(TR(MSG_HELP_EX_LIST), prog);
  printf(TR(MSG_HELP_EX_ATTACH), prog);
  printf(TR(MSG_HELP_EX_KILL), prog);
}

int main(int argc, char *argv[]) {
  i18n_init();

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
  if (argc == 3 && (strcmp(argv[1], "-k") == 0 || strcmp(argv[1], "-K") == 0)) {
    kill_session_id = strtol(argv[2], NULL, 10);
    log_info("killing session id=%d\n", kill_session_id);
  }
  uid_t uid = getuid();

  // 创建目录
  char dir[MINI_TMUX_BUF_SMALL] = {0};
  snprintf(dir, sizeof(dir), "%smini-tmux-%d", MINI_TMUX_SOCK, uid);
  if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
    perror(TR(MSG_ERR_MKDIR));
    return -1;
  }
  struct stat info;
  if (lstat(dir, &info) != 0 || !(info.st_mode & S_IFDIR)) {
    perror(TR(MSG_ERR_STAT));
    return -1;
  }

  // socket_path 指向目录内的文件
  static char buf[MINI_TMUX_BUF_SMALL] = {0};
  snprintf(buf, sizeof(buf), "%s/default", dir);
  socket_path = buf;
  client_init(&client);
  if (client_main(&client) < 0) {
    return -1;
  }
  return 0;
}