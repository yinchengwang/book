## Why

`games-mini-program` 微信小程序存在多个严重 Bug 和功能缺失：
1. **贪吃蛇进入即死** - `setState` 异步导致速度残留，新游戏速度过快
2. **消消乐未集成** - 代码已存在但未加入小程序游戏列表
3. **数独交互缺陷** - 数字填充失败、提示无限使用、无关卡设计
4. **无数据持久化** - 分数、进度未保存，版本更新数据丢失
5. **无自动化测试** - 无法保证后续迭代质量

此外，消消乐在 `web/` 目录有完整实现但未集成到小程序，且三个游戏的纯函数逻辑都缺乏单元测试覆盖。

## What Changes

### Bug 修复
- 修复贪吃蛇"进入即死"Bug（setState 异步问题）
- 修复数独数字无法填充问题（状态更新时序）
- 修复数独提示无限使用（添加次数限制）
- 修复 2048 数字随机出现在错误位置（游戏规则验证）

### 功能增强
- 将消消乐（match3）从 `web/` 集成到小程序游戏列表
- 为数独添加关卡系统（章节+关卡进度）
- 添加数据持久化（分数、最高分、游戏进度）
- 设计版本更新兼容机制（数据迁移策略）

### 测试体系
- 引入 Vitest 单元测试框架
- 为 2048/snake/sudoku/match3 核心逻辑添加单元测试
- 增强 Playwright E2E 测试覆盖
- 配置 CI/CD 自动化测试流程

## Capabilities

### New Capabilities

- `games-testing-framework`: 自动化测试框架
  - Vitest 单元测试配置
  - 核心逻辑测试用例（2048/snake/sudoku/match3）
  - Playwright E2E 测试增强
  - CI/CD 集成配置

- `games-data-persistence`: 数据持久化能力
  - 本地存储抽象层
  - 游戏状态保存/恢复
  - 版本升级数据迁移

- `games-level-progression`: 关卡进度系统
  - 数独关卡设计（章节+关卡）
  - 消消乐关卡集成（50 关）
  - 星星评价系统

- `games-match3-integration`: 消消乐集成
  - match3.ts 逻辑复用
  - Match3Game 组件适配
  - 游戏列表入口添加

### Modified Capabilities

- `apps-games-mini-program`: 更新规格
  - 新增消消乐游戏入口
  - 新增持久化存储要求
  - 新增关卡系统要求
  - 更新游戏列表结构

- `apps-games-snake`: 修复规格
  - 修复初始化逻辑 Bug
  - 添加速度重置要求

- `apps-games-sudoku`: 增强规格
  - 新增提示次数限制
  - 新增关卡系统
  - 新增数字填充交互

## Impact

### 受影响代码
- `apps/web/games-mini-program/src/utils/2048.ts` - 核心逻辑
- `apps/web/games-mini-program/src/utils/snake.ts` - 核心逻辑
- `apps/web/games-mini-program/src/utils/sudoku.ts` - 核心逻辑
- `apps/web/games-mini-program/src/pages/snake/index.tsx` - 贪吃蛇 UI
- `apps/web/games-mini-program/src/pages/sudoku/index.tsx` - 数独 UI
- `apps/web/games-mini-program/src/pages/index/index.tsx` - 游戏列表
- `apps/web/games-mini-program/web/utils/match3.ts` - 消消乐逻辑
- `apps/web/games-mini-program/web/components/Match3Game.tsx` - 消消乐组件

### 新增文件
- `apps/web/games-mini-program/test/` - Vitest 测试目录
- `apps/web/games-mini-program/.github/workflows/test.yml` - CI 配置

### 依赖变更
- 新增 `vitest` 测试框架
- 新增 `@testing-library/react` 组件测试库