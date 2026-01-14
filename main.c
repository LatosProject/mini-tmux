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
#include <string.h>

int master_fd;
int slave_fd;
int child_exited;
pid_t slave_pid;
struct winsize ws;
struct termios orig_termios;

void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGWINCH:
        // 设置终端尺寸
        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
        {
            return;
        }
        ioctl(slave_fd, TIOCSWINSZ, &ws);
        break;
    case SIGCHLD:
        int ret = waitpid(slave_pid, NULL, WNOHANG);
        if (ret > 0)
        {
            child_exited = 1;
            char msg[100] = {0};
            snprintf(msg, ret, "Child exited with PID: %d\n", ret);
            write(STDOUT_FILENO, msg, strlen(msg));
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
        break;
    }
}

void enable_raw_mode()
{
    // 原始终端切换至 raw 模式
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG); // 关掉回显/ 立即读取 / 禁用SIGINT
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main()
{
    master_fd = posix_openpt(O_RDWR);
    if (master_fd == -1)
    {
        perror("posix_openpt");
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
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGWINCH, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    // 原始终端属性备份
    tcgetattr(STDIN_FILENO, &orig_termios);

    enable_raw_mode();

    printf("Spawned child process with PID: %d\n", slave_pid);
    while (1)
    {
        if (child_exited)
            break;
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