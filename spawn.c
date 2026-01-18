#include "server.h"
#include "util.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
extern char **environ;

pid_t spawn_child(struct session *s) {

  pid_t pid = fork();
  if (pid < 0) {
    perror("Fork failed");
    exit(1);
  } else if (pid == 0) {
    const char *shell = getshell();
    char *args[] = {(char *)shell, NULL};

    // 创建了一个会话
    setsid();

    s->slave_fd = open(*&s->slave_name, O_RDWR);
    if (s->slave_fd < 0) {
      perror("open slave pty failed");
      _exit(1);
    }
    // 把 slave 设为子进程的控制终端
    ioctl(s->slave_fd, TIOCSCTTY, 0);

    setenv("TERM", "screen-256color", 1);

    char buf[100];
    snprintf(buf, sizeof(buf), "%d", s->slave_pid);

    setenv("MINI_TMUX", buf, 1);
    struct termios ts;

    tcsetpgrp(
        s->slave_fd,
        getpid()); // 设置前台进程组。这样，终端设备驱动程序就能了解将终端输入和终端产生的信号送到何处。

    dup2(s->slave_fd, STDIN_FILENO);
    dup2(s->slave_fd, STDOUT_FILENO);
    dup2(s->slave_fd, STDERR_FILENO);
    close(s->slave_fd);
    execve(args[0], args, environ);
    perror("Execve failed");
    _exit(1); // Use _exit to avoid flushing stdio buffers again
  }
  return pid;
}
