#define _XOPEN_SOURCE 600
#include "spawn.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

int master_fd;
int slave_fd;
int child_exited;
pid_t slave_pid;
struct winsize ws;

void sigwinch_handler(int sig)
{
    // Read size
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
    {
        return;
    }
    // Write size
    ioctl(slave_fd, TIOCSWINSZ, &ws);
}

void sigchld_handler(int sig)
{
    int ret = waitpid(slave_pid, NULL, WNOHANG);
    if (ret > 0)
    {
        child_exited = 1;
        printf("Child exited with PID: %d \n", ret);
    }
}

int main()
{
    master_fd = posix_openpt(O_RDWR);
    if (master_fd == -1)
    {
        perror("opsix_openpt");
        return -1;
    }
    // 解锁 slave 设备
    grantpt(master_fd);
    unlockpt(master_fd);

    char *slave_name = ptsname(master_fd);

    slave_fd = open(slave_name, O_RDWR);

    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
    {
        return -1;
    }
    ioctl(slave_fd, TIOCSWINSZ, &ws);

    slave_pid = spawn_child(slave_name, &ws);

    // 终端窗口尺寸更新
    struct sigaction sa_winch;
    sa_winch.sa_handler = sigwinch_handler;
    sa_winch.sa_flags = SA_RESTART;
    sigemptyset(&sa_winch.sa_mask);
    sigaction(SIGWINCH, &sa_winch, NULL);

    // 回收子进程
    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sa_chld.sa_flags = SA_RESTART;
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);
    // signal(SIGWINCH, sigwinch_handler);

    printf("Spawned child process with PID: %d\n", slave_pid);
    while (1)
    {
        if (child_exited)
        {
            break;
        }
        // int status = 0;
        // pid_t return_id = waitpid(slave_pid, &status, WNOHANG);
        // if (return_id < 0)
        // {
        //     perror("waitpid failed");
        //     exit(1);
        // }
        fd_set rfds;

        // 输入和输出
        int maxfd;
        char buff[4096];
        FD_ZERO(&rfds);
        FD_SET(master_fd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);

        maxfd = master_fd > STDIN_FILENO ? master_fd : STDIN_FILENO;

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            // 当进程接收到信号时，会打断正在堵塞的 select()
            perror("select");
            break;
        }

        if (FD_ISSET(master_fd, &rfds))
        {
            ssize_t n = read(master_fd, buff, sizeof(buff));
            if (n <= 0)
            {
                break;
            }
            write(STDOUT_FILENO, buff, n);
        }

        if (FD_ISSET(STDIN_FILENO, &rfds))
        {
            ssize_t n = read(STDIN_FILENO, buff, sizeof(buff));
            if (n <= 0)
            {
                break;
            }
            write(master_fd, buff, n);
        }
    }
    return 0;
}