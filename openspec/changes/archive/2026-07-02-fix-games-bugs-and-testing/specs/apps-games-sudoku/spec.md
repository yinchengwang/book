# apps-games-sudoku

数独游戏规范（Delta Spec - 增强版本）

## MODIFIED Requirements

### Requirement: 数字放置
系统 SHALL 允许用户在光标位置放置选中的数字。

#### Scenario: 有效数字放置
- **WHEN** 用户选中数字后点击空格
- **THEN** 数字立即填充到该位置，无需等待异步状态更新

#### Scenario: 固定格不可修改
- **WHEN** 用户尝试修改 `isGiven: true` 的格子
- **THEN** 格子值不变，无反应

## ADDED Requirements

### Requirement: 提示次数限制
系统 SHALL 为每个游戏对局提供有限的提示次数。

#### Scenario: 初始提示次数
- **WHEN** 开始新数独游戏
- **THEN** 获得 3 次提示机会

#### Scenario: 提示次数耗尽
- **WHEN** 提示次数为 0 时点击提示按钮
- **THEN** 显示"提示次数已用完"提示，无数字被填充

#### Scenario: 提示功能正常
- **WHEN** 提示次数 > 0 时调用 `hint(state)`
- **THEN** 第一个空格被填充正确答案，提示次数 -1

### Requirement: 关卡系统
系统 SHALL 提供章节和关卡进度系统。

#### Scenario: 章节结构
- **WHEN** 用户打开数独游戏
- **THEN** 显示 5 个章节，每个章节 10 关（共 50 关）

#### Scenario: 关卡解锁
- **WHEN** 完成前一章所有关卡
- **THEN** 自动解锁下一章节

#### Scenario: 星星评价
- **WHEN** 完成数独关卡
- **THEN** 根据用时给予 1-3 星评价

### Requirement: 进度持久化
系统 SHALL 保存和恢复游戏进度。

#### Scenario: 保存关卡进度
- **WHEN** 完成关卡获得星星
- **THEN** 保存章节解锁状态和各关卡星星数

#### Scenario: 恢复进度
- **WHEN** 重新打开数独游戏
- **THEN** 显示各章节解锁状态和星星评价