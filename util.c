#include "util.h"
#include "main.h"
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>
struct passwd *pw;

int checkshell(const char *shell) {
  if (shell == NULL || *shell != '/') {
    return 0;
  }
  if (access(shell, X_OK) != 0) {
    return 0;
  }
  return 1;
}

const char *getshell() {
  const char *shell;
#ifndef MINI_TMUX_DEFAULT_SHELL
  shell = getenv("SHELL");
  if (checkshell(shell)) {
    return shell;
  }

  pw = getpwuid(getuid());
  if (pw != NULL && checkshell(shell)) {
    return pw->pw_shell;
  }
#endif
  return (_PATH_BSHELL);
}

struct environ_entry *environ_find(const char *name,
                                   struct environ_entry *out) {

  out->name = (char *)name;
  out->value = getenv(name);
  return out;
}

int client_check_nested() {
  struct environ_entry out;
  struct environ_entry *envent = environ_find("MINI_TMUX", &out);

  if (envent == NULL || envent->value == NULL || *envent->value == '\0')
    return (0);
  return (1);
}

// fdpass.c
int send_fd(int sock, int fd) {
  struct msghdr msg = {0};
  struct iovec iov[1];
  char buf[1] = {0};

  // 辅助数据缓冲区，存放要传递的 fd
  char cmsgbuf[CMSG_SPACE(sizeof(int))];

  iov[0].iov_base = buf;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS; // 传递文件描述符
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *(int *)CMSG_DATA(cmsg) = fd; // 把 fd 放进去

  return sendmsg(sock, &msg, 0) >= 0 ? 0 : -1; // fd内核检查
}

int recv_fd(int sock) {
  struct msghdr msg = {0};
  struct iovec iov[1];
  char buf[1];
  char cmsgbuf[CMSG_SPACE(sizeof(int))];

  iov[0].iov_base = buf;
  iov[0].iov_len = 1;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsgbuf;
  msg.msg_controllen = sizeof(cmsgbuf);

  if (recvmsg(sock, &msg, 0) < 0) // fd内核验证
    return -1;

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (cmsg && cmsg->cmsg_type == SCM_RIGHTS) {
    return *(int *)CMSG_DATA(cmsg); // 提取接收到的 fd
  }
  return -1;
}
