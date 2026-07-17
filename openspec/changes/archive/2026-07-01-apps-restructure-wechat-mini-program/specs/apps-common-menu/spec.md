# apps-common-menu

统一游戏菜单系统规范，提供游戏选择、难度选择和结果展示等通用交互。

## ADDED Requirements

### Requirement: 主菜单显示
系统 SHALL 提供 `menu_main_select()` 函数，显示游戏列表并等待用户选择。

#### Scenario: 显示游戏列表
- **WHEN** 调用 `menu_main_select(games, 3)`
- **THEN** 显示带编号的游戏列表，等待用户输入数字

#### Scenario: 用户选择有效游戏
- **WHEN** 用户输入 1-3 之间的数字
- **THEN** 返回对应的游戏索引，启动该游戏

#### Scenario: 用户选择退出
- **WHEN** 用户输入 0
- **THEN** 返回 -1，表示退出

### Requirement: 难度选择
系统 SHALL 提供 `menu_difficulty_select()` 函数，显示难度选项并获取用户选择。

#### Scenario: 显示难度列表
- **WHEN** 调用 `menu_difficulty_select("选择难度", options, 3)`
- **THEN** 显示标题和难度选项列表

#### Scenario: 获取难度选择
- **WHEN** 用户输入 1-3 之间的数字
- **THEN** 返回对应的难度索引 (0-2)

### Requirement: 退出等待
系统 SHALL 提供 `menu_wait_exit()` 函数，显示"按任意键退出"提示并等待用户按键。

#### Scenario: 等待退出
- **WHEN** 调用 `menu_wait_exit()`
- **THEN** 显示提示信息，等待用户按键后返回

### Requirement: Banner 显示
系统 SHALL 提供 `menu_show_banner()` 函数，显示游戏标题横幅。

#### Scenario: 显示标题
- **WHEN** 调用 `menu_show_banner("贪吃蛇")`
- **THEN** 显示装饰边框的游戏标题

### Requirement: 游戏数据结构
系统 SHALL 提供 `GameEntry` 结构体定义游戏入口。

#### Scenario: 定义贪吃蛇游戏
```c
static void launch_snake(void) { /* ... */ }
static const GameEntry GAMES[] = {
    { "1. 贪吃蛇", launch_snake },
    { "2. 数独", launch_sudoku },
    { "3. 2048", launch_2048 },
    { "0. 退出", NULL },
};
```