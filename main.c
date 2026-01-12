#include "spawn.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>

int main()
{
    pid_t child_pid = spawn_child();
    printf("Spawned child process with PID: %d\n", child_pid);
    while (1)
    {
        int status = 0;
        pid_t return_id = waitpid(child_pid, &status, WNOHANG);
        if (return_id < 0)
        {
            perror("waitpid failed");
            exit(1);
        }
        sleep(1);
    }
    return 0;
}