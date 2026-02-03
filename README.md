# mini-tmux

一个极简版的终端复用器，用于在一个终端里同时管理和控制多个子 shell 会话。

**Version:** 0.3.3
**Author:** Latos

## 功能特性

- 创建独立的终端会话
- 会话分离 (detach)：断开连接但保持 shell 运行
- 会话附加 (attach)：重新连接到已分离的会话
- 终止指定会话 (kill)
- 列出所有活动会话
- 后台守护进程自动管理
- **窗格分割**：垂直分割窗格 (Ctrl+B %)
- **窗格切换**：在多个窗格之间切换 (Ctrl+B o)
- **窗格退出**：单个窗格退出时自动调整布局
- **终端模拟**：使用 libvterm 实现完整的终端模拟
- **历史滚动**：查看终端输出历史 (Ctrl+B [ / ])

## 快速开始

### 编译

```bash
mkdir build && cd build
cmake ..
make
```

### 基本使用

```bash
# 创建新会话
./mini-tmux

# 列出所有会话
./mini-tmux -l

# 附加到指定会话
./mini-tmux -s 0

# 终止指定会话
./mini-tmux -k 0

# 显示帮助
./mini-tmux -h
```

## 使用说明

### 命令行参数

| 参数 | 说明 |
|------|------|
| `-l` | 列出所有会话 |
| `-s <id>` | 附加到指定 ID 的会话 |
| `-k <id>` | 终止指定 ID 的会话 |
| `-h` | 显示帮助信息 |

### 快捷键

| 按键 | 功能 |
|------|------|
| `Ctrl+B d` | 从当前会话分离 |
| `Ctrl+B %` | 垂直分割窗格 |
| `Ctrl+B o` | 切换到下一个窗格 |
| `Ctrl+B [` | 向上滚动查看历史 |
| `Ctrl+B ]` | 向下滚动 |

### 自定义快捷键

可以通过配置文件自定义快捷键，配置文件路径：`/tmp/mini-tmux-<uid>/keybinds.conf`

配置格式：`key table action`

```conf
# 示例配置
1 KEY_PREFIX new_pane
2 KEY_PREFIX next_pane
q KEY_PREFIX detach
```

可用的 action：
- `detach_session` - 分离会话
- `new_pane` - 新建窗格
- `next_pane` - 切换到下一个窗格
- `scroll_up` - 向上滚动查看历史
- `scroll_down` - 向下滚动

## 项目架构

```
mini-tmux/
├── main.c/h              # 程序入口，命令行解析
├── client.c/h            # 客户端：终端 I/O、状态机
├── server.c/h            # 服务端：会话管理、守护进程
├── spawn.c/h             # 子进程创建（fork/exec shell）
├── window.c/h            # 窗口和窗格管理
├── render.c/h            # 终端渲染（窗格内容、边框、状态栏）
├── input.c/h             # 输入处理
├── util.c/h              # 工具函数（fd 传递、socket 操作）
├── log.c/h               # 日志系统
├── list.h                # Linux 内核风格双向链表
├── mini_tmux-protocol.h  # 客户端-服务端通信协议
└── CMakeLists.txt        # CMake 构建配置
```

### 通信架构

```
┌─────────────────────────────────────────────────────────────┐
│                        Terminal                             │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Client Process                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Raw Mode    │  │ Event Loop  │  │ Signal Handler      │  │
│  │ Terminal    │  │ (select)    │  │ (SIGWINCH/SIGCHLD)  │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                    Unix Domain Socket
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Server Process (Daemon)                   │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Session     │  │ Event Loop  │  │ Child Process       │  │
│  │ Manager     │  │ (select)    │  │ Monitor (SIGCHLD)   │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                       PTY Master/Slave
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Shell Process                          │
│                     (bash/zsh/sh)                           │
└─────────────────────────────────────────────────────────────┘
```

## 技术实现

### 核心技术

| 技术 | 说明 |
|------|------|
| **PTY (伪终端)** | 使用 `posix_openpt`、`grantpt`、`ptsname` 创建虚拟终端对 |
| **Unix 域套接字** | 客户端与服务端之间的本地进程间通信 |
| **文件描述符传递** | 通过 `sendmsg`/`recvmsg` 的 `SCM_RIGHTS` 跨进程传递 fd |
| **信号处理** | `SIGCHLD` 监控子进程退出，`SIGWINCH` 响应窗口大小变化 |
| **守护进程** | Double-fork 模式创建后台服务进程 |
| **termios** | 终端 raw 模式控制，禁用行缓冲和回显 |
| **libvterm** | 终端模拟库，解析转义序列，维护屏幕缓冲区 |

### 消息协议

客户端与服务端通过自定义协议通信：

```c
struct msg_header {
    enum msgtype type;  // 消息类型
    size_t len;         // 消息体长度
};
```

主要消息类型：
- `MSG_COMMAND` - 创建新会话
- `MSG_DETACH` - 分离/附加会话
- `MSG_RESIZE` - 窗口大小变化
- `MSG_LIST_SESSIONS` - 列出会话

## 日志

日志文件位于 `/tmp/mini-tmux-<uid>/` 目录：
- `client.log` - 客户端日志
- `server.log` - 服务端日志

## 参考

本项目参考了 [tmux](https://github.com/tmux/tmux) 的设计思想，适合学习 Unix 系统编程。

涉及的 Unix 编程知识：
- 《Advanced Programming in the UNIX Environment》(APUE)
- POSIX 标准 PTY 接口
- Unix 域套接字编程
- 守护进程创建

## License

[MIT](LICENSE)
