# apps-games-2048

2048 游戏规范，C 语言实现，支持终端渲染。

## Purpose

该规格定义了 apps/games/2048 模块的功能需求。

## Requirements

### Requirement: 游戏初始化
系统 SHALL 提供 `game2048_init()` 函数，根据难度参数初始化游戏状态。

#### Scenario: 单块初始化
- **WHEN** 调用 `game2048_init(SINGLE_INIT_TILES)`
- **THEN** 棋盘为空，随机生成 1 个新块（2 或 4）

#### Scenario: 困难模式初始化
- **WHEN** 调用 `game2048_init(HARD_INIT_TILES)`
- **THEN** 棋盘为空，随机生成 3 个新块

### Requirement: 游戏状态管理
系统 SHALL 提供状态查询接口。

#### Scenario: 获取棋盘状态
- **WHEN** 调用 `game2048_get_board()`
- **THEN** 返回 4x4 棋盘数组（0 表示空格）

#### Scenario: 获取分数
- **WHEN** 调用 `game2048_get_score()`
- **THEN** 返回当前分数

#### Scenario: 获取最高分
- **WHEN** 调用 `game2048_get_best_score()`
- **THEN** 返回历史最高分

#### Scenario: 获取游戏结束状态
- **WHEN** 调用 `game2048_is_game_over()`
- **THEN** 当无有效移动时返回 true

#### Scenario: 获取胜利状态
- **WHEN** 调用 `game2048_has_won()`
- **THEN** 当棋盘出现 2048 时返回 true

### Requirement: 移动逻辑
系统 SHALL 提供四个方向的移动函数。

#### Scenario: 向左移动
- **WHEN** 调用 `game2048_move_left()`
- **THEN** 所有块向左合并，得分增加

#### Scenario: 向右移动
- **WHEN** 调用 `game2048_move_right()`
- **THEN** 所有块向右合并，得分增加

#### Scenario: 向上移动
- **WHEN** 调用 `game2048_move_up()`
- **THEN** 所有块向上合并，得分增加

#### Scenario: 向下移动
- **WHEN** 调用 `game2048_move_down()`
- **THEN** 所有块向下合并，得分增加

### Requirement: 新块生成
系统 SHALL 提供 `game2048_spawn_tile()` 函数，在空格中随机生成新块。

#### Scenario: 生成新块
- **WHEN** 调用 `game2048_spawn_tile()`
- **THEN** 在随机空格位置生成 2（90%）或 4（10%）

### Requirement: 游戏结束判定
系统 SHALL 检测游戏是否无法继续。

#### Scenario: 检测无法移动
- **WHEN** 棋盘已满且无相邻相同数字
- **THEN** `game2048_can_move()` 返回 false

### Requirement: 难度配置
系统 SHALL 支持三种难度等级。

| 难度 | 初始块数 | 说明 |
|------|----------|------|
| 入门 | 1 | 适合新手 |
| 简单 | 2 | 中等挑战 |
| 困难 | 3 | 高难挑战 |
