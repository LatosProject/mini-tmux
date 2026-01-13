#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

pid_t spawn_child(char *slave_name)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        char *args[] = {"/bin/bash", NULL};
        setsid();
        int slave_fd = open(slave_name, O_RDWR);
        ioctl(slave_fd, TIOCSCTTY, 0);

        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        close(slave_fd);
        execve(args[0], args, NULL);
        perror("Execve failed");
        _exit(1); // Use _exit to avoid flushing stdio buffers again
    }
    return pid;
}
