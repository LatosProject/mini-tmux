#define _XOPEN_SOURCE 600
#include "spawn.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

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
        // fd_set rfds;
        // FD_ZERO(&rfds);
        // FD_SET(master_fd, &rfds);
        sleep(1);
    }
    return 0;
}