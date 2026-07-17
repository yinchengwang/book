# apps-games-snake

贪吃蛇游戏规范，C 语言实现，支持终端渲染。

## ADDED Requirements

### Requirement: 游戏初始化
系统 SHALL 提供 `snake_init()` 函数，根据难度参数初始化游戏状态。

#### Scenario: 简单难度初始化
- **WHEN** 调用 `snake_init(DIFF_EASY)`
- **THEN** 蛇长度为 3，速度为 180ms，初始化食物位置

#### Scenario: 地狱难度初始化
- **WHEN** 调用 `snake_init(DIFF_HELL)`
- **THEN** 蛇长度为 3，速度为 80ms，初始化食物位置

### Requirement: 游戏状态管理
系统 SHALL 提供状态查询接口，供渲染层获取游戏数据。

#### Scenario: 获取蛇身位置
- **WHEN** 调用 `snake_get_positions()`
- **THEN** 返回蛇身坐标数组（包含头部和身体）

#### Scenario: 获取食物位置
- **WHEN** 调用 `snake_get_food()`
- **THEN** 返回当前食物坐标

#### Scenario: 获取分数
- **WHEN** 调用 `snake_get_score()`
- **THEN** 返回当前得分（每吃一个食物 +5 分）

#### Scenario: 获取游戏结束状态
- **WHEN** 调用 `snake_is_game_over()`
- **THEN** 返回游戏是否结束

### Requirement: 游戏逻辑更新
系统 SHALL 提供 `snake_update()` 函数，每帧更新游戏状态。

#### Scenario: 蛇移动
- **WHEN** 调用 `snake_update()`
- **THEN** 蛇根据当前方向移动一格

#### Scenario: 吃食物
- **WHEN** 蛇头移动到食物位置
- **THEN** 蛇身增长一格，分数 +5，生成新食物

#### Scenario: 撞墙
- **WHEN** 蛇头撞到边界
- **THEN** 游戏结束

#### Scenario: 撞自己
- **WHEN** 蛇头撞到蛇身
- **THEN** 游戏结束

### Requirement: 方向控制
系统 SHALL 提供 `snake_set_direction()` 函数，设置蛇的移动方向。

#### Scenario: 有效方向改变
- **WHEN** 当前方向为 RIGHT，用户输入 LEFT
- **THEN** 方向设置为 LEFT

#### Scenario: 无效方向改变（反向）
- **WHEN** 当前方向为 RIGHT，用户输入 LEFT
- **THEN** 如果 RIGHT 的反方向是 LEFT，则忽略输入

### Requirement: 难度配置
系统 SHALL 支持三种难度等级。

| 难度 | 速度 | 说明 |
|------|------|------|
| DIFF_EASY | 180ms | 适合新手 |
| DIFF_HARD | 120ms | 中等挑战 |
| DIFF_HELL | 80ms | 地狱模式 |