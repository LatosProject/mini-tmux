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

int main()
{
    int master_fd = posix_openpt(O_RDWR);
    if (master_fd == -1)
    {
        perror("opsix_openpt");
        return -1;
    }
    // Unlock the slave device.
    grantpt(master_fd);
    unlockpt(master_fd);

    char *slave_name = ptsname(master_fd);

    pid_t slave_pid = spawn_child(slave_name);
    printf("Spawned child process with PID: %d\n", slave_pid);
    while (1)
    {
        int status = 0;
        pid_t return_id = waitpid(slave_pid, &status, WNOHANG);
        if (return_id < 0)
        {
            perror("waitpid failed");
            exit(1);
        }
        fd_set rfds;

        // Input and output
        int maxfd;
        char buff[4096];
        FD_ZERO(&rfds);
        FD_SET(master_fd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);

        maxfd = master_fd > STDIN_FILENO ? master_fd : STDIN_FILENO;

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
        {
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