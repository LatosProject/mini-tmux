#ifndef MAIN_H
#define MAIN_H

#include <paths.h>
#include <stdarg.h>

// #define MINI_TMUX_DEFAULT_SHELL
#ifndef MINI_TMUX_SOCK
#define MINI_TMUX_SOCK _PATH_TMP
#endif

// Buffer size constants
#define MINI_TMUX_BUF_SMALL   100   // Small buffers for paths, short messages
#define MINI_TMUX_BUF_MEDIUM  256   // Medium buffers for paths, rendering
#define MINI_TMUX_BUF_LARGE   1024  // Large buffers for log messages
#define MINI_TMUX_BUF_XLARGE  4096  // Extra large buffers for data transfer
#define MINI_TMUX_BUF_PATH    512   // Path buffers
#define MINI_TMUX_INPUT_SEQ   64    // Input sequence buffer
#define MINI_TMUX_LISTEN_BACKLOG 5  // listen() backlog

#endif /* MAIN_H */
