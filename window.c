#include "window.h"
#include "list.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// vterm 输出回调 - 将终端响应发送回 PTY
static void vterm_output_callback(const char *s, size_t len, void *user) {
  struct window_pane *p = user;
  if (p->master_fd >= 0) {
    write(p->master_fd, s, len);
  }
}

void pane_set_master_fd(struct window_pane *p, int fd) {
  if (!p) return;
  p->master_fd = fd;
  if (p->vt) {
    vterm_output_set_callback(p->vt, vterm_output_callback, p);
  }
}

void pane_resize(struct window_pane *p, unsigned int sx, unsigned int sy) {
  if (!p || !p->grid)
    return;
  struct cell *new_cells = calloc(sx * sy, sizeof(struct cell));
  if (!new_cells)
    return;
  for (unsigned int y = 0; y < p->grid->height && y < sy; y++) {
    unsigned int copy_width = (p->grid->width < sx) ? p->grid->width : sx;
    memcpy(&new_cells[y * sx], &p->grid->cells[y * p->grid->width],
           copy_width * sizeof(struct cell));
  }

  free(p->grid->cells);
  p->grid->cells = new_cells;
  p->grid->width = sx;
  p->grid->height = sy;
  p->sx = sx;
  p->sy = sy;

  // 同步 libvterm 尺寸
  if (p->vt) {
    vterm_set_size(p->vt, sy, sx);
  }

  if (p->cx >= sx)
    p->cx = sx - 1;
  if (p->cy >= sy)
    p->cy = sy - 1;
}
struct window *window_create(const char *name) {
  struct window *w = calloc(1, sizeof(*w));
  if (!w)
    return NULL;

  list_init(&w->panes);
  w->name = name ? strdup(name) : NULL;

  return w;
}
struct window_pane *pane_create(struct window *w, unsigned int sx,
                                unsigned int sy, unsigned int xoff,
                                unsigned int yoff) {
  struct window_pane *p = calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  p->sx = sx;
  p->sy = sy;
  p->xoff = xoff;
  p->yoff = yoff;
  p->cx = 0;
  p->cy = 0;
  p->window = w;

  p->grid = calloc(1, sizeof(*p->grid));
  if (p->grid) {
    p->grid->width = sx;
    p->grid->height = sy;
    p->grid->cells = calloc(sx * sy, sizeof(struct cell));
  }

  // 初始化 libvterm
  p->vt = vterm_new(sy, sx);
  if (p->vt) {
    vterm_set_utf8(p->vt, 1);
    p->vts = vterm_obtain_screen(p->vt);
    vterm_screen_enable_altscreen(p->vts, 1);  // 启用备用屏幕
    vterm_screen_reset(p->vts, 1);
  }

  list_add_tail(&p->link, &w->panes);
  return p;
}

void pane_destroy(struct window_pane *p) {
  if (!p)
    return;
  if (p->vt)
    vterm_free(p->vt);
  if (p->grid) {
    free(p->grid->cells);
    free(p->grid);
  }
  free(p);
}