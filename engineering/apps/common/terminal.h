/**
 * @file terminal.h
 * @brief 跨平台终端适配层
 *
 * 提供终端操作封装，支持 Windows 和 POSIX 系统。
 */
#ifndef TERMINAL_H
#define TERMINAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 终端初始化结果
 */
typedef enum {
    TERM_OK = 0,
    TERM_ERR_INIT_FAILED,
    TERM_ERR_RESTORE_FAILED
} TermResult;

/**
 * @brief 初始化终端
 *
 * 设置 UTF-8 编码和原始输入模式。
 * @return TERM_OK 成功，其他失败
 */
TermResult terminal_init(void);

/**
 * @brief 恢复终端
 *
 * 恢复终端到初始状态。
 * @return TERM_OK 成功，其他失败
 */
TermResult terminal_restore(void);

/**
 * @brief 获取按键（非阻塞）
 *
 * @return 按键码，无按键时返回 -1
 */
int terminal_getch(void);

/**
 * @brief 检测是否有按键按下
 *
 * @return 1 有按键，0 无按键
 */
int terminal_kbhit(void);

/**
 * @brief 毫秒级延时
 *
 * @param ms 延时毫秒数
 */
void terminal_sleep_ms(int ms);

/**
 * @brief 隐藏光标
 */
void terminal_hide_cursor(void);

/**
 * @brief 显示光标
 */
void terminal_show_cursor(void);

/**
 * @brief 清屏
 */
void terminal_clear(void);

/**
 * @brief 移动光标到指定位置
 *
 * @param row 行号（从 1 开始）
 * @param col 列号（从 1 开始）
 */
void terminal_move_cursor(int row, int col);

/**
 * @brief 设置 UTF-8 编码
 */
void terminal_set_utf8(void);

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_H */
