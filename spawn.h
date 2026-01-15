#include <sys/types.h>
#include <sys/ioctl.h>

pid_t spawn_child(char *slave_name, int slave_fd, struct winsize *ws);