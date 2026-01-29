#ifndef LOG_H
#define LOG_H
typedef enum {
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
} log_level_t;

// 初始化日志系统，name 用于区分 server/client
// 日志文件会创建在 socket_path 同目录下
void log_init(const char *name);

// 关闭日志
void log_close(void);

// 设置最低日志级别
void log_set_level(log_level_t level);

// 日志开关
#define ENABLE_LOG
#ifdef ENABLE_LOG
// 日志宏
#define log_debug(...) log_write(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_write(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_write(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_write(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
#define log_debug(...)                                                         \
  do {                                                                         \
  } while (0)
#define log_info(...)                                                          \
  do {                                                                         \
  } while (0)
#define log_warn(...)                                                          \
  do {                                                                         \
  } while (0)
#define log_error(...)                                                         \
  do {                                                                         \
  } while (0)
#endif
// 内部函数，通过宏调用
void log_write(log_level_t level, const char *file, int line, const char *fmt,
               ...);

#endif
