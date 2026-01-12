#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t spawn_child()
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
        execve(args[0], args, NULL);
        perror("Execve failed");
        _exit(1); // Use _exit to avoid flushing stdio buffers again
    }
    return pid;
}
