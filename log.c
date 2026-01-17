#define _GNU_SOURCE
#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern char *socket_path;

static FILE *log_fp = NULL;
static log_level_t min_level = LOG_DEBUG;
static const char *log_name = "unknown";

static const char *level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

void log_init(const char *name) {
  log_name = name;

  // 从 socket_path 提取目录，创建日志文件
  if (socket_path) {
    char log_path[256];
    char *last_slash = strrchr(socket_path, '/');
    if (last_slash) {
      size_t dir_len = last_slash - socket_path;
      snprintf(log_path, sizeof(log_path), "%.*s/%s.log", (int)dir_len,
               socket_path, name);
    } else {
      snprintf(log_path, sizeof(log_path), "%s.log", name);
    }
    log_fp = fopen(log_path, "a");
    if (log_fp) {
      setvbuf(log_fp, NULL, _IOLBF, 0); // 行缓冲
    }
  }
}

void log_close(void) {
  if (log_fp) {
    fclose(log_fp);
    log_fp = NULL;
  }
}

void log_set_level(log_level_t level) { min_level = level; }

void log_write(log_level_t level, const char *file, int line, const char *fmt,
               ...) {
  if (level < min_level)
    return;

  // 获取时间戳
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char time_buf[32];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

  // 提取文件名
  const char *base = strrchr(file, '/');
  base = base ? base + 1 : file;

  // 格式化用户消息
  char msg[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  // 完整日志行
  char log_line[1280];
  snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s:%d] %s\n", time_buf,
           level_names[level], base, line, msg);

  // 输出到 stderr
  fputs(log_line, stderr);

  // 输出到文件
  if (log_fp) {
    fputs(log_line, log_fp);
  }
}
