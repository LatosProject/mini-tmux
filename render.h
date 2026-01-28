#ifndef RENDER_H
#define RENDER_H

#include "server.h"
#include "window.h"
#include <stdint.h>
#include <sys/types.h>
// 前向声明（避免循环依赖）
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
void render_init(struct screen *s); // 缺少这个声明
void screen_reinit(struct screen *s);
void render_cleanup(struct screen *s);
void render_screen(struct session *s);
void render_pane(struct window_pane *p);
void render_status_bar(struct client *c);
void render_pane_borders(struct window_pane *w); // 参数类型应该是 window

#endif /* RENDER_H */