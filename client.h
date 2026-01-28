// client.h
#ifndef CLIENT_H
#define CLIENT_H
#include "render.h"
#include "window.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
typedef enum {
  ST_BOOT,
  ST_RUNNING,
  ST_RESIZING,
  ST_EXITING,
} client_state;

typedef enum {
  EV_STDIN_READ,
  EV_PTY_READ,
  EV_WINCH,
  EV_CHLD_EXIT,
  EV_INTERRUPT,
  EV_EOF_STDIN,
  EV_EOF_PTY,
  EV_ENABLE_RAW_MODE,
  EV_DETACHED,
  EV_PANE_SPLIT,
  
} client_event;

struct client {
  client_state state;
  int server_fd; // 与 server 的连接 fd
  int master_fd;
  int slave_fd;
  pid_t slave_pid;
  struct winsize ws;
  struct termios orig_termios;
  int child_exited;
  struct termios raw;
  char *slave_name;
  struct environ *environ;
  struct window_pane *pane;
};

typedef void (*action_fn)(struct client *c, client_event ev);
typedef struct {
  client_state state;
  client_event event;
  client_state next;
  action_fn action;
} state_transition;

void client_init(struct client *c);
int client_main(struct client *c);
void dispatch_event(struct client *c, client_event ev);
void act_resize(struct client *C, client_event ev);
void act_child_exit(struct client *C, client_event ev);
void act_enable_raw_mode(struct client *C, client_event ev);
void act_pty_read(struct client *c, client_event ev);
void act_stdin_read(struct client *c, client_event ev);
void act_detach(struct client *c, client_event ev);
void act_pane_split(struct client *c, client_event ev);

#endif /* CLIENT_H */