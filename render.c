#include "render.h"
#include "client.h"
#include "list.h"
#include "window.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CURSOR_HIDE "\033[?25l"
#define CURSOR_SHOW "\033[?25h"
void render_init(struct screen *s) {
  s->title = "\0";
  s->path = NULL;
  s->color = -1;
}

void screen_reinit(struct screen *s) {
  s->cx = 0;
  s->cy = 0;
  s->saved_cx = UINT_MAX;
  s->saved_cy = UINT_MAX;
}
void render_cleanup(struct screen *s) {
  s->title = NULL;
  s->path = NULL;
}
void render_screen(struct session *s) {
  struct window *w = s->active_window;
  write(STDOUT_FILENO, CURSOR_HIDE, strlen(CURSOR_HIDE));
  struct window_pane *p;
  // 从链头panes开始，每次取一个包含link的节点，返回给p
  list_for_each_entry(p, &w->panes, link) { render_pane(p); }
}
void render_pane(struct window_pane *p) {
  if (!p || !p->grid)
    return;
  // 隐藏光标
  write(STDOUT_FILENO, CURSOR_HIDE, 6);

  char buf[128];
  struct grid *g = p->grid;
  uint8_t last_fg = 0, last_bg = 0, last_attr = 0, last_flags = 0x03;

  // 重置颜色
  write(STDOUT_FILENO, "\033[0m", 4);

  for (unsigned int y = 0; y < p->sy; y++) {
    // ANSI 从 1,1 开始
    int len =
        snprintf(buf, sizeof(buf), "\033[%u;%uH", p->yoff + y + 1, p->xoff + 1);
    write(STDOUT_FILENO, buf, len);

    for (unsigned int x = 0; x < p->sx;) {
      struct cell *c = &g->cells[y * g->width + x];

      // 检查是否需要更新颜色/属性
      int need_update = (c->fg != last_fg || c->bg != last_bg ||
                         c->attr != last_attr || c->flags != last_flags);

      if (need_update) {
        // 先重置
        write(STDOUT_FILENO, "\033[0m", 4);
        // 设置属性
        if (c->attr & 0x01)
          write(STDOUT_FILENO, "\033[1m", 4); // bold
        if (c->attr & 0x02)
          write(STDOUT_FILENO, "\033[4m", 4); // underline
        if (c->attr & 0x04)
          write(STDOUT_FILENO, "\033[3m", 4); // italic
        if (c->attr & 0x08)
          write(STDOUT_FILENO, "\033[7m", 4); // reverse

        // 设置前景色 (非默认)
        if (!(c->flags & 0x01)) {
          len = snprintf(buf, sizeof(buf), "\033[38;5;%um", c->fg);
          write(STDOUT_FILENO, buf, len);
        }

        // 设置背景色 (非默认)
        if (!(c->flags & 0x02)) {
          len = snprintf(buf, sizeof(buf), "\033[48;5;%um", c->bg);
          write(STDOUT_FILENO, buf, len);
        }

        last_fg = c->fg;
        last_bg = c->bg;
        last_attr = c->attr;
        last_flags = c->flags;
      }

      if (c->ch[0]) {
        write(STDOUT_FILENO, c->ch, strlen(c->ch));
        // 宽字符占多列，跳过后续单元格
        x += (c->width > 0) ? c->width : 1;
      } else {
        write(STDOUT_FILENO, " ", 1);
        x++;
      }
    }
  }
  // 重置颜色
  write(STDOUT_FILENO, "\033[0m", 4);

  // 光标移动到 pane 内的正确位置 （vt解析）
  int clen = snprintf(buf, sizeof(buf), "\033[%u;%uH", p->yoff + p->cy + 1,
                      p->xoff + p->cx + 1);
  write(STDOUT_FILENO, buf, clen);

  // 显示光标
  write(STDOUT_FILENO, CURSOR_SHOW, 6);
}
void render_status_bar(struct client *c) {
  char buf[256];
  unsigned int row = c->ws.ws_row + 1; // 最后一行
  unsigned int cols = c->ws.ws_col;
  write(STDOUT_FILENO, CURSOR_HIDE, 6);
  // 移动到最后一行，蓝色背景白色文字
  int len = snprintf(buf, sizeof(buf), "\033[%u;1H\033[44;97m", row);
  write(STDOUT_FILENO, buf, len);

  // 写状态内容
  const char *wname = c->pane->window->name ? c->pane->window->name : "unnamed";
  len = snprintf(buf, sizeof(buf), " %s ", wname);
  write(STDOUT_FILENO, buf, len);

  // 用空格填满整行
  for (unsigned int i = strlen(buf); i < cols; i++) {
    if (i >= cols - 17) {
      int len = snprintf(buf, sizeof(buf), "mini-tmux v0.3.0");
      write(STDOUT_FILENO, buf, len);
      write(STDOUT_FILENO, " ", 1);
      break;
    }
    write(STDOUT_FILENO, " ", 1);
  }

  // 重置属性
  write(STDOUT_FILENO, "\033[0m", 4);
  write(STDOUT_FILENO, CURSOR_SHOW, 6);
}
void render_pane_borders(struct window_pane *p) {
  write(STDOUT_FILENO, CURSOR_HIDE, 6);
  char buf[256];
  for (unsigned int y = 0; y < p->sy; y++) {
    int len = snprintf(buf, sizeof(buf), "\033[%u;%uH\033[34m│\033[0m",
                       p->yoff + y + 1, p->xoff + p->sx + 1);
    write(STDOUT_FILENO, buf, len);
  }
  // 光标移动到 pane 内的正确位置 （vt解析）
  int clen = snprintf(buf, sizeof(buf), "\033[%u;%uH", p->yoff + p->cy + 1,
                      p->xoff + p->cx + 1);
  write(STDOUT_FILENO, buf, clen);
  write(STDOUT_FILENO, CURSOR_SHOW, 6);
}