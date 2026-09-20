# Games & Reading Radar UI Redesign — Design Spec

**Date:** 2026-09-05
**Status:** Draft (Pending User Review)
**Scope:** `engineering/apps/games/web/` + `engineering/apps/web/reading-radar/`

## 1. 概述

### 1.1 背景

工程目录 `D:\code\book` 下有两个独立的 Web 子项目：

- **游戏中心** (`engineering/apps/games/web/`)：包含 2048、贪吃蛇的 Web 版（HTML + Canvas + WASM），UI 简陋（基础 Canvas 渲染，CSS 只有几十行）。
- **读书雷达** (`engineering/apps/web/reading-radar/`)：静态学习站点，11 个大型 HTML 页面（quiz-system.html 117KB），涵盖知识地图、题库、学习内容、仪表盘、五年计划等。

两个项目当前风格不统一、技术栈老旧，需要现代化重设计。`engineering/apps/web/reading-radar/` 已有完整功能（314 知识点 + 题库），本次重点是 UI 改造而非功能重写。

### 1.2 目标

- 统一设计系统与视觉语言
- 引入现代化前端栈（React + TypeScript + Vite）
- 保留 C → WASM 的游戏核心
- 增加业界最佳实践功能（动画、撤销、笔记、提示、暗色主题、移动端适配等）
- 不破坏现有数据格式与用户存档

### 1.3 非目标

- 不实现新的玩法机制（仅 UI 增强）
- 不改变题库内容、知识点结构
- 不引入服务端数据库（保持纯前端 + 可选 server.js）

## 2. 范围

### 2.1 包含

| 模块 | 改造内容 |
|------|---------|
| **游戏 web** | React + TS 重构，2048/贪吃蛇 UI 升级，新增数独 Web 版 |
| **读书雷达** | 11 个 HTML 渐进迁移到 React + TS，保留 server.js 数据层 |
| **设计系统** | 共享 tokens、组件库、主题切换 |
| **WASM 集成** | 三个游戏统一编译、类型化绑定、Web Worker 卸载 |
| **测试** | 单元测试、E2E 测试、性能监控 |

### 2.2 不包含

- `engineering/apps/web/games-mini-program/`（微信小游戏，独立项目）
- `engineering/apps/web/knowledge_hub/`（与 RAG 关联）
- `engineering/apps/web/taro-template/`、`todo-app/`（独立模板项目）
- `web/`（RAG 系统独立前端）

## 3. 技术栈

| 类别 | 技术 | 理由 |
|------|------|------|
| 框架 | React 18 + TypeScript 5 | 业界标准，类型安全 |
| 构建 | Vite 5 | 快速 HMR、ESM 原生 |
| 样式 | Tailwind CSS 3.4 + CSS Modules | 设计 tokens 统一 |
| 状态 | Zustand | 轻量、TS 友好 |
| 路由 | React Router 6 | 标准选择 |
| 动画 | Framer Motion + Canvas 帧动画 | UI 动画 + 游戏帧动画分工 |
| 国际化 | react-i18next | 中英双语 |
| 测试 | Vitest + Playwright | 单元 + E2E |
| WASM 编译 | Emscripten (emcc) 6.0.9 | 已有 emsdk 环境 |
| 后端（可选） | Express 5 | 迁移自 server.js |

## 4. 架构

### 4.1 目录结构

#### 4.1.1 游戏 web（基于现有 `engineering/apps/games/web/`）

```
engineering/apps/games/web/
├── index.html                      # 入口（Vite 自动生成）
├── package.json                    # React/Vite/Tailwind 依赖
├── vite.config.ts
├── tailwind.config.ts
├── tsconfig.json
│
├── src/                            # 前端源码
│   ├── main.tsx
│   ├── App.tsx
│   ├── router.tsx
│   │
│   ├── pages/
│   │   ├── Home/                   # 游戏中心（取代旧 index.html）
│   │   ├── Game2048/
│   │   ├── Snake/
│   │   └── Sudoku/                 # 新增
│   │
│   ├── games/                      # 游戏渲染层
│   │   ├── g2048/
│   │   │   ├── renderer.ts         # Canvas 绘制
│   │   │   ├── input.ts            # 键盘/触摸
│   │   │   ├── animation.ts        # 动画系统
│   │   │   ├── hooks.ts            # React hooks
│   │   │   └── types.ts
│   │   ├── snake/
│   │   └── sudoku/
│   │
│   ├── shared/
│   │   ├── ui/                     # Button/Dialog/Slider 等
│   │   ├── theme/                  # tokens + ThemeProvider
│   │   ├── storage/                # localStorage/IndexedDB 封装
│   │   ├── i18n/                   # 中英双语
│   │   └── components/             # 跨游戏复用组件
│   │
│   ├── wasm/
│   │   ├── loader.ts               # 异步加载 + 错误处理
│   │   ├── bindings.ts             # 类型化绑定
│   │   └── types.ts
│   │
│   └── stores/
│       ├── settings.ts             # 全局设置
│       ├── stats.ts                # 游戏统计
│       └── achievements.ts         # 成就系统
│
├── js/                             # 旧文件保留（过渡期）
│   └── engine.js                   # 旧 LocalEngine，向前兼容
│
├── css/                            # 旧文件保留
│
├── wasm-src/                       # C 源码
│   ├── g2048.c
│   ├── snake.c
│   ├── sudoku.c                    # 从 C++ 移植为 C
│   └── binding.c                   # WASM 胶水层
│
└── public/
    └── wasm/
        ├── games.js
        ├── games.wasm
        └── games.worker.js         # 可选：计算隔离
```

#### 4.1.2 读书雷达（基于现有 `engineering/apps/web/reading-radar/`）

```
engineering/apps/web/reading-radar/
├── package.json
├── vite.config.ts
├── tsconfig.json
│
├── src/
│   ├── main.tsx
│   ├── App.tsx
│   ├── router.tsx
│   │
│   ├── pages/
│   │   ├── Home/                   # 原 index.html（雷达图入口）
│   │   ├── Kanban/                 # 原 learning-kanban.html
│   │   ├── Quiz/                   # 原 quiz-system.html
│   │   ├── Learn/                  # 原 learn.html
│   │   ├── Dashboard/              # 原 dashboard.html
│   │   ├── FiveYearPlan/           # 原 five-year-plan.html
│   │   ├── Interview/              # 原 interview.html
│   │   ├── InterviewTracker/       # 原 interview-tracker.html
│   │   ├── Practice/               # 原 practice.html
│   │   ├── Grok/                   # 原 grok.html
│   │   └── Excerpt/                # 原 excerpt.html
│   │
│   ├── components/                 # 共享组件
│   │
│   ├── data/                       # 数据访问层
│   │   ├── tech.ts                 # 技术栈元数据
│   │   ├── questions.ts            # 题库加载
│   │   └── learn.ts                # 学习内容索引
│   │
│   └── lib/
│       ├── markdown.tsx            # MD 渲染（含 Mermaid）
│       ├── stats.ts                # 掌握率计算
│       └── storage.ts
│
├── public/
│   └── data/                       # 现有 data/ 内容（只读引用）
│       ├── app/
│       ├── quiz/
│       └── learn-deep/
│
├── server/                         # 迁移自 server.js
│   ├── index.ts                    # Express 入口
│   ├── api/                        # REST 路由
│   └── storage/                    # user-data/ 持久化
│
└── user-data/                      # 运行时数据（Git 追踪）
```

### 4.2 整体架构图

```
┌──────────────────────────────────────────────┐
│        React UI Layer                         │
│  路由 / 组件 / Hooks / 主题 / 动画 / i18n     │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│        Game Logic Layer（仅游戏）              │
│  渲染循环 / 输入 / 撤销栈 / 计时 / 统计       │
└────────────────┬─────────────────────────────┘
                 │ 类型安全绑定
┌────────────────▼─────────────────────────────┐
│        WASM Adapter                           │
│  异步加载 / 类型校验 / 错误处理 / 性能计时    │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│        WASM Module（games.js）                │
│  emcc 生成的胶水 + 类型化 C 函数              │
└────────────────┬─────────────────────────────┘
                 │
┌────────────────▼─────────────────────────────┐
│        C Game Core                            │
│  2048 / 贪吃蛇 / 数独 纯算法                  │
└──────────────────────────────────────────────┘
```

## 5. 设计系统

### 5.1 Design Tokens

```ts
// shared/theme/tokens.ts
export const tokens = {
  color: {
    primary: { 50: '#eef2ff', 500: '#6366f1', 900: '#312e81' },
    g2048: {
      bg: '#faf8ef',
      tile: { 2: '#eee4da', 4: '#ede0c8', 8: '#f2b179', 16: '#f59563',
             32: '#f67c5f', 64: '#f65e3b', 128: '#edcf72', 256: '#edcc61',
             512: '#edc850', 1024: '#edc53f', 2048: '#edc22e' },
    },
    snake: { board: '#f5f5f5', snake: '#2ecc71', food: '#e74c3c' },
    sudoku: { grid: '#bbada0', cell: '#faf8ef', accent: '#3b82f6' },
  },
  spacing: { 1: '4px', 2: '8px', 3: '12px', 4: '16px', 8: '32px' },
  radius: { sm: '4px', md: '8px', lg: '16px', full: '9999px' },
  font: {
    sans: '"Inter", "Noto Sans SC", system-ui, sans-serif',
    mono: '"JetBrains Mono", "Cascadia Code", monospace',
  },
  shadow: { sm: '0 1px 2px rgba(0,0,0,.05)', md: '0 4px 6px rgba(0,0,0,.1)' },
};
```

### 5.2 主题系统

- **亮色 / 暗色**：自动跟随 `prefers-color-scheme`
- **手动切换**：右上角主题按钮，写入 localStorage
- **游戏微调**：每个游戏可有专属色板（2048 暖、贪吃蛇活力、数独冷静）

### 5.3 共享组件库

```
shared/ui/
├── Button          # variant: primary/ghost/icon, size: sm/md/lg
├── Card            # 卡片容器
├── Dialog/Modal    # 模态框（Radix UI 封装）
├── Select/Slider/Switch/Checkbox
├── Toast           # 操作反馈
├── Tooltip
├── ProgressBar
├── ScoreDisplay    # 分数 + 历史最高
├── KeyboardHint    # WASD / 方向键可视化
└── Layout          # Header/Sidebar/Footer
```

## 6. 游戏 web 改造

### 6.1 功能矩阵

| 功能 | 2048 | 贪吃蛇 | 数独 |
|------|:----:|:------:|:----:|
| 基础玩法 | ✅ | ✅ | ✅ |
| 动画过渡（滑动/弹出/插值） | ✅ | ✅ | ✅ |
| 撤销/重做 | ✅/✅ | ❌ | ✅/✅ |
| 难度选择 | ✅ 4x4/5x5/6x6 | ✅ 简/中/难 | ✅ 简/中/难 |
| 暂停/继续 | ✅ | ✅ | ✅ |
| 计时 | ✅ | ✅ | ✅ |
| 分数 + 最高分 | ✅ | ✅ | ✅ |
| 触摸/滑动 | ✅ | ✅ | ✅ |
| 响应式 | ✅ | ✅ | ✅ |
| 键盘 + 触摸双输入 | ✅ | ✅ | ✅ |
| 新游戏（随机种子） | ✅ | ✅ | ✅ |
| 笔记/草稿 | ❌ | ❌ | ✅ |
| 智能提示 | ❌ | ❌ | ✅ |
| 错误检测 | ❌ | ❌ | ✅ |
| 自动保存/恢复 | ✅ | ✅ | ✅ |
| 复盘/截图分享 | ✅ | ✅ | ✅ |
| 主题/皮肤 | ✅ 3套 | ✅ 2套 | ✅ |
| 音效（可关） | ✅ | ✅ | ✅ |
| 新手引导 | ✅ | ✅ | ✅ |
| 成就系统 | ✅ | ✅ | ✅ |
| 多语言 | ✅ | ✅ | ✅ |

### 6.2 2048 设计要点

- **动画**：方块滑动 `transform: translate3d()` 200ms `cubic-bezier(0.25, 0.1, 0.25, 1)`；新方块出现 `scale(0→1)` 弹性 `cubic-bezier(0.18, 0.89, 0.32, 1.28)`；合并瞬间 `scale(1.2)` + 颜色脉冲。
- **撤销栈**：WASM 端在每次操作前快照棋盘（16 ints + score），JS 栈存 20 步。
- **触摸**：起点 + delta 阈值 30px，主方向判定。

### 6.3 贪吃蛇设计要点

- **插值动画**：`requestAnimationFrame` + `lerp(prev, current, t)` 平滑移动。
- **变体**：经典（撞墙死）/ 穿墙 / 障碍物。
- **难度速度**：简单 180ms/格、中等 120ms/格、困难 80ms/格、无尽每 10 食物 -5ms。

### 6.4 数独设计要点（新增）

**C 层移植**：将现有 C++ 代码移植为纯 C（与 emcc 兼容）。

```c
// sudoku.h
void sudoku_generate(SudokuGame *g, int difficulty, int seed);
int  sudoku_solve(SudokuGame *g, int row, int col, int *count, int limit);
int  sudoku_hint(const SudokuGame *g, int row, int col);
bool sudoku_is_valid(const SudokuGame *g, int row, int col, int num);
```

**难度（挖洞）**：简单 30、中等 40、困难 50，保证唯一解（用 `count_solutions(board, 2)` 验证）。

**笔记系统**：每格三态（未填 / 笔记 / 答案），双击切换。

**智能提示**：揭示一格正确答案（扣分）+ 给出逻辑推理（X-Wing、唯余法）。

**错误检测**：实时行/列/宫冲突高亮，错误次数统计。

### 6.5 路由

```tsx
<Routes>
  <Route path="/" element={<Home />} />
  <Route path="/2048" element={<Game2048 />} />
  <Route path="/snake" element={<Snake />} />
  <Route path="/sudoku" element={<Sudoku />} />
  <Route path="/sudoku/:difficulty" element={<Sudoku />} />
  <Route path="/settings" element={<Settings />} />
</Routes>
```

## 7. 读书雷达改造

### 7.1 现状

| 文件 | 行数 | 复杂度 |
|------|-----:|-------:|
| `dashboard.html` | ~600 | 中 |
| `learn.html` | ~1100 | 高 |
| `quiz-system.html` | ~2500 | **高** |
| `index.html` | ~1500 | 高 |
| `five-year-plan.html` | ~1000 | 中 |
| `interview.html` | ~700 | 中 |
| `interview-tracker.html` | ~600 | 中 |
| `learning-kanban.html` | ~700 | 中 |
| `grok.html` | ~600 | 中 |
| `practice.html` | ~500 | 中 |
| `excerpt.html` | ~400 | 低 |

代码量极大，直接重写风险高。采用**渐进迁移 + 数据/UI 分离**策略。

### 7.2 迁移策略

**Phase 1（基础设施，1-2 周）**
- 项目搭建（React + Vite + TS）
- 设计系统接入
- 共享组件库
- 数据层抽象（封装现有 `data/`）

**Phase 2（核心页迁移，3-5 周）**
- 优先级：**Quiz → Learn → Kanban → Dashboard**（用户最多）
- 每页独立迁移，老 HTML 加"新版上线"横幅
- hash 路由保持兼容

**Phase 3（次要页迁移，6-7 周）**
- Index / FiveYearPlan / Interview 等
- 老 HTML 在 1-2 个迭代后删除

### 7.3 功能保留与增强

| 功能 | 当前 | 改造后 |
|------|:----:|:------:|
| 知识地图（雷达） | ✅ | ✅ + 动画、搜索、过滤 |
| 题库测评 | ✅ | ✅ + 收藏、错题本、解析动画 |
| 学习内容（MD） | ✅ | ✅ + Mermaid、目录、进度同步 |
| 看板 | ✅ | ✅ + 拖拽、批量操作 |
| 仪表盘 | ✅ | ✅ + 趋势图、热力图、PDF 导出 |
| 五年计划 | ✅ | ✅ + 提醒、回顾 |
| 面试题 | ✅ | ✅ + 公司分类、难度筛选 |
| **全局搜索** | ❌ | ✅ 题/知识点/内容 |
| **学习时长统计** | ❌ | ✅ |
| **暗色主题** | ❌ | ✅ |
| **移动端适配** | ❌ | ✅ |

### 7.4 关键技术决策

- **Markdown**：`react-markdown` + `remark-gfm` + `rehype-mermaid`
- **数据兼容**：`import { C_TECH_DATA } from '../public/data/quiz/tech.js'` 直接复用旧数据
- **Server.js 迁移**：Express 5，REST 路径保持不变（`/api/state` 等）

## 8. WASM 集成

### 8.1 命名冲突解决

**当前问题**：手写胶水易与底层符号冲突（已遇到 `g2048_create` 冲突）。

**新方案**：统一前缀 + 显式命名空间。

```c
// wasm-src/binding.c
#include <emscripten.h>
#include "g2048.h"
#include "snake.h"
#include "sudoku.h"

// 2048 命名空间
EMSCRIPTEN_KEEPALIVE void g2048_init(int seed);
EMSCRIPTEN_KEEPALIVE int  g2048_move(int dir);
EMSCRIPTEN_KEEPALIVE int  g2048_get(int row, int col);
EMSCRIPTEN_KEEPALIVE int  g2048_score(void);

// 数独命名空间
EMSCRIPTEN_KEEPALIVE void sudoku_init(int difficulty, int seed);
EMSCRIPTEN_KEEPALIVE void sudoku_set(int row, int col, int num);

// 蛇命名空间
EMSCRIPTEN_KEEPALIVE void snake_init(int difficulty, int seed);
```

### 8.2 构建脚本

```bash
# wasm-src/build.sh
emcc -o public/wasm/games.js \
    src/binding.c src/g2048.c src/snake.c src/sudoku.c \
    -lm \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="GameModule" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
    -s SINGLE_FILE=0 \
    -O3
```

### 8.3 TypeScript 绑定

```ts
// src/wasm/bindings.ts
export interface G2048Module {
  g2048_init(seed: number): void;
  g2048_move(dir: number): 0 | 1;
  g2048_get(row: number, col: number): number;
  g2048_score(): number;
  // ...
}

export interface SudokuModule {
  sudoku_init(difficulty: number, seed: number): void;
  sudoku_set(row: number, col: number, num: number): void;
  sudoku_hint(): { row: number; col: number; num: number } | null;
  // ...
}

let _module: GameModule | null = null;
export async function loadWasm(): Promise<GameExports> {
  if (_module) return _module;
  _module = await window.GameModule({ locateFile: (p) => `/wasm/${p}` });
  return _module as unknown as GameExports;
}
```

## 9. 错误处理

| 场景 | 表现 | 兜底 |
|------|------|------|
| `games.wasm` 404 | 显示"资源加载失败" | 重试 + 路径检查提示 |
| 浏览器不支持 WASM | 显示"请升级浏览器" | 推荐 Chrome/Firefox/Safari 最低版本 |
| WASM 编译失败 | 显示具体错误 | 联系开发者（带日志） |
| `games.js` 语法错误 | 控制台报错 | 强制刷新 + 清缓存 |
| 网络慢/CORS | 进度条 | 预估剩余时间 |

C → JS 数据校验：

```ts
function readBoardCell(row: number, col: number): number {
  if (row < 0 || row > 3 || col < 0 || col > 3) {
    throw new RangeError(`Invalid cell: ${row},${col}`);
  }
  return _exports.g2048_get(row, col);
}
```

游戏状态异常：

- 蛇飞出棋盘 → 标记 game_over=true（不崩溃）
- 数独生成超时 → 降级到简单难度
- 撤销栈溢出 → 自动丢弃最早的快照

`localStorage` 损坏静默降级：

```ts
function safeGet<T>(key: string, fallback: T): T {
  try {
    const v = localStorage.getItem(key);
    return v ? JSON.parse(v) : fallback;
  } catch {
    return fallback;
  }
}
```

## 10. 测试策略

### 10.1 测试层级

- **单元测试**（Vitest）：WASM 适配层、Hooks、组件
- **C 代码测试**（cmocka）：核心算法，100% 覆盖关键路径
- **E2E 测试**（Playwright）：每个游戏 5-10 个核心场景

### 10.2 覆盖目标

| 层级 | 目标 |
|------|------|
| C 核心算法 | 100%（关键路径） |
| TS 绑定层 | 80% |
| React 组件 | 70% |
| E2E 关键路径 | 每游戏 5-10 场景 |

### 10.3 自动化游戏测试示例

```ts
test('2048: 完成一局能合并到 128', async ({ page }) => {
  await page.goto('/2048');
  for (let i = 0; i < 50; i++) {
    await page.keyboard.press(['ArrowUp','ArrowRight','ArrowDown','ArrowLeft'][i % 4]);
    await page.waitForTimeout(50);
  }
  expect(await page.textContent('#score-val')).not.toBe('0');
});
```

## 11. 性能指标

| 指标 | 目标 |
|------|------|
| 首屏加载 (FCP) | < 1.5s |
| 可交互 (TTI) | < 2.5s |
| WASM 加载 | < 500ms（已缓存） |
| 2048 操作响应 | < 16ms（60fps） |
| 蛇移动 tick | < 8ms |
| 数独生成（困难） | < 200ms |
| 包体积（gzipped） | < 200KB JS + 50KB WASM |

## 12. 迁移与回滚

- **数据兼容**：所有旧 `localStorage` key 保留读取，迁移期并行运行
- **代码兼容**：旧 HTML 顶部加"新版上线"横幅，用户点击跳转新版
- **回滚机制**：保留旧 HTML 文件，CI 一键切换（环境变量控制）

## 13. 交付物

- 完整源代码（两个项目的 `src/`）
- 编译后的 WASM（`public/wasm/`）
- 文档：用户手册 + 开发者文档 + 迁移指南
- 测试报告 + 覆盖率报告
- 演示视频 / 截图
- CI/CD 配置

## 14. 时间线与分阶段交付

**交付策略**：MVP 优先。每个阶段都有可演示成果，里程碑可中断。

| 阶段 | 时间 | 交付物 | 演示能力 |
|------|------|--------|----------|
| **MVP-1** | W1-W2 | 设计系统 + WASM 重构 + 2048 完整迁移 | 可玩的 2048 + 现代 UI |
| **MVP-2** | W3-W4 | 贪吃蛇 + 数独 Web 版 | 三个游戏全部可玩 |
| **MVP-3** | W5-W7 | 游戏 web 收尾（暗色主题、动画、撤销、笔记、提示、成就） | 游戏 web 完整功能 |
| **MVP-4** | W8-W9 | 读书雷达基础设施 + 数据层 | 设计系统统一接入 |
| **MVP-5** | W10-W12 | Quiz / Learn / Kanban / Dashboard 迁移 | 核心 4 页可用 |
| **MVP-6** | W13-W14 | 次要页 + 测试 + 文档 | 全部完成 |

**MVP 中断策略**：若 W7 后时间/资源紧张，可交付 MVP-3 作为最终成果，读书雷达改造延后到下个迭代。每个 MVP 都有用户可感知的价值。

## 15. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| WASM 重构引入新 bug | 中 | 高 | 完整单元测试 + E2E 覆盖 |
| 读书雷达迁移丢失功能 | 中 | 高 | 渐进迁移 + 老 HTML 并行 |
| C++ → C 移植出错 | 低 | 中 | 逐函数测试 + 行为对比 |
| 性能退化 | 低 | 中 | 性能预算监控 + 关键路径 benchmark |
| 用户数据丢失 | 低 | 高 | 数据兼容层 + 迁移前快照 |
| 工作量超预期 | 高 | 中 | 分阶段交付 + MVP 优先 |

## 16. 后续工作（不在本 spec 范围）

- WebGPU 渲染（仅在 2048/数独性能瓶颈时考虑）
- 在线排行榜（需后端支持）
- PWA + 离线优先
- AI 对手（贪吃蛇、数独提示升级）
- 多设备同步（需服务端账号系统）

---

**审阅状态**：等待用户审阅