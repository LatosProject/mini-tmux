#include "client.h"
#include "log.h"
#include "render.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MAX_KEYBINDS 16
extern char *socket_path;
enum key_table { KEY_PREFIX };
struct keybind {
  char key;
  enum key_table table;
  void (*handler)(struct client *c);
};

struct action_map {
  const char *name;
  void (*handler)(struct client *c);
};

void detach_session(struct client *c) { dispatch_event(c, EV_DETACHED); }
void new_pane(struct client *c) { dispatch_event(c, EV_PANE_SPLIT); }
void next_pane(struct client *c) {
  struct window_pane *next =
      list_entry(c->pane->link.next, struct window_pane, link);
  if (&next->link == &c->pane->window->panes) {
    // 到达链表头，回到第一个 pane
    next = list_entry(c->pane->window->panes.next, struct window_pane, link);
  }
  c->pane = next;
}
void scroll_up(struct client *c) {
  if (c->pane && c->pane->grid) {
    grid_scroll_up(c->pane->grid, c->pane->sy); // 滚动一屏
    render_pane(c->pane);                       // 刷新显示
    render_status_bar(c);
  }
}
void scroll_down(struct client *c) {
  if (c->pane && c->pane->grid) {
    grid_scroll_down(c->pane->grid, c->pane->sy); // 滚动一屏
    render_pane(c->pane);                         // 刷新显示
    render_status_bar(c);
  }
}

static struct keybind keybinds[MAX_KEYBINDS];
struct action_map actions[] = {
    {"detach_session", detach_session}, {"new_pane", new_pane},
    {"next_pane", next_pane},           {"scroll_up", scroll_up},
    {"scroll_down", scroll_down},
};
int keybind_count = 0;

void handle_key(struct client *c, enum key_table table, char key) {
  int lower_key = (int)key;
  if (isalpha(key) && isupper(key)) {
    lower_key = tolower(key);
  }
  for (int i = 0; i < keybind_count; i++) {
    if (keybinds[i].table == table && keybinds[i].key == lower_key) {
      keybinds[i].handler(c);
      return;
    }
  }
  // 没有匹配的快捷键，发送 Ctrl+B + 原字符到 PTY
  char cb = 0x02;
  write(c->pane->master_fd, &cb, 1);
  write(c->pane->master_fd, &key, 1);
}
void keybind_init() {
  keybinds[keybind_count++] = (struct keybind){'d', KEY_PREFIX, detach_session};
  keybinds[keybind_count++] = (struct keybind){'%', KEY_PREFIX, new_pane};
  keybinds[keybind_count++] = (struct keybind){'o', KEY_PREFIX, next_pane};
  keybinds[keybind_count++] = (struct keybind){'[', KEY_PREFIX, scroll_up};
  keybinds[keybind_count++] = (struct keybind){']', KEY_PREFIX, scroll_down};

  // tmp/mini-tmux-1000/default -> /tmp/mini-tmux-1000/
  char dirpath[512];
  strncpy(dirpath, socket_path, sizeof(dirpath) - 1);
  dirpath[sizeof(dirpath) - 1] = '\0';
  char *last_slash = strrchr(dirpath, '/');
  if (last_slash) {
    *(last_slash + 1) = '\0';
  }

  char filename[512];
  snprintf(filename, sizeof(filename), "%skeybinds.conf", dirpath);
  log_debug("keybinds config: %s", filename);
  FILE *fp = fopen(filename, "r");
  if (!fp)
    return;

  char line[128];
  void (*handler)(struct client *c) = NULL;
  int n_actions = sizeof(actions) / sizeof(actions[0]);
  while (fgets(line, sizeof(line), fp)) {
    if (line[0] == '#') {
      continue;
    }
    char table_str[16], key_char[4], action_str[32];
    if (sscanf(line, "%15s %3s %31s", table_str, key_char, action_str) != 3) {
      continue;
    }
    char key = key_char[0];
    enum key_table table = KEY_PREFIX;
    for (int i = 0; i < n_actions; i++) {
      if (strcmp(action_str, actions[i].name) == 0) {
        for (int z = 0; z < keybind_count; z++) {
          if (actions[i].handler == keybinds[z].handler) {
            keybinds[z].key = key;
          }
        }
      }
    }
  }
  fclose(fp);
};