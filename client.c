#include "mini_tmux-protocol.h"
#define _GNU_SOURCE
#include "client.h"
#include "log.h"
#include "main.h"
#include "server.h"
#include "spawn.h"
#include "util.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
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
    {ST_RUNNING, EV_EOF_PTY, ST_EXITING, act_child_exit},
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
  log_warn("FSM unhandled event %d in state %d", ev, c->state);
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
    // ret = waitpid(client.slave_pid, &status, WNOHANG);
    if (ret > 0) {
      sigchld_pending = 1;
    }
    break;
  }
}

void client_init(struct client *c) {
  c->state = ST_BOOT;
  c->server_fd = -1;
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
    FD_SET(c->server_fd, &rfds); // 监听 server 连接

    maxfd = c->master_fd > STDIN_FILENO ? c->master_fd : STDIN_FILENO;
    if (c->server_fd > maxfd)
      maxfd = c->server_fd;

    int select_ok = 1;
    if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
      // 防止收到信号后中断 fd
      if (errno != EINTR) {
        dispatch_event(c, EV_INTERRUPT);
        log_error("select failed: %s", strerror(errno));
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
      // server 关闭连接，说明 session 结束
      if (FD_ISSET(c->server_fd, &rfds)) {
        char buf[1];
        if (read(c->server_fd, buf, 1) <= 0) {
          dispatch_event(c, EV_EOF_PTY);
        }
      }

      if (FD_ISSET(c->master_fd, &rfds)) {
        dispatch_event(c, EV_PTY_READ);
      }

      if (FD_ISSET(STDIN_FILENO, &rfds)) {
        dispatch_event(c, EV_STDIN_READ);
      }
    }
  }
}
static int client_get_lock(char *lockfile) {
  int lockfd;
  log_debug("lock file is %s", lockfile);

  if ((lockfd = open(lockfile, O_RDWR | O_CREAT, 0600)) == -1) {
    log_error("open lock file failed: %s", strerror(errno));
    return -1;
  }

  if (flock(lockfd, LOCK_EX | LOCK_NB) == -1) {
    log_debug("flock failed: %s", strerror(errno));
    if (errno != EAGAIN)
      return lockfd;
    // 信号阻塞等待
    while (flock(lockfd, LOCK_EX) == -1 && errno == EINTR)
      ;
    close(lockfd);
    return -2;
  }
  log_debug("flock succeeded");
  return lockfd;
}
static int client_connect(const char *path) {
  struct sockaddr_un sa;
  int fd, lockfd = -1;
  int locked = 0;
  char buf[100] = {0};
  char *lockfile = NULL;

  // 将一段内存全部设置为指定的字节值
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strlcpy(sa.sun_path, path, sizeof(sa.sun_path));

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    log_error("socket failed: %s", strerror(errno));
    return -1;
  }
  log_debug("socket path is %s", path);
  log_debug("trying connect");
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    log_debug("connect failed: %s", strerror(errno));
    close(fd);
    if (!locked) {
      snprintf(buf, sizeof(buf), "%s.lock", path);
      lockfile = buf;
      if ((lockfd = client_get_lock(lockfile)) < 0) {
        log_debug("didn't get lock %d", lockfd);
      }
    }
    // lock标识符存在，无法删除目录项，且失败原因不是文件夹不存在
    if (lockfd >= 0 && unlink(path) != 0 && errno != ENOENT) {
      close(lockfd);
      return -1;
    }
    log_debug("got lock %d", lockfd);
    // 连接失败后新建
    fd = server_start();
  } else {
    log_debug("connected sucessfully");
  }
  if (locked && lockfd >= 0) {
    close(lockfd);
  }
  return fd;
}

int send_server(enum msgtype type, int fd, const void *buf, size_t len) {
  struct msg_header hdr = {type, len};
  ssize_t n;
  const char *ph = (const char *)&hdr;
  // 发送 header
  size_t sent = 0;
  while (sent < sizeof(hdr)) {
    ssize_t n = write(fd, ph + sent, sizeof(hdr) - sent);
    if (n == -1) {
      if (errno == EINTR)
        continue; // 被信号打断，重试
      return -1;
    }
    sent += n;
  }

  // 发送数据
  const char *p = buf;
  sent = 0;
  while (sent < len) {
    ssize_t n = write(fd, p + sent, len - sent);
    if (n == -1) {
      if (errno == EINTR)
        continue; // 被信号打断，重试
      return -1;
    }
    sent += n;
  }
  return 0;
}

int client_main(struct client *c) {
  log_init("client");
  log_info("client starting");

  int fd;
  fd = client_connect(socket_path);
  if (fd == -1) {
    log_error("client connect failed");
    return -1;
  }
  log_info("connected to server, fd %d", fd);

  // 保存 server 连接 fd
  c->server_fd = fd;

  // 创建新session
  char buf[100] = "new-session";
  send_server(MSG_RESIZE, fd, &c->ws, sizeof(c->ws));
  send_server(MSG_COMMAND, fd, buf, strlen(buf) + 1);

  // 获取 server 主进程fd
  c->master_fd = recv_fd(fd);
  if (c->master_fd == -1) {
    log_error("recv_fd failed");
    return -1;
  }

  // 终端窗口尺寸更新
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = SA_RESTART; // restart就是收到信号打断后，重新执行被打断的函数
  sigemptyset(&sa.sa_mask);
  sigaction(SIGWINCH, &sa, NULL);
  sigaction(SIGCHLD, &sa, NULL);

  dispatch_event(c, EV_ENABLE_RAW_MODE);

  log_info("entering client loop");
  client_loop(c);

  snprintf(buf, sizeof(buf), "%d", c->slave_pid);
  send_server(MSG_EXITED, fd, buf, strlen(buf) + 1);
  log_info("client exiting");
  log_close();
  return 0;
}
