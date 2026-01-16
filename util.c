#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <paths.h>

struct passwd *pw;

int checkshell(const char *shell)
{
    if (shell == NULL || *shell != '/')
    {
        return 0;
    }
    if (access(shell, X_OK) != 0)
    {
        return 0;
    }
    return 1;
}

static const char *getshell()
{
    const char *shell;
    shell = getenv("SHELL");
    if (checkshell(shell))
    {
        return shell;
    }

    pw = getpwuid(getuid());
    if (pw != NULL && checkshell(shell))
    {
        return pw->pw_shell;
    }

    return (_PATH_BSHELL);
}
