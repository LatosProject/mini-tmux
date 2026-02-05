#include "render.h"
#include "window.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Unicode codepoint 转 UTF-8
static int unicode_to_utf8(uint32_t cp, char *buf) {
  if (cp < 0x80) {
    buf[0] = (char)cp;
    buf[1] = 0;
    return 1;
  } else if (cp < 0x800) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    buf[2] = 0;
    return 2;
  } else if (cp < 0x10000) {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    buf[3] = 0;
    return 3;
  } else if (cp < 0x110000) {
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    buf[4] = 0;
    return 4;
  }
  buf[0] = 0;
  return 0;
}

// 从 grid 同步屏幕内容到 VTerm
void sync_vterm_from_grid(struct window_pane *p) {
  if (!p->vt || !p->grid)
    return;

  struct grid *g = p->grid;
  char seq[64];
  int len;

  // 清屏并重置状态
  vterm_input_write(p->vt, "\033[H\033[2J\033[0m", 11);

  uint8_t last_fg = 0, last_bg = 0, last_attr = 0, last_flags = 0x03;
  unsigned int last_non_empty_y = 0;
  unsigned int last_non_empty_x = 0;

  for (unsigned int y = 0; y < g->height; y++) {
    len = snprintf(seq, sizeof(seq), "\033[%u;1H", y + 1);
    vterm_input_write(p->vt, seq, len);

    for (unsigned int x = 0; x < g->width;) {
      struct cell *c = &g->cells[y * g->width + x];

      // 只在属性变化时更新
      if (c->attr != last_attr || c->fg != last_fg || c->bg != last_bg ||
          c->flags != last_flags) {
        vterm_input_write(p->vt, "\033[0m", 4);
        if (c->attr & 0x01)
          vterm_input_write(p->vt, "\033[1m", 4);
        if (c->attr & 0x02)
          vterm_input_write(p->vt, "\033[4m", 4);
        if (c->attr & 0x04)
          vterm_input_write(p->vt, "\033[3m", 4);
        if (c->attr & 0x08)
          vterm_input_write(p->vt, "\033[7m", 4);
        if (!(c->flags & 0x01)) {
          len = snprintf(seq, sizeof(seq), "\033[38;5;%um", c->fg);
          vterm_input_write(p->vt, seq, len);
        }
        if (!(c->flags & 0x02)) {
          len = snprintf(seq, sizeof(seq), "\033[48;5;%um", c->bg);
          vterm_input_write(p->vt, seq, len);
        }
        last_attr = c->attr;
        last_fg = c->fg;
        last_bg = c->bg;
        last_flags = c->flags;
      }

      if (c->ch[0]) {
        vterm_input_write(p->vt, c->ch, strlen(c->ch));
        // 记录最后一个非空字符的位置
        last_non_empty_y = y;
        last_non_empty_x = x + ((c->width > 0) ? c->width : 1);
        x += (c->width > 0) ? c->width : 1;
      } else {
        vterm_input_write(p->vt, " ", 1);
        x++;
      }
    }
  }

  // 把光标移到最后一个非空行的末尾（而不是 p->cy, p->cx）
  // 这样 shell 发送 \033[J 时不会清除太多内容
  len = snprintf(seq, sizeof(seq), "\033[%u;%uH", last_non_empty_y + 1,
                 last_non_empty_x + 1);
  vterm_input_write(p->vt, seq, len);

  // 同时更新 pane 的光标位置
  p->cy = last_non_empty_y;
  p->cx = last_non_empty_x;
}

// 从 libvterm 同步屏幕内容到 grid
static void sync_grid_from_vterm(struct window_pane *p) {
  if (!p->vts || !p->grid)
    return;

  for (unsigned int y = 0; y < p->sy; y++) {
    for (unsigned int x = 0; x < p->sx; x++) {
      // 终端解析完成的数据
      VTermPos pos = {.row = y, .col = x};
      VTermScreenCell cell;
      memset(&cell, 0, sizeof(cell));
      vterm_screen_get_cell(p->vts, pos, &cell);

      // grid 中的单元格
      struct cell *c = &p->grid->cells[y * p->grid->width + x];
      memset(c, 0, sizeof(*c));

      if (cell.chars[0]) {
        unicode_to_utf8(cell.chars[0], c->ch);
      } else {
        c->ch[0] = 0;
      }
      c->width = cell.width; // 始终从 libvterm 获取宽度

      // 提取颜色
      c->flags = 0;
      if (VTERM_COLOR_IS_DEFAULT_FG(&cell.fg)) {
        c->flags |= 0x01; // 使用默认前景色
      } else if (VTERM_COLOR_IS_INDEXED(&cell.fg)) {
        c->fg = cell.fg.indexed.idx;
      } else if (VTERM_COLOR_IS_RGB(&cell.fg)) {
        // RGB 转 216 色立方体 + 灰度
        uint8_t r = cell.fg.rgb.red, g = cell.fg.rgb.green,
                b = cell.fg.rgb.blue;
        c->fg = 16 + (r / 51) * 36 + (g / 51) * 6 + (b / 51);
      }

      if (VTERM_COLOR_IS_DEFAULT_BG(&cell.bg)) {
        c->flags |= 0x02; // 使用默认背景色
      } else if (VTERM_COLOR_IS_INDEXED(&cell.bg)) {
        c->bg = cell.bg.indexed.idx;
      } else if (VTERM_COLOR_IS_RGB(&cell.bg)) {
        uint8_t r = cell.bg.rgb.red, g = cell.bg.rgb.green,
                b = cell.bg.rgb.blue;
        c->bg = 16 + (r / 51) * 36 + (g / 51) * 6 + (b / 51);
      }

      // 提取属性
      c->attr = 0;
      if (cell.attrs.bold)
        c->attr |= 0x01;
      if (cell.attrs.underline)
        c->attr |= 0x02;
      if (cell.attrs.italic)
        c->attr |= 0x04;
      if (cell.attrs.reverse)
        c->attr |= 0x08;
    }
  }

  // 同步光标位置
  VTermPos cursor;
  VTermState *state = vterm_obtain_state(p->vt); // state 状态机跟踪光标位置
  vterm_state_get_cursorpos(state, &cursor);     // 查询光标
  p->cx = (unsigned int)cursor.col;
  p->cy = (unsigned int)cursor.row;
}

void pane_input(struct window_pane *p, const char *data, size_t len) {
  if (!p->vt)
    return;

  vterm_input_write(p->vt, data, len);
  sync_grid_from_vterm(p);
}
