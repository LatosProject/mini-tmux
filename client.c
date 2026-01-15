#include "client.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

static const state_transition table[] = {
    {ST_BOOT, EV_ENABLE_RAW_MODE, ST_RUNNING, act_enbale_raw_mode},
    {ST_RUNNING, EV_WINCH, ST_RUNNING, act_resize},
    {ST_RUNNING, EV_CHLD_EXIT, ST_EXITING, act_child_exit},
    {ST_RUNNING, EV_PTY_READ, ST_RUNNING, act_pty_read},
    {ST_RUNNING, EV_STDIN_READ, ST_RUNNING, act_stdin_read},
    {ST_EXITING, EV_STDIN_READ, ST_EXITING, NULL},
    {ST_EXITING, EV_PTY_READ, ST_EXITING, NULL}};

#define NTRANS (sizeof(table) / sizeof(table[0]))

void dispatch_event(struct client *c, client_event ev)
{
    for (size_t i = 0; i < NTRANS; i++)
    {
        if (table[i].state == c->state && table[i].event == ev)
        {
            if (table[i].action)
            {
                table[i].action(c, ev);
            }

            c->state = table[i].next;
            return;
        }
    }
    fprintf(stderr,
            "[FSM] unhandled event %d in state %d\n",
            ev, c->state);
}

void act_resize(struct client *c, client_event ev)
{
    // 设置终端尺寸
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &(c->ws)) == -1)
    {
        return;
    }
    ioctl(c->slave_fd, TIOCSWINSZ, &(c->ws));
    return;
}

void act_child_exit(struct client *c, client_event ev)
{
    c->child_exited = 1;
    char msg[100] = {0};
    snprintf(msg, sizeof(msg), "Child exited with PID: %d\n", c->slave_pid);
    write(STDOUT_FILENO, msg, strlen(msg));
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->orig_termios));
}

void act_enbale_raw_mode(struct client *c, client_event ev)
{
    // 原始终端切换至 raw 模式
    tcgetattr(STDIN_FILENO, &(c->raw));
    c->raw.c_lflag &= ~(ECHO | ICANON | ISIG); // 关掉回显/ 立即读取 / 禁用SIGINT
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &(c->raw));
}

void act_pty_read(struct client *c, client_event ev)
{
    char buff[4096];
    ssize_t n = read(c->master_fd, buff, sizeof(buff));
    if (n <= 0)
    {
        return;
    }
    write(STDOUT_FILENO, buff, n);
}

void act_stdin_read(struct client *c, client_event ev)
{
    char buff[4096];
    ssize_t n = read(STDIN_FILENO, buff, sizeof(buff));
    if (n <= 0)
    {
        return;
    }
    write(c->master_fd, buff, n);
}

void client_init(struct client *c)
{
    c->state = ST_BOOT;
    c->master_fd = -1;
    c->slave_fd = -1;
    c->slave_pid = -1;
    c->child_exited = 0;

    tcgetattr(STDIN_FILENO, &(c->orig_termios));
    ioctl(STDIN_FILENO, TIOCGWINSZ, &(c->ws));
}
