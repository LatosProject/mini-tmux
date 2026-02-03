#include "window.h"
#include "list.h"
#include "render.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Unicode codepoint 转 UTF-8
static int cp_to_utf8(uint32_t cp, char *buf) {
  if (cp < 0x80) {
    buf[0] = cp;
    buf[1] = 0;
    return 1;
  } else if (cp < 0x800) {
    buf[0] = 0xC0 | (cp >> 6);
    buf[1] = 0x80 | (cp & 0x3F);
    buf[2] = 0;
    return 2;
  } else if (cp < 0x10000) {
    buf[0] = 0xE0 | (cp >> 12);
    buf[1] = 0x80 | ((cp >> 6) & 0x3F);
    buf[2] = 0x80 | (cp & 0x3F);
    buf[3] = 0;
    return 3;
  } else {
    buf[0] = 0xF0 | (cp >> 18);
    buf[1] = 0x80 | ((cp >> 12) & 0x3F);
    buf[2] = 0x80 | ((cp >> 6) & 0x3F);
    buf[3] = 0x80 | (cp & 0x3F);
    buf[4] = 0;
    return 4;
  }
}

// vterm 屏幕滚动回调
static int screen_sb_pushline(int cols, const VTermScreenCell *cells,
                              void *user) {
  struct window_pane *p = user;
  if (!p || !p->grid || !p->grid->history_cells)
    return 0;

  struct grid *g = p->grid;
  unsigned int dst_line = g->history_count % g->history_size;
  struct cell *dst = &g->history_cells[dst_line * g->width];

  // libvterm 提供的 cells 复制
  for (unsigned int x = 0; x < g->width && (int)x < cols; x++) {
    const VTermScreenCell *vc = &cells[x];
    struct cell *c = &dst[x];
    if (vc->chars[0]) {
      cp_to_utf8(vc->chars[0], c->ch);
    } else {
      c->ch[0] = ' ';
      c->ch[1] = 0;
    }
    c->width = vc->width ? vc->width : 1;
    c->fg = VTERM_COLOR_IS_INDEXED(&vc->fg) ? vc->fg.indexed.idx : 0;
    c->bg = VTERM_COLOR_IS_INDEXED(&vc->bg) ? vc->bg.indexed.idx : 0;
    c->flags = (VTERM_COLOR_IS_DEFAULT_FG(&vc->fg) ? 0x01 : 0) |
               (VTERM_COLOR_IS_DEFAULT_BG(&vc->bg) ? 0x02 : 0);
    c->attr = (vc->attrs.bold ? 0x01 : 0) | (vc->attrs.underline ? 0x02 : 0) |
              (vc->attrs.italic ? 0x04 : 0) | (vc->attrs.reverse ? 0x08 : 0);
  }
  g->history_count++;
  return 0;
}

static VTermScreenCallbacks screen_callbacks = {
    .sb_pushline = screen_sb_pushline,
};

// vterm 输出回调 - 将终端响应发送回 PTY
static void vterm_output_callback(const char *s, size_t len, void *user) {
  struct window_pane *p = user;
  if (p->master_fd >= 0) {
    write(p->master_fd, s, len);
  }
}

void pane_set_master_fd(struct window_pane *p, int fd) {
  if (!p)
    return;
  p->master_fd = fd;
  if (p->vt) {
    /* 通过 vterm_input_write() 喂给 vterm 的数据会经过 vterm_output_callback
       发送到 master_fd */
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

void window_destroy(struct window *w) {
  if (!w)
    return;
  free(w->name);
  free(w);
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
  if (!p->grid) {
    free(p);
    return NULL;
  }
  if (p->grid) {
    p->grid->width = sx;
    p->grid->height = sy;
    p->grid->cells = calloc(sx * sy, sizeof(struct cell));
    grid_init_history(p->grid, 1000); // 初始化历史缓冲区
  }

  // 初始化 libvterm
  p->vt = vterm_new(sy, sx);
  if (p->vt) {
    vterm_set_utf8(p->vt, 1); // 设置输入为 UTF-8 编码
    p->vts = vterm_obtain_screen(
        p->vt); // 初始化screen 屏幕单元格内容（字符+颜色+属性）
    vterm_screen_enable_altscreen(p->vts,
                                  1); // 启用备用屏幕（维护两个屏幕缓冲区）
    vterm_screen_set_callbacks(p->vts, &screen_callbacks, p); // 设置滚动回调
    vterm_screen_reset(p->vts, 1);                            // 初始化内存
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
    grid_free_history(p->grid); // 释放历史
    free(p->grid->cells);
    free(p->grid);
  }
  free(p);
}