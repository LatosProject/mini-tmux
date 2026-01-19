#include "list.h"
#define _XOPEN_SOURCE 700
#include "client.h"
#include "log.h"
#include "main.h"
#include "mini_tmux-protocol.h"
#include "server.h"
#include "spawn.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
extern char *socket_path;
struct list_head session_list;
static volatile sig_atomic_t sigchld_pending = 0;
ssize_t read_n(int fd, void *buf, size_t n) {
  size_t recvd = 0;
  char *p = buf;
  while (recvd < n) {
    ssize_t r = read(fd, p + recvd, n - recvd);
    if (r == 0)
      return 0; // EOF
    if (r == -1) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    recvd += r;
  }
  return recvd;
}

void server_signal_handler(int sig) {
  switch (sig) {
  case SIGCHLD:
    sigchld_pending = 1; // 只设置标志，主循环处理
    break;
  }
}

void session_init(struct session *s) {
  s->id = -1;
  s->client_fd = -1;
  s->master_fd = -1;
  s->slave_fd = -1;
  s->slave_pid = -1;
  s->child_exited = 0;
  list_init(&s->link);
  tcgetattr(STDIN_FILENO, &(s->orig_termios));
  ioctl(STDIN_FILENO, TIOCGWINSZ, &(s->ws));
}

// 根据 client_fd 查找 session
static struct session *find_session_by_client_fd(int fd) {
  struct session *sess;
  list_for_each_entry(sess, &session_list, link) {
    if (sess->client_fd == fd) {
      return sess;
    }
  }
  return NULL;
}

// 根据 session id 查找 session
static struct session *find_session_by_id(int id) {
  struct session *sess;
  list_for_each_entry(sess, &session_list, link) {
    if (sess->id == id) {
      return sess;
    }
  }
  return NULL;
}

/*
  处理来自客户端的消息 (forked 子进程)
*/
int server_receive(int fd) {
  // 先读取消息头，判断消息类型
  struct msg_header hdr;
  if (read_n(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    log_error("read header failed: %s", strerror(errno));
    return -1;
  }

  // 读取消息体
  char *buf = NULL;
  if (hdr.len > 0) {
    buf = malloc(hdr.len);
    if (read_n(fd, buf, hdr.len) != hdr.len) {
      log_error("read payload failed: %s", strerror(errno));
      free(buf);
      return -1;
    }
  }

  // MSG_LIST_SESSIONS 不需要创建 session，直接处理
  if (hdr.type == MSG_LIST_SESSIONS) {
    char response[4096] = {0};
    int offset = 0;
    struct session *s;
    int count = 0;

    list_for_each_entry(s, &session_list, link) {
      // 只列出有效的 session（有 shell 进程的）
      if (s->slave_pid > 0) {
        count++;
        const char *status = s->detached ? "\ndetached" : "\nattached";
        offset += snprintf(response + offset, sizeof(response) - offset,
                           "%d: %s (pid %d)\n", s->id, status, s->slave_pid);
      }
    }

    if (count == 0) {
      snprintf(response, sizeof(response), "(no sessions)\n");
    }

    size_t len = strlen(response) + 1;
    write(fd, &len, sizeof(len));
    write(fd, response, len);

    log_info("listed %d sessions", count);
    free(buf);
    return -1; // 关闭连接
  }

  // 其他消息类型需要关联 session
  struct session *cur = find_session_by_client_fd(fd);

  // 如果没找到，说明是新连接，创建新 session
  if (cur == NULL) {
    cur = malloc(sizeof(struct session));
    session_init(cur);
    cur->client_fd = fd;

    // 设置 session id
    if (list_empty(&session_list)) {
      cur->id = 0;
    } else {
      struct session *last =
          list_last_entry(&session_list, struct session, link);
      cur->id = last->id + 1;
    }
    list_add_tail(&cur->link, &session_list);
    log_debug("created new session id=%d for fd=%d", cur->id, fd);
  }

  // 判断消息类型
  switch (hdr.type) {
    // 处理命令
  case MSG_COMMAND:
    if (strcmp(buf, "new-session") == 0) {
      // 创建伪终端
      cur->master_fd = posix_openpt(O_RDWR);
      if (cur->master_fd == -1) {
        log_error("posix_openpt failed: %s", strerror(errno));
        _exit(-1);
      }
      // 传回client
      // 解锁 slave 设备
      grantpt(cur->master_fd);
      unlockpt(cur->master_fd);

      send_fd(fd, cur->master_fd);
      cur->slave_name = ptsname(cur->master_fd);
      cur->slave_fd = open(cur->slave_name, O_RDWR);
      ioctl(cur->slave_fd, TIOCSWINSZ, &cur->ws);

      log_info("create a new session, id:%d", cur->id);

      cur->slave_pid = spawn_child(cur);

      if (cur->slave_pid < 0) {
        log_error("spawn_child failed");
        _exit(-1);
      }

      log_info("spawned child process with pid %d", cur->slave_pid);
    }
    free(buf);
    return 1;
  case MSG_RESIZE:
    log_debug("resize session");
    if (cur == NULL) {
      log_warn("MSG_RESIZE: session not found for fd %d", fd);
      free(buf);
      return 1;
    }
    memcpy(&cur->ws, buf, sizeof(cur->ws)); // 保存尺寸
    if (cur->slave_fd >= 0) {
      ioctl(cur->slave_fd, TIOCSWINSZ, &cur->ws);
    }
    free(buf);
    return 1;
  case MSG_EXITED:
    log_info("exit a session, pid:%s", buf);
    struct session *sess;
    list_for_each_entry(sess, &session_list, link) {
      log_info("session id=%d, pid=%d", sess->id, sess->slave_pid);
    }
    return -1;
    break;
  case MSG_DETACH:
    if (hdr.len == 0) {
      log_info("detach a session");
      sess = NULL;
      sess = find_session_by_client_fd(fd);
      if (sess) {
        sess->detached = 1;
        log_debug("session id=%d marked as detached", sess->id);
      }
    } else {
      // attach: 客户端发送的是二进制 int
      int session_id;
      memcpy(&session_id, buf, sizeof(session_id));
      struct session *target = find_session_by_id(session_id);
      if (target && target->detached) {
        log_debug("attaching to detached session id=%d", target->id);
        send_fd(fd, target->master_fd);
        target->client_fd = fd;
        target->detached = 0;
      } else {
        log_warn("attach failed: session %d not found or not detached",
                 session_id);
        // 发送失败标记：写入一个字节表示失败，不传递 fd
        char fail = 0;
        write(fd, &fail, 1);
      }
    }
    free(buf);
    return 1; // 返回 1，让 detach 处理代码来关闭 fd

  default:
    log_warn("unknown msgtype %d", hdr.type);
  }

  free(buf);
  return 1;
}

/*
  服务器主循环，监听客户端连接请求
*/
void server_loop(int listen_fd) {
  log_info("server loop started, listening on fd %d", listen_fd);

  // 在循环开始前设置信号处理器
  struct sigaction sa;
  sa.sa_handler = server_signal_handler;
  sa.sa_flags = 0; // 不用 SA_RESTART，让 select 被信号打断
  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa, NULL);

  fd_set read_fds;
  int max_fd;
  int client_fds[MAX_CLIENTS] = {-1};
  // 初始化客户端 fd 数组
  for (int i = 0; i < MAX_CLIENTS; i++) {
    client_fds[i] = -1;
  }
  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds); // 添加监听 fd
    max_fd = listen_fd;

    // 当client_fds不为空时，把client_fds加入监听集合
    // 添加所有已连接的客户端 fd
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (client_fds[i] >= 0) {
        FD_SET(client_fds[i], &read_fds);
        if (client_fds[i] > max_fd) {
          max_fd = client_fds[i];
        }
      }
    }

    // 阻塞，等待 fd 可读
    int select_ok = 1;
    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
      if (errno == EINTR) {
        select_ok = 0; // 不 continue，让后续代码检查 sigchld_pending
      } else {
        log_error("select failed: %s", strerror(errno));
        break;
      }
    }

    // 只有 select 成功时才处理 fd
    if (select_ok) {
      // 检查监听 fd是否可读，有新客户端连接
      if (FD_ISSET(listen_fd, &read_fds)) {
        int new_fd = accept(listen_fd, NULL, NULL);
        if (new_fd >= 0) {
          for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] == -1) {
              client_fds[i] = new_fd;
              break;
            }
          }
        }
      }

      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] >= 0 &&
            FD_ISSET(client_fds[i],
                     &read_fds)) { // 只处理"内核告诉你可读"的 fd"
          // 客户端断开连接则关闭 fd
          if (server_receive(client_fds[i]) < 0) {
            close(client_fds[i]);
            client_fds[i] = -1;
          }
        }
      }
    }

    // 处理 detach 的 session
    struct session *sess;
    list_for_each_entry(sess, &session_list, link) {
      if (sess->detached == 1) {
        // 先从 client_fds 数组中移除(此时 sess->client_fd 还保存着旧值)
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (client_fds[i] == sess->client_fd) {
            client_fds[i] = -1; // 清空槽位,防止 fd 复用时冲突
            break;
          }
        }

        // 关闭客户端连接(但保持 PTY 和 shell 继续运行)
        close(sess->client_fd);
        sess->client_fd = -1; // 标记 session 已没有客户端连接

        log_info("session %d detached, shell continues running", sess->id);
      }
    }

    // 无论 select 是否成功，都检查 sigchld_pending
    if (sigchld_pending) {
      sigchld_pending = 0;
      // 回收所有退出的子进程
      int status;
      pid_t pid;
      while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct session *sess;
        list_for_each_entry(sess, &session_list, link) {
          if (sess->slave_pid == pid) {
            sess->child_exited = 1;
            close(sess->master_fd);
            close(sess->slave_fd);
            // 关闭 client 连接，通知 client 退出
            if (sess->client_fd >= 0) {
              // 同步清理 client_fds 数组
              for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == sess->client_fd) {
                  client_fds[i] = -1;
                  break;
                }
              }
              close(sess->client_fd);
              sess->client_fd = -1;
            }
            break;
          }
        }
      }
      // 安全删除已退出的 session
      struct session *sess, *tmp;
      list_for_each_entry_safe(sess, tmp, &session_list, link) {
        if (sess->child_exited) {
          log_info("cleaning up session id=%d", sess->id);
          list_del(&sess->link);
          free(sess);
        }
      }
    }
  }
}

/*
  服务器启动函数，返回连接到服务器的客户端socket fd
*/
int server_start() {
  // 初始化 session 链表
  list_init(&session_list);
  sigset_t set, oldset;
  log_info("server is starting");

  // 创建 unix 套接字，用于客户端连接
  int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd == -1) {
    log_error("socket failed: %s", strerror(errno));
    return -1;
  }
  struct sockaddr_un sa;
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, socket_path, sizeof(sa.sun_path) - 1);

  // 绑定 socket
  if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
    log_error("bind failed: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  log_debug("bound to %s", socket_path);

  // 监听客户端连接
  if (listen(listen_fd, 5) == -1) {
    log_error("listen failed: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // 阻塞所有信号，防止 fork 出错
  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  /*
    fork流程图如下：

    parent (wait for child1)
      |
      |-- fork() --> child1
                        |
                        |-- setsid()
                        |
                        |-- fork() --> child2 (daemon)
                                           |
                                           |-- parent (child1) exits
                                           |
                                           |-- child2 (daemon)
                                                 |
                                                 |-- umask(0)
                                                 |-- close stdio, redirect to
    /dev/null
                                                 |-- sigprocmask(SIG_SETMASK,
    &oldset, NULL)
                                                 |-- log_init
                                                 |-- server_loop(listen_fd)
                                                 |-- close(listen_fd)
                                                 |-- log_close()
                                                 |-- _exit(0)
      |
      |-- parent waits for child1 to exit
      |-- parent closes listen_fd
      |-- parent connects to server as client
  */

  // fork 出守护进程
  pid_t pid = fork();
  if (pid < 0) {
    log_error("fork failed: %s", strerror(errno));
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    close(listen_fd);
    return -1;
  }
  if (pid == 0) {
    // 服务端守护进程
    // 创建新会话，脱离控制终端
    if (setsid() == -1) {
      log_error("setsid failed: %s", strerror(errno));
      _exit(1);
    }

    // 二次fork，防止进程重新获得控制终端
    pid_t pid2 = fork();
    if (pid2 < 0) {
      _exit(1);
    }

    if (pid2 > 0) {
      // 第一个子进程退出，让子进程成为真正的守护进程
      _exit(0);
    }

    // 设置文件权限掩码，确保读写权限
    umask(0);

    // 关闭标准输入输出，重定向到 /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY); // stdin  -> fd 0
    open("/dev/null", O_WRONLY); // stdout -> fd 1
    open("/dev/null", O_WRONLY); // stderr -> fd 2
                                 //
    // 恢复信号掩码
    sigprocmask(SIG_SETMASK, &oldset, NULL);

    // 服务器启动完毕
    log_init("server");
    log_info("server daemon started, pid %d", getpid());

    // 进入服务器主循环
    server_loop(listen_fd);
    close(listen_fd);
    log_close();
    _exit(0);
  } else {

    // 等待第一个子进程退出（它会立即退出，子进程继续运行）
    waitpid(pid, NULL, 0);

    // 恢复parent进程信号掩码
    sigprocmask(SIG_SETMASK, &oldset, NULL);
    close(listen_fd);

    // 连接到刚创建的 server
    // 只用于获取 child2 的 fd
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
      log_error("client socket failed: %s", strerror(errno));
      return -1;
    }
    if (connect(client_fd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
      log_error("client connect to new server failed: %s", strerror(errno));
      close(client_fd);
      return -1;
    }
    log_debug("connected to server, fd %d", client_fd);
    return client_fd; // 获取 child2 的fd，返回到 client 进程
  }
}
