#include <sys/types.h>
#include <sys/ioctl.h>

pid_t spawn_child(char *slave_name, struct winsize *ws);