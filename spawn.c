#include "client.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <termios.h>
extern char **environ;

pid_t spawn_child(char *slave_name, int slave_fd, struct winsize *ws)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        char *args[] = {"/bin/bash", "-i", NULL};

        // 创建了一个会话
        setsid();

        slave_fd = open(slave_name, O_RDWR);

        // 把 slave 设为子进程的控制终端
        ioctl(slave_fd, TIOCSCTTY, 0);

        setenv("TERM", "screen-256color", 1);

        struct termios ts;

        tcsetpgrp(slave_fd, getpid()); // 设置前台进程组。这样，终端设备驱动程序就能了解将终端输入和终端产生的信号送到何处。

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);
        execve(args[0], args, environ);
        perror("Execve failed");
        _exit(1); // Use _exit to avoid flushing stdio buffers again
    }
    return pid;
}
