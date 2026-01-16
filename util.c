#include "util.h"
#include "client.h"
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
struct passwd *pw;

int checkshell(const char *shell) {
  if (shell == NULL || *shell != '/') {
    return 0;
  }
  if (access(shell, X_OK) != 0) {
    return 0;
  }
  return 1;
}

const char *getshell() {
  const char *shell;
  shell = getenv("SHELL");
  if (checkshell(shell)) {
    return shell;
  }

  pw = getpwuid(getuid());
  if (pw != NULL && checkshell(shell)) {
    return pw->pw_shell;
  }

  return (_PATH_BSHELL);
}

struct environ_entry *environ_find(const char *name,
                                   struct environ_entry *out) {

  out->name = (char *)name;
  out->value = getenv(name);
  return out;
}

int client_check_nested() {
  struct environ_entry out;
  struct environ_entry *envent = environ_find("MINI_TMUX", &out);

  if (envent == NULL || envent->value == NULL || *envent->value == '\0')
    return (0);
  return (1);
}
