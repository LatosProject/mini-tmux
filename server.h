#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include "list.h"
int server_start() ;
struct session {
  int id;
  int master_fd;
  int slave_fd;
  pid_t slave_pid;
  struct winsize ws;
  struct termios orig_termios;
  int child_exited;
  struct termios raw;
  char *slave_name;
  struct environ *environ;

  struct list_head link;
};