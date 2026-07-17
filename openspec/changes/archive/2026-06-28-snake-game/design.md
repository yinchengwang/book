## Context

AlgorithmPractice 仓库的 `project/` 目录用于存放独立的小项目 demo。第一个计划项目为贪吃蛇游戏，纯 C 语言实现，运行于终端，无外部库依赖。

## Goals / Non-Goals

**Goals:**
- 100×50 终端围墙内运行的贪吃蛇游戏
- 三档难度选择（简单/困难/地域），初始速度不同
- 速度随分数梯度提升（每吃掉一个食物，分数+5，速度加快一档）
- 侧面面板实时显示得分、蛇长度、当前速度
- WASD 和方向键双支持，Q 退出

**Non-Goals:**
- 不引入外部库依赖（ncurses 等），纯标准 C + conio.h（Windows）/ termios（Linux）
- 不实现存档/读档
- 不实现多人模式或网络功能

## Decisions

### 控制台图形方案
- **选择**: 纯字符渲染 + ANSI 转义序列（Linux）或 `conio.h`（Windows）
- **替代方案**: ncurses 跨平台更好但引入了外部依赖，放弃
- **渲染策略**: 每次更新清屏后重新渲染整个地图到 stdout（`system("cls")` / `\033[2J\033[H`）

### 蛇的数据结构
- **选择**: 数组模拟循环队列，`snake[0]` 为蛇头
- **替代方案**: 链表实现更灵活但开销大，贪吃蛇长度有上限，数组足够
- 食物碰撞后标记 `snake_growth_pending`，下一帧延长一格而非立即插入新节点

### 速度控制
- 基础延迟（ms）= 初始速度，`sleep_ms()` 控制帧间隔
- 速度提升：`base_speed - (level * delta)`，有下限（20ms）
- 三个难度档位各自独立的递减曲线

### 依赖关系
- 游戏代码位于 `project/snake/` 下，通过根 `CMakeLists.txt:95-97` 的 `add_subdirectory(project)` 自动发现
- 独立可执行文件，不链接 `self_made` 等库，保持零外部依赖

## Risks / Trade-offs

- [风险] Windows `_getch()` 行为与 Linux `termios` 不同 → 使用 `#ifdef _WIN32` 隔离，Win/Linux 分别测试
- [风险] `system("cls")` 在某些终端下闪烁 → 可优化为局部刷新，但当前方案更简单
- [权衡] 无 ncurses 导致跨平台需要额外的宏隔离代码 → 接受，代码量不大，维护成本可控
