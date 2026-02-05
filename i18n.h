#ifndef I18N_H
#define I18N_H

// 语言枚举
typedef enum {
  LANG_EN,  // English
  LANG_ZH,  // 中文
} language_t;

// 消息 ID 枚举
typedef enum {
  // 帮助信息
  MSG_HELP_TITLE,
  MSG_HELP_VERSION,
  MSG_HELP_USAGE,
  MSG_HELP_OPTIONS,
  MSG_HELP_OPT_LIST,
  MSG_HELP_OPT_ATTACH,
  MSG_HELP_OPT_KILL,
  MSG_HELP_OPT_HELP,
  MSG_HELP_KEYBINDINGS,
  MSG_HELP_KEY_DETACH,
  MSG_HELP_KEY_SPLIT,
  MSG_HELP_KEY_NEXT,
  MSG_HELP_KEY_SCROLL_UP,
  MSG_HELP_KEY_SCROLL_DOWN,
  MSG_HELP_EXAMPLES,
  MSG_HELP_EX_NEW,
  MSG_HELP_EX_LIST,
  MSG_HELP_EX_ATTACH,
  MSG_HELP_EX_KILL,

  // 错误信息
  MSG_ERR_MKDIR,
  MSG_ERR_STAT,
  MSG_ERR_FORK,
  MSG_ERR_OPEN_PTY,
  MSG_ERR_EXEC,

  // 会话管理
  MSG_SESSION_FORMAT,
  MSG_NO_SESSIONS,
  MSG_SESSION_KILLED,
  MSG_SESSION_NOT_FOUND,
  MSG_ATTACH_FAILED,
  MSG_NESTED_WARNING,

  // 状态栏
  MSG_STATUS_HISTORY,

  // 窗口名称
  MSG_WINDOW_NEW,
  MSG_WINDOW_ATTACHED,

  MSG_COUNT  // 消息总数
} message_id_t;

// 初始化 i18n，自动检测语言环境
void i18n_init(void);

// 设置语言
void i18n_set_language(language_t lang);

// 获取当前语言
language_t i18n_get_language(void);

// 获取翻译字符串
const char *_(message_id_t id);

// 便捷宏
#define TR(id) _(id)

#endif /* I18N_H */
