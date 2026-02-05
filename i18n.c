#include "i18n.h"
#include <stdlib.h>
#include <string.h>

static language_t current_lang = LANG_EN;

// 英文翻译
static const char *messages_en[MSG_COUNT] = {
    // 帮助信息
    [MSG_HELP_TITLE] = "mini-tmux - a minimal terminal multiplexer\n\n",
    [MSG_HELP_VERSION] = "        Version: %s By Latos\n\n",
    [MSG_HELP_USAGE] = "Usage: %s [options]\n\n",
    [MSG_HELP_OPTIONS] = "Options:\n",
    [MSG_HELP_OPT_LIST] = "  -l         List all sessions\n",
    [MSG_HELP_OPT_ATTACH] = "  -s <id>    Attach to detached session by id\n",
    [MSG_HELP_OPT_KILL] = "  -k <id>    Kill session by id\n",
    [MSG_HELP_OPT_HELP] = "  -h         Show this help message\n\n",
    [MSG_HELP_KEYBINDINGS] = "Key bindings:\n",
    [MSG_HELP_KEY_DETACH] = "  Ctrl+B d   Detach from current session\n",
    [MSG_HELP_KEY_SPLIT] = "  Ctrl+B %%   Split pane vertically\n",
    [MSG_HELP_KEY_NEXT] = "  Ctrl+B o   Switch to next pane\n",
    [MSG_HELP_KEY_SCROLL_UP] = "  Ctrl+B [   Scroll up (view history)\n",
    [MSG_HELP_KEY_SCROLL_DOWN] = "  Ctrl+B ]   Scroll down\n\n",
    [MSG_HELP_EXAMPLES] = "Examples:\n",
    [MSG_HELP_EX_NEW] = "  %s           Start a new session\n",
    [MSG_HELP_EX_LIST] = "  %s -l        List all sessions\n",
    [MSG_HELP_EX_ATTACH] = "  %s -s 0      Attach to session 0\n",
    [MSG_HELP_EX_KILL] = "  %s -k 0      Kill session 0\n",

    // 错误信息
    [MSG_ERR_MKDIR] = "mkdir failed",
    [MSG_ERR_STAT] = "stat failed",
    [MSG_ERR_FORK] = "Fork failed",
    [MSG_ERR_OPEN_PTY] = "open slave pty failed",
    [MSG_ERR_EXEC] = "Execve failed",

    // 会话管理
    [MSG_SESSION_FORMAT] = "%d: %s (pid %d)\n",
    [MSG_NO_SESSIONS] = "(no sessions)\n",
    [MSG_SESSION_KILLED] = "killed session %d\n",
    [MSG_SESSION_NOT_FOUND] = "session %d not found\n",
    [MSG_ATTACH_FAILED] = "attach failed: session %d not found or not detached\n",
    [MSG_NESTED_WARNING] = "sessions should be nested with care\n",

    // 状态栏
    [MSG_STATUS_HISTORY] = "[history]",

    // 窗口名称
    [MSG_WINDOW_NEW] = "New Window",
    [MSG_WINDOW_ATTACHED] = "Attached Window",
};

// 中文翻译
static const char *messages_zh[MSG_COUNT] = {
    // 帮助信息
    [MSG_HELP_TITLE] = "mini-tmux - 轻量级终端复用器\n\n",
    [MSG_HELP_VERSION] = "        版本: %s 作者: Latos\n\n",
    [MSG_HELP_USAGE] = "用法: %s [选项]\n\n",
    [MSG_HELP_OPTIONS] = "选项:\n",
    [MSG_HELP_OPT_LIST] = "  -l         列出所有会话\n",
    [MSG_HELP_OPT_ATTACH] = "  -s <id>    连接到指定会话\n",
    [MSG_HELP_OPT_KILL] = "  -k <id>    终止指定会话\n",
    [MSG_HELP_OPT_HELP] = "  -h         显示帮助信息\n\n",
    [MSG_HELP_KEYBINDINGS] = "快捷键:\n",
    [MSG_HELP_KEY_DETACH] = "  Ctrl+B d   分离当前会话\n",
    [MSG_HELP_KEY_SPLIT] = "  Ctrl+B %%   垂直分割窗格\n",
    [MSG_HELP_KEY_NEXT] = "  Ctrl+B o   切换到下一窗格\n",
    [MSG_HELP_KEY_SCROLL_UP] = "  Ctrl+B [   向上滚动(查看历史)\n",
    [MSG_HELP_KEY_SCROLL_DOWN] = "  Ctrl+B ]   向下滚动\n\n",
    [MSG_HELP_EXAMPLES] = "示例:\n",
    [MSG_HELP_EX_NEW] = "  %s           启动新会话\n",
    [MSG_HELP_EX_LIST] = "  %s -l        列出所有会话\n",
    [MSG_HELP_EX_ATTACH] = "  %s -s 0      连接到会话 0\n",
    [MSG_HELP_EX_KILL] = "  %s -k 0      终止会话 0\n",

    // 错误信息
    [MSG_ERR_MKDIR] = "创建目录失败",
    [MSG_ERR_STAT] = "获取文件状态失败",
    [MSG_ERR_FORK] = "创建进程失败",
    [MSG_ERR_OPEN_PTY] = "打开伪终端失败",
    [MSG_ERR_EXEC] = "执行程序失败",

    // 会话管理
    [MSG_SESSION_FORMAT] = "%d: %s (进程号 %d)\n",
    [MSG_NO_SESSIONS] = "(无会话)\n",
    [MSG_SESSION_KILLED] = "已终止会话 %d\n",
    [MSG_SESSION_NOT_FOUND] = "会话 %d 不存在\n",
    [MSG_ATTACH_FAILED] = "连接失败: 会话 %d 不存在或未分离\n",
    [MSG_NESTED_WARNING] = "警告: 不建议嵌套运行会话\n",

    // 状态栏
    [MSG_STATUS_HISTORY] = "[历史]",

    // 窗口名称
    [MSG_WINDOW_NEW] = "新窗口",
    [MSG_WINDOW_ATTACHED] = "已连接窗口",
};

void i18n_init(void) {
  const char *lang = getenv("LANG");
  if (lang == NULL) {
    lang = getenv("LC_ALL");
  }
  if (lang == NULL) {
    lang = getenv("LC_MESSAGES");
  }

  if (lang != NULL) {
    if (strncmp(lang, "zh", 2) == 0) {
      current_lang = LANG_ZH;
    } else {
      current_lang = LANG_EN;
    }
  }
}

void i18n_set_language(language_t lang) { current_lang = lang; }

language_t i18n_get_language(void) { return current_lang; }

const char *_(message_id_t id) {
  if (id < 0 || id >= MSG_COUNT) {
    return "";
  }

  const char *msg = NULL;
  switch (current_lang) {
  case LANG_ZH:
    msg = messages_zh[id];
    break;
  case LANG_EN:
  default:
    msg = messages_en[id];
    break;
  }

  return msg ? msg : messages_en[id];
}
