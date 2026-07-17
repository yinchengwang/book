# apps-games-snake

贪吃蛇游戏规范（Delta Spec - 修复版本）

## MODIFIED Requirements

### Requirement: 游戏初始化
系统 SHALL 提供 `createInitialState()` 函数，初始化游戏状态，新游戏必须重置速度。

#### Scenario: 简单难度初始化
- **WHEN** 调用 `createInitialState()` 启动新游戏
- **THEN** 蛇长度为 3，初始速度为 150ms（INITIAL_SPEED），食物位置随机生成

#### Scenario: 新游戏速度重置
- **WHEN** 游戏中调用 `createInitialState()` 开始新游戏
- **THEN** 速度必须重置为 INITIAL_SPEED (150ms)，不是上次游戏结束时的速度

### Requirement: 速度恢复 Bug 修复
系统 SHALL 确保新游戏开始时速度正确重置，不受上次游戏速度影响。

#### Scenario: 速度不被残留
- **WHEN** 上次游戏吃到多个食物后速度降到 50ms
- **AND** 用户开始新游戏
- **THEN** 新游戏速度必须为 150ms，不是 50ms

## ADDED Requirements

### Requirement: 分数持久化接口
系统 SHALL 提供分数保存和读取接口。

#### Scenario: 获取历史最高分
- **WHEN** 调用 `getBestScore()` 时
- **THEN** 从存储读取历史最高分，无数据时返回 0

#### Scenario: 保存新最高分
- **WHEN** 游戏结束且当前分数 > 历史最高分
- **THEN** 自动保存新最高分到存储