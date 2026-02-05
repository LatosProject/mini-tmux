#ifndef RENDER_H
#define RENDER_H

#include "server.h"
#include "window.h"
#include <stdint.h>
#include <sys/types.h>
struct session;
struct window_pane;
struct client;
struct cell {
  char ch[5];    // UTF-8 字符 (最多4字节 + null)
  uint8_t width; // 显示宽度 (1 或 2)
  uint8_t fg;    // 前景色索引 (0-255)
  uint8_t bg;    // 背景色索引 (0-255)
  uint8_t attr;  // 属性 (bold, underline 等)
  uint8_t flags; // 标志位: bit0=默认fg, bit1=默认bg
};

// 屏幕网格
struct grid {
  struct cell *cells; // cells[y * width + x]
  unsigned int width;
  unsigned int height;

  struct cell *history_cells;
  unsigned int history_size;
  unsigned int history_count;
  unsigned int scroll_offset;
};

struct screen {
  char *title;
  char *path;
  unsigned int cx; // 光标 x
  unsigned int cy; // 光标 y
  int color;
  unsigned int saved_cx; // 保存的光标位置（用于 ESC 7/8）
  unsigned int saved_cy;
};

// 函数声明
void render_init(struct screen *s);
void screen_reinit(struct screen *s);
void render_cleanup(struct screen *s);
void render_screen(struct session *s);
void render_pane(struct window_pane *p);
void render_status_bar(struct client *c);
void render_pane_borders(struct window_pane *w);

// 历史管理函数
void grid_init_history(struct grid *g, unsigned int max_lines);
void grid_free_history(struct grid *g);
void grid_push_line_to_history(struct grid *g, unsigned int line);
void grid_scroll_up(struct grid *g, unsigned int lines);
void grid_scroll_down(struct grid *g, unsigned int lines);
struct cell *grid_get_display_line(struct grid *g, unsigned int y);
size_t grid_serialize(struct grid *g, unsigned int pane_id,
                      unsigned int cx, unsigned int cy, void **out_buf);
int grid_deserialize(struct grid *g, unsigned int *pane_id,
                     unsigned int *cx, unsigned int *cy,
                     const void *buf, size_t len);

#endif /* RENDER_H */