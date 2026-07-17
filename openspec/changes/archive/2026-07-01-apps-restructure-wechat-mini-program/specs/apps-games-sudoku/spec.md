# apps-games-sudoku

数独游戏规范，C++ 实现，支持终端渲染。

## ADDED Requirements

### Requirement: 数独生成
系统 SHALL 提供 `sudoku_generate()` 函数，生成指定难度的数独题目。

#### Scenario: 生成简单难度
- **WHEN** 调用 `sudoku_generate(0)` （简单）
- **THEN** 生成 30 个空格的数独题目

#### Scenario: 生成困难难度
- **WHEN** 调用 `sudoku_generate(2)` （困难）
- **THEN** 生成 50 个空格的数独题目

### Requirement: 数独求解
系统 SHALL 提供 `sudoku_solve()` 函数，求解数独并验证解的正确性。

#### Scenario: 正确求解
- **WHEN** 传入有效的数独棋盘
- **THEN** 返回 true，棋盘被填满

#### Scenario: 无解
- **WHEN** 传入无效的数独棋盘
- **THEN** 返回 false

### Requirement: 数字放置验证
系统 SHALL 提供 `sudoku_is_valid()` 函数，验证数字放置是否符合规则。

#### Scenario: 有效放置
- **WHEN** 在空格中放置不冲突的数字
- **THEN** 返回 true

#### Scenario: 行冲突
- **WHEN** 放置的数字与同行已有数字冲突
- **THEN** 返回 false

#### Scenario: 列冲突
- **WHEN** 放置的数字与同列已有数字冲突
- **THEN** 返回 false

#### Scenario: 宫格冲突
- **WHEN** 放置的数字与 3x3 宫格内已有数字冲突
- **THEN** 返回 false

### Requirement: 游戏状态管理
系统 SHALL 提供状态查询接口。

#### Scenario: 获取棋盘状态
- **WHEN** 调用 `sudoku_get_board()`
- **THEN** 返回当前棋盘状态（9x9 数组）

#### Scenario: 获取光标位置
- **WHEN** 调用 `sudoku_get_cursor()`
- **THEN** 返回当前光标位置 (row, col)

#### Scenario: 获取剩余空格数
- **WHEN** 调用 `sudoku_get_empty_count()`
- **THEN** 返回剩余空格数量

#### Scenario: 获取游戏结束状态
- **WHEN** 调用 `sudoku_is_complete()`
- **THEN** 当所有空格填满且无冲突时返回 true

### Requirement: 难度配置
系统 SHALL 支持三种难度等级。

| 难度 | 空格子数 | 说明 |
|------|----------|------|
| 简单 | 30 | 适合新手 |
| 中等 | 40 | 中等挑战 |
| 困难 | 50 | 高难挑战 |