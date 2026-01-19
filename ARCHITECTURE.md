# Mini-Tmux 项目架构文档

## 1. 项目概述

Mini-tmux 是一个简化版的终端复用器，模仿 tmux 的核心功能。它允许用户在一个终端中运行多个 shell 会话，支持会话的创建和管理。

## 2. 核心架构

```
┌─────────────────────────────────────────────────────────────────┐
│                         用户终端                                  │
│                            │                                     │
│                      ┌─────▼─────┐                               │
│                      │  Client   │  (前台进程)                    │
│                      │  client.c │                               │
│                      └─────┬─────┘                               │
│                            │ Unix Socket + FD Passing            │
│                      ┌─────▼─────┐                               │
│                      │  Server   │  (守护进程)                    │
│                      │  server.c │                               │
│                      └─────┬─────┘                               │
│                            │ PTY (master/slave)                  │
│                      ┌─────▼─────┐                               │
│                      │   Shell   │  (/bin/bash 等)               │
│                      │  spawn.c  │                               │
│                      └───────────┘                               │
└─────────────────────────────────────────────────────────────────┘
```

## 3. 文件结构与职责

| 文件 | 职责 |
|------|------|
| `main.c` | 程序入口，初始化 socket 路径和 client |
| `client.c` | 客户端逻辑，状态机驱动的事件循环 |
| `server.c` | 服务端守护进程，管理 session 和 PTY |
| `spawn.c` | 子进程创建，执行 shell |
| `util.c` | 工具函数：shell 查找、fd 传递、环境检测 |
| `log.c` | 日志系统 |
| `list.h` | Linux 内核风格的双向链表实现 |
| `mini_tmux-protocol.h` | 客户端-服务端通信协议定义 |

## 4. 核心数据结构

### 4.1 Session (服务端会话)
```c
struct session {
  int id;                    // 会话 ID
  int master_fd;             // PTY 主端 fd
  int slave_fd;              // PTY 从端 fd
  pid_t slave_pid;           // shell 子进程 PID
  struct winsize ws;         // 终端窗口大小
  struct termios orig_termios; // 原始终端属性
  int child_exited;          // 子进程是否已退出
  char *slave_name;          // PTY slave 设备名 (如 /dev/pts/0)
  struct list_head link;     // 链表节点，用于 session_list
};
```

### 4.2 Client (客户端状态)
```c
struct client {
  client_state state;        // 状态机当前状态
  int master_fd;             // 从 server 接收的 PTY master fd
  int slave_fd;              // (未使用)
  pid_t slave_pid;           // (未使用，shell 由 server 管理)
  struct winsize ws;         // 终端窗口大小
  struct termios orig_termios; // 原始终端属性，用于退出时恢复
  int child_exited;          // 退出标志
  struct termios raw;        // raw 模式终端属性
};
```

### 4.3 客户端状态机
```
  ST_BOOT ──EV_ENABLE_RAW_MODE──► ST_RUNNING
     │                               │
     │                    ┌──────────┼──────────┐
     │                    │          │          │
     │              EV_WINCH    EV_PTY_READ  EV_STDIN_READ
     │                    │          │          │
     │                    └──────────┼──────────┘
     │                               │
     │                    ┌──────────┴──────────┐
     │                    │                     │
     │            EV_EOF_PTY/STDIN        EV_CHLD_EXIT
     │                    │                     │
     │                    └──────────┬──────────┘
     │                               │
     └───────────────────────────► ST_EXITING
```

## 5. 通信协议

Client 和 Server 通过 Unix Domain Socket 通信，消息格式：

```c
struct msg_header {
    enum msgtype type;  // 消息类型
    size_t len;         // 消息体长度
};
// 紧跟 len 字节的消息体
```

主要消息类型：
- `MSG_RESIZE (231)`: Client 发送窗口大小
- `MSG_COMMAND (200)`: Client 发送命令 (如 "new-session")
- `MSG_EXITED (227)`: Client 通知退出

## 6. 核心流程

### 6.1 启动流程
```
main()
  │
  ├─► 创建 socket 目录 /tmp/mini-tmux-{uid}/
  ├─► 设置 socket_path = /tmp/mini-tmux-{uid}/default
  ├─► client_init() - 初始化客户端状态
  └─► client_main()
        │
        ├─► client_connect()
        │     │
        │     ├─► connect() 尝试连接已有 server
        │     │     │
        │     │     ├─► 成功: 返回 fd
        │     │     └─► 失败: 获取锁，调用 server_start()
        │     │
        │     └─► server_start() 创建守护进程
        │           │
        │           ├─► fork() + setsid() + 二次 fork() 成为守护进程
        │           ├─► server_loop() 监听连接
        │           └─► 父进程返回连接 fd 给 client
        │
        ├─► send_server(MSG_RESIZE) - 发送窗口大小
        ├─► send_server(MSG_COMMAND, "new-session") - 请求新会话
        ├─► recv_fd() - 接收 PTY master fd
        ├─► 设置信号处理器 (SIGWINCH, SIGCHLD)
        ├─► dispatch_event(EV_ENABLE_RAW_MODE) - 进入 raw 模式
        └─► client_loop() - 主事件循环
```

### 6.2 Server 处理流程
```
server_loop()
  │
  └─► while(1)
        │
        ├─► accept() - 等待新连接
        ├─► fork() - 为每个 client 创建子进程
        │     │
        │     ├─► 子进程: server_receive() 循环处理消息
        │     └─► 父进程: close(client_fd), 继续 accept
        │
        └─► server_receive()
              │
              ├─► 读取 msg_header
              ├─► 读取消息体
              └─► switch(msg_type)
                    │
                    ├─► MSG_RESIZE: 保存窗口大小
                    └─► MSG_COMMAND "new-session":
                          │
                          ├─► posix_openpt() - 创建 PTY
                          ├─► grantpt() + unlockpt() - 解锁 PTY
                          ├─► send_fd() - 发送 master_fd 给 client
                          ├─► open(slave) - 打开从端
                          └─► spawn_child() - fork 并执行 shell
```

### 6.3 数据流
```
用户键盘输入
     │
     ▼
  Client stdin
     │
     ▼ read()
  act_stdin_read()
     │
     ▼ write(master_fd)
  PTY Master ────────────► PTY Slave ────────────► Shell stdin
                                                      │
                                                      ▼
                                               Shell 处理
                                                      │
                                                      ▼
  PTY Master ◄──────────── PTY Slave ◄──────────── Shell stdout
     │
     ▼ read(master_fd)
  act_pty_read()
     │
     ▼ write(STDOUT)
  Client stdout
     │
     ▼
  用户屏幕
```

## 7. PTY (伪终端) 机制

PTY 由一对设备组成：
- **Master**: 控制端，Client 通过它与 Shell 通信
- **Slave**: 从端，Shell 的 stdin/stdout/stderr

```c
// Server 创建 PTY
master_fd = posix_openpt(O_RDWR);  // 创建 master
grantpt(master_fd);                 // 设置 slave 权限
unlockpt(master_fd);                // 解锁 slave
slave_name = ptsname(master_fd);    // 获取 slave 路径 (如 /dev/pts/0)
slave_fd = open(slave_name, O_RDWR); // 打开 slave
```

## 8. FD Passing (文件描述符传递)

通过 Unix Socket 的 `SCM_RIGHTS` 辅助消息传递 fd：

```c
// 发送端 (server)
send_fd(sock, master_fd);

// 接收端 (client)
int fd = recv_fd(sock);
```

这样 Client 可以直接读写 Server 创建的 PTY master。

## 9. 信号处理

| 信号 | 处理位置 | 作用 |
|------|----------|------|
| SIGWINCH | Client | 终端窗口大小改变，触发 resize |
| SIGCHLD | Server | Shell 子进程退出，关闭 PTY，通知 Client |

## 10. 当前已知问题

1. **Session ID 问题**: `session_init()` 中 `list_add_tail` 被调用两次（init 里一次，server_receive 里一次），导致链表状态混乱

2. **多 Session 隔离问题**: `server_loop` 中 fork 后子进程各自独立，不共享 `session_list`，无法实现多 session 管理

3. **退出时的清理**: 需要确保 server 关闭 `slave_fd`，否则 Client 无法收到 EOF
