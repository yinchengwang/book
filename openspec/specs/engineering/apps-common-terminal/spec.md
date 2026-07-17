# apps-common-terminal

终端适配层规范，封装跨平台的终端操作接口。

## Purpose

该规格定义了 apps-common/terminal 模块的功能需求，用于为游戏程序提供跨平台的终端操作能力。

## Requirements

### Requirement: 终端初始化
系统 SHALL 提供 `terminal_init()` 函数，初始化终端环境，包括设置 UTF-8 编码和进入原始输入模式。

#### Scenario: Windows 平台初始化
- **WHEN** 在 Windows 平台调用 `terminal_init()`
- **THEN** 设置控制台代码页为 65001 (UTF-8)，启用原始输入模式

#### Scenario: Linux/macOS 平台初始化
- **WHEN** 在 POSIX 平台调用 `terminal_init()`
- **THEN** 设置 locale 为 UTF-8，配置 termios 原始模式

### Requirement: 终端恢复
系统 SHALL 提供 `terminal_restore()` 函数，在程序退出前恢复终端状态。

#### Scenario: 正常恢复
- **WHEN** 游戏结束后调用 `terminal_restore()`
- **THEN** 终端恢复到初始状态，光标可见，可输入

### Requirement: 按键输入
系统 SHALL 提供 `terminal_getch()` 和 `terminal_kbhit()` 函数，实现跨平台的非阻塞按键检测。

#### Scenario: 检测按键按下
- **WHEN** 用户按下任意键
- **THEN** `terminal_kbhit()` 返回 1，`terminal_getch()` 返回按键码

#### Scenario: 无按键时
- **WHEN** 没有按键按下
- **THEN** `terminal_kbhit()` 返回 0

### Requirement: 方向键支持
系统 SHALL 支持 WASD 和方向键输入，在 Windows 和 POSIX 平台均正常工作。

#### Scenario: Windows 方向键
- **WHEN** 用户按下方向键（上）
- **THEN** `terminal_getch()` 返回 0xE0 或 0x00，后跟 72

#### Scenario: POSIX 方向键
- **WHEN** 用户按下方向键（上）
- **THEN** `terminal_getch()` 返回 '\\033'，后跟 '[', 'A'

### Requirement: 光标控制
系统 SHALL 提供光标显示/隐藏和清屏功能。

#### Scenario: 隐藏光标
- **WHEN** 调用 `terminal_hide_cursor()`
- **THEN** 终端光标被隐藏

#### Scenario: 显示光标
- **WHEN** 调用 `terminal_show_cursor()`
- **THEN** 终端光标被显示

### Requirement: 时间控制
系统 SHALL 提供 `terminal_sleep_ms()` 函数，实现跨平台的毫秒级延时。

#### Scenario: 延时等待
- **WHEN** 调用 `terminal_sleep_ms(100)`
- **THEN** 程序阻塞 100 毫秒后继续执行
