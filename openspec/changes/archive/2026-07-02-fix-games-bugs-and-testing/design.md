## Context

### 当前状态

`games-mini-program` 是基于 Taro + React 的微信小程序，包含三个游戏：
- **贪吃蛇** (`src/pages/snake/`) - 有 Bug：进入即死
- **数独** (`src/pages/sudoku/`) - 有 Bug：数字填充失败、提示无限使用
- **2048** (`src/pages/game2048/`) - 有 Bug：数字出现在错误位置

此外，`web/` 目录有独立的 H5 React 版本，包含完整的消消乐实现但未集成。

### 问题根因

1. **贪吃蛇 Bug**: `setState` 异步执行后立即读取 `this.state.gameState.speed`，拿到的是旧值
2. **数独 Bug**: 点击数字按钮时，光标移动的 `setState` 尚未完成
3. **消消乐未集成**: `src/` 和 `web/` 是两套独立代码，未共享逻辑
4. **无测试**: 只有基础 E2E 测试，无单元测试覆盖核心逻辑

### 约束

- 微信小程序环境限制：无法使用 Node.js 测试工具
- 需要兼容 H5 模式运行测试
- 保持现有代码风格（类组件 + 纯函数逻辑分离）

## Goals / Non-Goals

**Goals:**
- 修复所有已确认的 Bug
- 建立完整的自动化测试体系
- 实现数据持久化和版本兼容性
- 将消消乐集成到小程序

**Non-Goals:**
- 不重构现有组件架构（保持类组件）
- 不添加在线排行榜或社交功能
- 不实现游戏内购买

## Decisions

### Decision 1: 测试框架选择

**选择**: Vitest + @testing-library/react

**理由**:
- Vitest 与 Vite 深度集成，配置简单
- 与 Jest API 兼容，迁移成本低
- 支持 TypeScript 原生
- 比 Jest 速度更快

**替代方案**:
- Jest: 需要额外配置 TypeScript，HMR 支持差
- Mocha + Chai: 需要手动配置，不够现代

### Decision 2: 单元测试结构

**选择**: `src/utils/*.test.ts` 模式

```
src/
├── utils/
│   ├── 2048.ts          # 纯函数逻辑
│   ├── 2048.test.ts     # 单元测试（新增）
│   ├── snake.ts
│   ├── snake.test.ts
│   ├── sudoku.ts
│   ├── sudoku.test.ts
│   └── match3.ts        # 从 web/ 复制
│   └── match3.test.ts
```

**理由**:
- 测试文件与源文件同目录，便于查找
- 核心逻辑是纯函数，无需模拟依赖
- 可直接在 H5 模式运行单元测试

### Decision 3: 消消乐集成策略

**选择**: 复制 `web/utils/match3.ts` 到 `src/utils/`，不直接共享

**理由**:
- 小程序版和 H5 版可能需要差异化适配
- 避免构建时复杂的依赖处理
- 保持两套代码独立性

**替代方案**:
- 符号链接: 微信开发者工具不支持
- monorepo 共享: 增加复杂度

### Decision 4: 数据持久化架构

**选择**: Taro Storage API + 版本迁移层

```typescript
// src/utils/storage.ts
interface StorageSchema {
  version: number
  games: {
    snake: { bestScore: number; unlockedLevels: number[] }
    sudoku: { bestScore: number; unlockedLevels: number[]; stars: Record<number, number> }
    game2048: { bestScore: number }
    match3: { chapters: Record<number, { unlocked: boolean; stars: Record<number, number> }> }
  }
}

const CURRENT_VERSION = 1

function migrate(data: any, fromVersion: number): StorageSchema {
  // 版本迁移逻辑
  // v0 -> v1: 初始化新字段
}
```

**理由**:
- Taro `getStorageSync`/`setStorageSync` 跨平台兼容
- 版本号支持增量迁移
- 避免数据丢失

### Decision 5: 贪吃蛇 Bug 修复

**选择**: 使用回调确保状态更新

```typescript
// 修复前
startGame = () => {
  this.setState({ gameState: createInitialState() })
  this.timer = setInterval(() => {
    this.tick()
  }, this.state.gameState.speed) // Bug: 异步问题
}

// 修复后
startGame = () => {
  this.setState(
    { gameState: createInitialState() },
    () => {
      // 确保状态已更新后再读取
      this.startTimer(this.state.gameState.speed)
    }
  )
}
```

**理由**:
- `setState` 的第二个参数是回调，在状态更新后执行
- 最小改动，风险低

### Decision 6: 数独数字填充修复

**选择**: 同步更新光标位置

```typescript
// 修复前
handleCellClick = (row: number, col: number) => {
  moveCursor(gameState, col - gameState.cursorCol, row - gameState.cursorRow)
  this.setState({ gameState: { ...gameState } })
}

// 修复后
handleNumberSelect = (num: number) => {
  const { gameState } = this.state
  // 直接在当前光标位置放置（不依赖异步状态）
  placeNumber(gameState, num)
  this.setState({ gameState: { ...gameState } })
}

handleCellClick = (row: number, col: number) => {
  const { gameState } = this.state
  // 同步更新光标
  gameState.cursorRow = row
  gameState.cursorCol = col
  this.setState({ gameState })
}
```

**理由**:
- 数字选择不依赖光标移动的异步完成
- 使用直接赋值而非 moveCursor 函数，避免重复计算

### Decision 7: CI/CD 流程

**选择**: GitHub Actions + Playwright

```yaml
name: 游戏测试
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装依赖
        run: npm install && cd web && npm install
      - name: 单元测试
        run: npm run test:unit
      - name: 启动 H5 服务
        run: npm run dev:h5 &
        sleep 40
      - name: E2E 测试
        run: npx playwright test
```

**理由**:
- GitHub Actions 免费额度充足
- Playwright 支持 H5 真实验证
- 可并行运行多个测试

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|-----|------|---------|
| Vitest 与 Taro 组件兼容问题 | 中 | 仅测试纯函数逻辑，不测 React 组件 |
| 微信小程序 Storage 限制 | 低 | 存储前 JSON 序列化，控制数据量 |
| Playwright 测试不稳定 | 中 | 添加重试机制，使用稳定选择器 |
| 消消乐代码复制导致维护成本 | 低 | 定期同步，减少差异 |

## Open Questions

1. **数据存储位置**: 使用微信本地存储还是云开发？
   - 当前建议：微信本地存储（`wx.setStorageSync`）
   - 后续可迁移到云开发实现多设备同步

2. **消消乐 H5 版本集成**: 是否需要将 Match3Game 组件也复制到小程序？
   - 建议：先只集成逻辑，UI 单独实现小程序版本

3. **测试覆盖率目标**: 单元测试覆盖率目标设为多少？
   - 建议：核心逻辑函数 80%+，UI 交互 40%+