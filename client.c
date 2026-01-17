#include "client.h"
#include "main.h"
#include "spawn.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

extern char *socket_path;
volatile sig_atomic_t sigwinch_pending,
    sigchld_pending = 0; // C 语言唯一保证信号读写安全的类型
static const state_transition table[] = {
    {ST_BOOT, EV_ENABLE_RAW_MODE, ST_RUNNING, act_enable_raw_mode},
    {ST_RUNNING, EV_WINCH, ST_RUNNING, act_resize},
    {ST_RUNNING, EV_CHLD_EXIT, ST_EXITING, act_child_exit},
    {ST_RUNNING, EV_PTY_READ, ST_RUNNING, act_pty_read},
    {ST_RUNNING, EV_STDIN_READ, ST_RUNNING, act_stdin_read},
    {ST_EXITING, EV_STDIN_READ, ST_EXITING, NULL},
    {ST_EXITING, EV_PTY_READ, ST_EXITING, NULL},
    {ST_RUNNING, EV_EOF_PTY, ST_EXITING, NULL},
    {ST_RUNNING, EV_EOF_STDIN, ST_EXITING, NULL},
    {ST_RUNNING, EV_INTERRUPT, ST_EXITING, NULL}};

#define NTRANS (sizeof(table) / sizeof(table[0]))

void dispatch_event(struct client *c, client_event ev) {
  for (size_t i = 0; i < NTRANS; i++) {
    if (table[i].state == c->state && table[i].event == ev) {
      if (table[i].action) {
        table[i].action(c, ev);
      }

      c->state = table[i].next;
      return;
    }
  }
  fprintf(stderr, "[FSM] unhandled event %d in state %d\n", ev, c->state);
}

void act_resize(struct client *c, client_event ev) {
  // 设置终端尺寸
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &(c->ws)) == -1) {
    return;
  }
  ioctl(c->master_fd, TIOCSWINSZ, &(c->ws));
  return;
}

void act_child_exit(struct client *c, client_event ev) {
  c->child_exited = 1;
  char msg[100] = {0};
  snprintf(msg, sizeof(msg), "Child exited with PID: %d\n", c->slave_pid);
  write(STDOUT_FILENO, msg, strlen(msg));
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->orig_termios));
}

void act_enable_raw_mode(struct client *c, client_event ev) {
  // 原始终端切换至 raw 模式
  tcgetattr(STDIN_FILENO, &(c->raw));
  c->raw.c_lflag &= ~(ECHO | ICANON | ISIG); // 关掉回显/ 立即读取 / 禁用SIGINT
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->raw));
}

void act_pty_read(struct client *c, client_event ev) {
  char buff[4096];
  ssize_t n = read(c->master_fd, buff, sizeof(buff));
  if (n <= 0) {
    dispatch_event(c, EV_EOF_PTY);
    return;
  }
  write(STDOUT_FILENO, buff, n);
}

void act_stdin_read(struct client *c, client_event ev) {
  char buff[4096];
  ssize_t n = read(STDIN_FILENO, buff, sizeof(buff));
  if (n <= 0) {
    dispatch_event(c, EV_EOF_STDIN);
    return;
  }
  write(c->master_fd, buff, n);
}

/*
 * Signal handlers are async and may interrupt execution at arbitrary points.
 * Only set flags here; handle logic in main loop.
 */
void signal_handler(int sig) {
  extern struct client client;
  int status;
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

void client_init(struct client *c) {
  c->state = ST_BOOT;
  c->master_fd = -1;
  c->slave_fd = -1;
  c->slave_pid = -1;
  c->child_exited = 0;

  tcgetattr(STDIN_FILENO, &(c->orig_termios));
  ioctl(STDIN_FILENO, TIOCGWINSZ, &(c->ws));
}

void client_loop(struct client *c) {
  while (1) {
    if (c->child_exited)
      break;
    fd_set rfds;

    // 输入和输出
    int maxfd;
    FD_ZERO(&rfds);
    FD_SET(c->master_fd, &rfds);
    FD_SET(STDIN_FILENO, &rfds);

    maxfd = c->master_fd > STDIN_FILENO ? c->master_fd : STDIN_FILENO;

    int select_ok = 1;
    if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
      // 防止收到信号后中断 fd
      if (errno != EINTR) {
        dispatch_event(c, EV_INTERRUPT);
        perror("select");
        break;
      }
      // fd 检查完毕
      select_ok = 0;
    }

    if (sigwinch_pending) {
      sigwinch_pending = 0;
      dispatch_event(c, EV_WINCH);
    }

    if (sigchld_pending) {
      sigchld_pending = 0;
      dispatch_event(c, EV_CHLD_EXIT);
    }

    // 只有 select 成功时才检查 fd
    if (select_ok) {
      if (FD_ISSET(c->master_fd, &rfds)) {
        dispatch_event(c, EV_PTY_READ);
      }

      if (FD_ISSET(STDIN_FILENO, &rfds)) {
        dispatch_event(c, EV_STDIN_READ);
      }
    }
  }
}

static int client_connect(const char *path) {
  int fd = -1;
  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket failed");
    return -1;
  }
  // TODO
  printf("socket is %s\n", path);
  return 1;
}

int client_main(struct client *c) {
  client_connect(socket_path);
  c->master_fd = posix_openpt(O_RDWR);
  if (c->master_fd == -1) {
    perror("posix_openpt");
    return -1;
  }
  // 解锁 slave 设备
  grantpt(c->master_fd);
  unlockpt(c->master_fd);
  c->slave_name = ptsname(c->master_fd);
  c->slave_fd = open(c->slave_name, O_RDWR);
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &c->ws) == -1) {
    return -1;
  }
  ioctl(c->slave_fd, TIOCSWINSZ, &c->ws);
  c->slave_pid = spawn_child(c);
  if (c->slave_pid < 0) {
    return -1;
  }
  // 终端窗口尺寸更新
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  dispatch_event(c, EV_ENABLE_RAW_MODE);

  char msg[100] = {0};
  snprintf(msg, sizeof(msg), "Spawned child process with PID: %d\n",
           c->slave_pid);
  write(STDOUT_FILENO, msg, strlen(msg));
  client_loop(c);
  return 0;
}
