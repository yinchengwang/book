# games-testing-framework

自动化测试框架规范，为游戏小程序提供完整的测试覆盖。

## Purpose

该规格定义了游戏小程序的自动化测试框架，包括单元测试、E2E 测试和 CI/CD 集成。

## ADDED Requirements

### Requirement: 单元测试框架
系统 SHALL 提供 Vitest 单元测试框架，支持 TypeScript 纯函数测试。

#### Scenario: 2048 单元测试
- **WHEN** 运行 `npm run test:unit -- games/2048`
- **THEN** 执行 `src/utils/2048.test.ts` 测试用例并报告结果

#### Scenario: 贪吃蛇单元测试
- **WHEN** 运行 `npm run test:unit -- games/snake`
- **THEN** 执行 `src/utils/snake.test.ts` 测试用例并报告结果

#### Scenario: 数独单元测试
- **WHEN** 运行 `npm run test:unit -- games/sudoku`
- **THEN** 执行 `src/utils/sudoku.test.ts` 测试用例并报告结果

#### Scenario: 消消乐单元测试
- **WHEN** 运行 `npm run test:unit -- games/match3`
- **THEN** 执行 `src/utils/match3.test.ts` 测试用例并报告结果

### Requirement: 2048 核心逻辑测试

#### Scenario: 向左移动合并测试
- **WHEN** 棋盘为 `[[2,2,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]` 调用 `moveLeft`
- **THEN** 棋盘变为 `[[4,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]`，得分为 4

#### Scenario: 连续合并测试
- **WHEN** 棋盘为 `[[2,2,2,2],[0,0,0,0],[0,0,0,0],[0,0,0,0]]` 调用 `moveLeft`
- **THEN** 棋盘变为 `[[4,4,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]`，得分为 8

#### Scenario: 游戏结束检测
- **WHEN** 棋盘为 `[[2,4,2,4],[4,2,4,2],[2,4,2,4],[4,2,4,2]]`
- **THEN** `canMove(state)` 返回 `false`

#### Scenario: 胜利条件检测
- **WHEN** 棋盘任意位置出现 2048
- **THEN** `state.hasWon` 为 `true`

### Requirement: 贪吃蛇核心逻辑测试

#### Scenario: 蛇正确移动测试
- **WHEN** 蛇位于中心向右移动
- **THEN** 蛇头 x 坐标 +1，尾部不移动

#### Scenario: 吃食物增长测试
- **WHEN** 蛇头位置与食物位置重合
- **THEN** 蛇身长度 +1，分数 +10

#### Scenario: 墙壁碰撞检测
- **WHEN** 蛇头 x 坐标 < 0 或 >= GRID_WIDTH
- **THEN** `state.isGameOver` 为 `true`

#### Scenario: 自身碰撞检测
- **WHEN** 蛇头位置与身体任一位置重合
- **THEN** `state.isGameOver` 为 `true`

#### Scenario: 反向移动禁止
- **WHEN** 蛇当前向右移动时输入向左
- **THEN** 方向不变，蛇继续向右

### Requirement: 数独核心逻辑测试

#### Scenario: 有效数字放置测试
- **WHEN** 在空格位置放置与行列宫不冲突的数字
- **THEN** 数字成功放置，`emptyCount` -1

#### Scenario: 无效数字放置测试
- **WHEN** 在空格位置放置与行列宫冲突的数字
- **THEN** 数字仍被放置（允许试探），冲突格标记为红色

#### Scenario: 固定格不可修改测试
- **WHEN** 尝试修改 `isGiven: true` 的格子
- **THEN** 格子值不变

#### Scenario: 提示功能测试
- **WHEN** 调用 `hint(state)` 时还有空格
- **THEN** 第一个空格被填入正确答案，`hintCount` -1

#### Scenario: 提示次数耗尽
- **WHEN** `hintCount` 为 0 时调用 `hint(state)`
- **THEN** 无空格被填充

### Requirement: 消消乐核心逻辑测试

#### Scenario: 棋盘初始化测试
- **WHEN** 调用 `createBoard()`
- **THEN** 返回 9x9 棋盘，无连续 3 个相同宝石

#### Scenario: 有效交换检测
- **WHEN** 交换两个相邻格子后产生匹配
- **THEN** `isValidSwap` 返回 `true`

#### Scenario: 无效交换检测
- **WHEN** 交换两个相邻格子后无匹配产生
- **THEN** `isValidSwap` 返回 `false`

#### Scenario: 宝石下落测试
- **WHEN** 调用 `dropGems(board)` 后
- **THEN** 所有空格被上方宝石填充，顶部生成新宝石

### Requirement: E2E 测试覆盖

#### Scenario: 贪吃蛇 E2E 测试
- **WHEN** Playwright 运行 `snake.spec.ts`
- **THEN** 验证页面加载、蛇移动、游戏结束流程

#### Scenario: 数独 E2E 测试
- **WHEN** Playwright 运行 `sudoku.spec.ts`
- **THEN** 验证数字放置、难度切换、提示功能

#### Scenario: 2048 E2E 测试
- **WHEN** Playwright 运行 `game2048.spec.ts`
- **THEN** 验证方向键移动、分数更新、新游戏重置

#### Scenario: 消消乐 E2E 测试
- **WHEN** Playwright 运行 `match3.spec.ts`
- **THEN** 验证宝石交换、消除动画、得分更新

### Requirement: CI/CD 集成

#### Scenario: PR 测试触发
- **WHEN** 创建 Pull Request
- **THEN** 自动运行单元测试和 E2E 测试

#### Scenario: 测试失败阻止合并
- **WHEN** 任何测试用例失败
- **THEN** PR 状态检查失败，阻止合并

#### Scenario: 测试通过允许合并
- **WHEN** 所有测试用例通过
- **THEN** PR 状态检查通过，允许合并