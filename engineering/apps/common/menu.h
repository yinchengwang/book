/**
 * @file menu.h
 * @brief 统一游戏菜单系统
 *
 * 提供游戏选择、难度选择和结果展示等通用交互接口。
 */
#ifndef MENU_H
#define MENU_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 游戏入口定义
 */
typedef struct {
    const char *name;      /**< 显示名称，如 "1. 贪吃蛇" */
    void (*launch)(void);  /**< 启动函数 */
} GameEntry;

/**
 * @brief 显示主菜单并获取用户选择
 *
 * @param games 游戏入口数组
 * @param count 游戏数量
 * @return 选中的游戏索引（0 表示退出），-1 表示退出
 */
int menu_main_select(const GameEntry *games, int count);

/**
 * @brief 显示难度选择菜单
 *
 * @param title 标题
 * @param options 难度选项数组
 * @param count 选项数量
 * @return 选中的难度索引，-1 表示取消
 */
int menu_difficulty_select(const char *title, const char *const *options, int count);

/**
 * @brief 等待用户按键退出
 */
void menu_wait_exit(void);

/**
 * @brief 显示游戏标题横幅
 *
 * @param title 游戏标题
 */
void menu_show_banner(const char *title);

/**
 * @brief 显示分隔线
 *
 * @param ch 分隔字符
 * @param width 宽度
 */
void menu_show_line(char ch, int width);

#ifdef __cplusplus
}
#endif

#endif /* MENU_H */
