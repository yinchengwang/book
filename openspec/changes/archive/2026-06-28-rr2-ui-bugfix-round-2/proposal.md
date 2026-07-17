# RR2 UI Bugfix Round 2 — 提案

## Why

`h5-vite-migration` 已归档（2026-06-23），但归档前未对 9 个真实用户问题进行系统性修复。本次新增独立变更，承接这些修复，避免与已归档的 `reading-radar-2-redesign`（20 项任务）混淆。

### 9 个用户反馈的真实问题

1. **浅色主题 UI 不协调** — 右上角切换 light 后页面看不清（背景浅、文字浅、按钮白字）
2. **仪表盘按钮缺 E2E** — 打卡 / 查看详细计划 / 5 个统计卡 / 7 个 stack / 5 年路线全部无自动化测试
3. **Quiz UI 单薄** — list 模式只显示单行卡片，缺少题数、密度信息，与仪表盘 7 stack 卡片视觉差异大
4. **Review 4 统计卡不可点击** — 待复习 / 7 天内 / 已延期 / 已掌握是展示卡，无法点击切换 tab
5. **面试追踪全按钮无 E2E** — 创建/删除/编辑/状态切换全部无测试覆盖
6. **模拟面试全按钮无 E2E**
7. **面试题库全按钮无 E2E**
8. **5 页面 length 崩** — `LearningPath.tsx:113` reduce 内 `s.subSteps.length` 崩、`Excerpt.tsx:62` `ex.tags.includes` 崩
9. **学习路径 / 图解 / 雷达 / 摘抄 / 差距分析 9 项待修**

### 真实根因（前 4 个高优先）

```
LearningPath 崩点：learningPathStore.migrate 保留老 activePath 引用，但老 step 数据缺 subSteps 字段
Excerpt 崩点：老 v1 数据 tags 是 string，新结构是 string[]，excerptStore.migrate 没做类型兼容
```

## What Changes

### 三个新 capability

| Capability | 用途 |
|---|---|
| `light-theme-coverage` | 全局浅色主题样式覆盖，修复所有页面白字/暗背景问题 |
| `quiz-list-ui` | Quiz list 模式改折叠卡片样式 + 密度增强 |
| `e2e-coverage` | 11 个页面的 Playwright E2E 自动化测试覆盖（除已有 3 个 spec） |

### 不在本次范围

- 不改业务逻辑（除必要的 store 兜底）
- 不动 Taro 小程序端编译
- 不动已归档 `h5-vite-migration` 的目录结构
- 不动全局架构（保持双端并存）

## Impact

### 新增文件

```
reading-radar-2/
├── web/styles/light-fixes.scss            # 浅色主题全局覆盖
└── e2e/
    ├── quiz.spec.ts                       # Quiz 4 模式 + 折叠卡
    ├── radar.spec.ts                      # 8 主题切换 + 圆点交互
    ├── excerpt.spec.ts                    # 摘抄 CRUD + 筛选 + 状态机
    ├── learning-path.spec.ts              # 路径切换 + 折叠 + 勾选
    ├── learn-deep.spec.ts                 # 分类筛选 + 搜索 + 详情
    ├── gap-analysis.spec.ts               # 空状态 + 有数据
    ├── settings.spec.ts                   # API key + 主题切换 + 导出
    ├── interview-tracker-full.spec.ts     # CRUD + 阶段推进
    ├── mock-interview-full.spec.ts        # 配置 + 答题 + 反馈
    ├── interview-bank-full.spec.ts        # 分类 + 详情 + 复制
    └── topbar-layout.spec.ts              # Sidebar + TopBar + 主题切换
```

### 修改文件

```
src/stores/learningPathStore.ts            # migrate 强制清空 activePath
src/stores/excerptStore.ts                 # migrate 兜底 tags 为 array
src/pages/review/Review.tsx                # 4 统计卡加 onClick
src/pages/quiz/Quiz.tsx                    # list 模式折叠卡 UI
src/pages/quiz/Quiz.scss                   # 折叠卡样式
openspec/specs/light-theme-coverage/spec.md   # 新建
openspec/specs/quiz-list-ui/spec.md           # 新建
openspec/specs/e2e-coverage/spec.md           # 新建
```

### 不变的部分

- `src/pages/learning-path/` 主体逻辑（仅修 store 兜底）
- `src/pages/excerpt/` 主体逻辑（仅修 store 兜底）
- `src/pages/learn-deep/` / `src/pages/radar/` / `src/pages/gap-analysis/` —— 代码层安全
- `web/` 入口、配置文件不动
- 已归档的 `h5-vite-migration` 不动
```

## Capabilities

### New Capabilities

- `light-theme-coverage`: 全局浅色主题 UI 协调（新增 capability）
- `quiz-list-ui`: Quiz list 模式折叠卡片 UI（新增 capability）
- `e2e-coverage`: 11 页面 Playwright 自动化测试覆盖（新增 capability）

### Modified Capabilities

无。已归档的 `h5-vite-build` / `multi-platform` 不需要修改。

## Verification

按 `reading-radar-2/CLAUDE.md` 强制流程：

```bash
# 1. TS 类型检查
cd /c/code/book/reading-radar-2
bunx tsc --noEmit

# 2. H5 端冒烟
cd web
node scripts/test-pages.cjs    # 期望 12/12 通过

# 3. E2E（新增 11 spec）
cd /c/code/book/reading-radar-2
bun run test:e2e               # 期望 14 spec 全部通过

# 4. H5 构建
bun run build:h5

# 5. 小程序构建
bun run build:weapp
```

新增专项验证：
- 浅色主题：手动截图对照 dark/light 各页面
- 5 页面 length：每个页面打开浏览器无 console 报错

## Risk & Mitigation

| 风险 | 缓解 |
|------|------|
| 浅色主题 CSS 改动破坏 dark 主题 | 仅新增覆盖样式，不改 dark 默认；dark 模式下不应用 light-fixes |
| 折叠卡样式与现有 SCSS 冲突 | 用 BEM 命名隔离 .question-list-item / .question-list-item--expanded |
| Playwright 浏览器启动超时 | 复用 test-pages.cjs 的 headless_shell 路径 |
| 老 v1 数据兼容触发大量迁移 | 学习路径 + 摘抄兜底后用户清 localStorage 即可恢复 |