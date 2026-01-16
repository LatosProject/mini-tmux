#define _XOPEN_SOURCE 600
#include "client.h"
#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

volatile sig_atomic_t sigwinch_pending,
    sigchld_pending = 0; // C 语言唯一保证信号读写安全的类型
struct client client;
int status;
/*
 * Signal handlers are async and may interrupt execution at arbitrary points.
 * Only set flags here; handle logic in main loop.
 */
void signal_handler(int sig) {
  int ret;
  switch (sig) {
  case SIGWINCH:
    sigwinch_pending = 1;
    break;
  // 回收子进程
  case SIGCHLD:
    ret = waitpid(client.slave_pid, &status, WNOHANG);
    if (ret > 0) {
      sigchld_pending = 1;
    }
    break;
  }
}

int main() {
  client_init(&client);
  client.master_fd = posix_openpt(O_RDWR);
  if (client.master_fd == -1) {
    perror("posix_openpt");
    return -1;
  }
  // 解锁 slave 设备
  grantpt(client.master_fd);
  unlockpt(client.master_fd);

  client.slave_name = ptsname(client.master_fd);
  client.slave_fd = open(client.slave_name, O_RDWR);

  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &client.ws) == -1) {
    return -1;
  }

  // 第一次初始化窗口尺寸
  ioctl(client.slave_fd, TIOCSWINSZ, &client.ws);

  client.slave_pid = spawn_child(&client);
  if (client.slave_pid < 0) {
    return -1;
  }

  // 终端窗口尺寸更新
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  dispatch_event(&client, EV_ENABLE_RAW_MODE);
  printf("Spawned child process with PID: %d\n", client.slave_pid);
  while (1) {
    if (client.child_exited)
      break;
    fd_set rfds;

    // 输入和输出
    int maxfd;
    FD_ZERO(&rfds);
    FD_SET(client.master_fd, &rfds);
    FD_SET(STDIN_FILENO, &rfds);

    maxfd = client.master_fd > STDIN_FILENO ? client.master_fd : STDIN_FILENO;

    int select_ok = 1;
    if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
      // 防止收到信号后中断 fd
      if (errno != EINTR) {
        dispatch_event(&client, EV_INTERRUPT);
        perror("select");
        break;
      }
      // fd 检查完毕
      select_ok = 0;
    }

    if (sigwinch_pending) {
      sigwinch_pending = 0;
      dispatch_event(&client, EV_WINCH);
    }

    if (sigchld_pending) {
      sigchld_pending = 0;
      dispatch_event(&client, EV_CHLD_EXIT);
    }

    // 只有 select 成功时才检查 fd
    if (select_ok) {
      if (FD_ISSET(client.master_fd, &rfds)) {
        dispatch_event(&client, EV_PTY_READ);
      }

      if (FD_ISSET(STDIN_FILENO, &rfds)) {
        dispatch_event(&client, EV_STDIN_READ);
      }
    }
  }
  return 0;
}