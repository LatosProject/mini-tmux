#ifndef SPAWN_H
#define SPAWN_H

#include <sys/types.h>
#include <sys/ioctl.h>

struct session;

pid_t spawn_child(struct session *s);

#endif /* SPAWN_H */
