#ifndef WINDOW_H
#define WINDOW_H

#include "list.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "vterm.h"

struct grid; // 前向声明

struct window {
  struct list_head link;
  struct list_head panes; // 头
  unsigned int id;
  char *name;
  unsigned int active_point;
  int flags;
};

struct window_pane {
  struct list_head link;
  struct grid *grid;
  unsigned int cx, cy;
  unsigned int id;
  unsigned int active_point;
  struct winsize ws;
  struct window *window;
  int master_fd;
  int slave_fd;
  pid_t slave_pid;

  unsigned int sx; // size
  unsigned int sy;

  unsigned int xoff; // position
  unsigned int yoff;

  int child_exited;
  int flags;

  // libvterm
  VTerm *vt;
  VTermScreen *vts;
};

struct window *window_create(const char *name);
struct window_pane *pane_create(struct window *w, unsigned int sx, unsigned int sy,
                                unsigned int xoff, unsigned int yoff);
void pane_destroy(struct window_pane *p);
void pane_resize(struct window_pane *p, unsigned int sx, unsigned int sy);
void pane_set_master_fd(struct window_pane *p, int fd);

#endif /* WINDOW_H */