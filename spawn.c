#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <termios.h>
extern int slave_fd;
extern char **environ;

pid_t spawn_child(char *slave_name, struct winsize *ws)
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
        setsid();

        // slave_fd = open(slave_name, O_RDWR);

        // 把 slave 设为子进程的控制终端
        ioctl(slave_fd, TIOCSCTTY, 0);

        setenv("TERM", "screen-256color", 1);

        struct termios ts;
        tcgetattr(slave_fd, &ts);
        ts.c_lflag &= ~ECHO; // 关闭回显
        tcsetattr(slave_fd, TCSANOW, &ts);

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
