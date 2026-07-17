# games-data-persistence

数据持久化规范，支持游戏状态保存和版本升级。

## Purpose

该规格定义游戏数据的持久化存储机制，确保分数、进度等数据在应用更新时不丢失。

## ADDED Requirements

### Requirement: 存储抽象层
系统 SHALL 提供统一的存储抽象层，兼容微信小程序和 H5 环境。

#### Scenario: 获取最高分
- **WHEN** 调用 `getBestScore('snake')`
- **THEN** 返回存储的贪吃蛇最高分，无数据时返回 0

#### Scenario: 保存最高分
- **WHEN** 当前分数 > 存储的最高分时调用 `saveBestScore('snake', score)`
- **THEN** 更新存储的最高分

#### Scenario: 获取游戏进度
- **WHEN** 调用 `getGameProgress('sudoku')`
- **THEN** 返回包含 `unlockedLevels`、`stars` 等的进度对象

#### Scenario: 保存游戏进度
- **WHEN** 调用 `saveGameProgress('sudoku', progress)`
- **THEN** 异步保存进度到存储

### Requirement: 存储数据结构

#### Scenario: 贪吃蛇数据结构
- **WHEN** 存储贪吃蛇数据
- **THEN** 数据结构为 `{ bestScore: number, unlockedLevels: number[] }`

#### Scenario: 数独数据结构
- **WHEN** 存储数独数据
- **THEN** 数据结构为 `{ bestScore: number, unlockedLevels: number[], stars: Record<number, number> }`

#### Scenario: 2048 数据结构
- **WHEN** 存储 2048 数据
- **THEN** 数据结构为 `{ bestScore: number }`

#### Scenario: 消消乐数据结构
- **WHEN** 存储消消乐数据
- **THEN** 数据结构为 `{ chapters: Record<number, { unlocked: boolean, stars: Record<number, number> }> }`

### Requirement: 版本迁移

#### Scenario: 首次启动无数据
- **WHEN** 用户首次启动应用
- **THEN** 存储版本设为 1，初始化空数据结构

#### Scenario: 版本检测
- **WHEN** 应用启动时读取存储数据
- **THEN** 检测存储版本号，执行相应的迁移函数

#### Scenario: 数据迁移 v0 -> v1
- **WHEN** 存储版本为 0（无版本字段）
- **THEN** 执行迁移：为各游戏添加默认数据结构，设置版本为 1

### Requirement: 数据恢复

#### Scenario: 页面加载恢复
- **WHEN** 游戏页面加载
- **THEN** 从存储读取最高分和进度，显示在界面上

#### Scenario: 游戏结束后保存
- **WHEN** 游戏结束且分数 > 最高分
- **THEN** 自动保存新最高分

#### Scenario: 意外关闭数据保护
- **WHEN** 应用意外关闭
- **THEN** 已完成的对局数据已保存，不丢失