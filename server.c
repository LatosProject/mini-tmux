/* Fork new server. */
#include <bits/types/sigset_t.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int server_start() {
  int pair[2];
  sigset_t set, oldset;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) {
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    perror("socketpair failed");
    return -1;
  }
  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  pid_t pid = fork();
  if (pid < 0) {
    perror("Fork failed");
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    exit(1);
  }
  if (pid == 0) {
    // 子进程：server
    close(pair[0]);
    return pair[1];
  } else {
    // 父进程：client
    close(pair[1]);
    return pair[0];
  }
  sigprocmask(SIG_SETMASK, &oldset, NULL);
}
