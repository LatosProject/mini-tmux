#include "log.h"
#include "mini_tmux-protocol.h"
#include <bits/types/sigset_t.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

extern char *socket_path;

ssize_t read_n(int fd, void *buf, size_t n) {
  size_t recvd = 0;
  char *p = buf;
  while (recvd < n) {
    ssize_t r = read(fd, p + recvd, n - recvd);
    if (r == 0)
      return 0; // EOF
    if (r == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    recvd += r;
  }
  return recvd;
}

void server_receive(int fd) {
  struct msg_header hdr;
  if (read_n(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    log_error("read header failed: %s", strerror(errno));
    return;
  }

  char *buf = malloc(hdr.len);
  if (read_n(fd, buf, hdr.len) != hdr.len) {
    log_error("read payload failed: %s", strerror(errno));
    free(buf);
    return;
  }

  switch (hdr.type) {
  case MSG_COMMAND:
    if (strcmp(buf, "new-session") == 0) {
      log_info("create a new session");
    }
    break;
  case MSG_EXITED:
    log_info("exit a session, pid:%d", buf);
  default:
    log_warn("unknown msgtype %d", hdr.type);
  }

  free(buf);
}

void server_loop(int listen_fd) {
  log_info("server loop started, listening on fd %d", listen_fd);
  while (1) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == -1) {
      if (errno == EINTR)
        continue;
      log_error("accept failed: %s", strerror(errno));
      break;
    }
    log_debug("accepted client fd %d", client_fd);
    server_receive(client_fd);
    close(client_fd);
  }
}

int server_start() {
  sigset_t set, oldset;
  log_info("server is starting");

  // 创建 Unix 域套接字
  int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    log_error("socket failed: %s", strerror(errno));
    return -1;
  }

  struct sockaddr_un sa;
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, socket_path, sizeof(sa.sun_path) - 1);

  // 绑定到路径
  if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    log_error("bind failed: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  log_debug("bound to %s", socket_path);

  if (listen(listen_fd, 5) == -1) {
    log_error("listen failed: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }

  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  pid_t pid = fork();
  if (pid < 0) {
    log_error("fork failed: %s", strerror(errno));
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    close(listen_fd);
    return -1;
  }
  if (pid == 0) {
    // 成为守护进程

    if (setsid() == -1) {
      log_error("setsid failed: %s", strerror(errno));
      _exit(1);
    }

    //  二次 fork，确保不能重新获取控制终端
    pid_t pid2 = fork();
    if (pid2 < 0) {
      _exit(1);
    }
    if (pid2 > 0) {
      // 第一个子进程退出，让子进程成为真正的守护进程
      _exit(0);
    }

    // 设置文件权限掩码
    umask(0);

    // 关闭标准输入输出，重定向到 /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY); // stdin  -> fd 0
    open("/dev/null", O_WRONLY); // stdout -> fd 1
    open("/dev/null", O_WRONLY); // stderr -> fd 2

    sigprocmask(SIG_SETMASK, &oldset, NULL);
    log_init("server");
    log_info("server daemon started, pid %d", getpid());
    server_loop(listen_fd);
    close(listen_fd);
    log_close();
    _exit(0);
  } else {
    // 等待第一个子进程退出（它会立即退出，孙进程继续运行）
    waitpid(pid, NULL, 0);
    // 父进程：client，连接到 server
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    close(listen_fd);

    // 连接到刚创建的 server
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
      log_error("client socket failed: %s", strerror(errno));
      return -1;
    }
    if (connect(client_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
      log_error("client connect to new server failed: %s", strerror(errno));
      close(client_fd);
      return -1;
    }
    log_debug("connected to server, fd %d", client_fd);
    return client_fd;
  }
}
