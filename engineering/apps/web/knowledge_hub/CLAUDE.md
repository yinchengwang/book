# CLAUDE.md - Reading Radar 2.0

This file provides guidance to Claude Code when working with the `knowledge_hub/` subproject.

> ⚠️ 根 `CLAUDE.md` 是 C/C++ 项目的规则。本文件专门定义本子项目的规则。

## 项目语言规范

1. 所有对话、解释、建议必须使用**简体中文**。
2. 代码注释必须使用**中文**。
3. 生成的 Commit Message 必须使用**中文**。
4. 严禁出现大段未翻译的英文技术名词（保留专业术语如 API、SDK 除外）。

## 项目概述

Reading Radar 2.0 是基于 **Taro 3.6 + React 18 + Vite 5** 的 H5 + 微信小程序双端学习追踪平台。
旧版位于 `project/reading-radar/`，新版位于 `knowledge_hub/`。

## 数据存储规范（强制）

**所有内容数据必须以 Markdown 文件存储**，禁止转成 TS/JSON 等二进制格式。

**理由**：
- 用户可直接用 Obsidian 打开 md 文件编辑
- Git 可追踪变更历史
- 保持数据可读性

| 数据类型 | 存储位置 | 格式 |
|----------|----------|------|
| 图解文章（learn_deep） | `src/data/learn_deep/` | `.md` 文件，按技术栈分类 |
| 摘抄（excerpt） | `src/data/excerpt/` | `.md` 文件 |

**Learn Deep 数据源**（S19）：
- 主存储：`../../../learning/notes/learn_deep/{c,cpp,db,ds,grok,illustrate}/*.md`
- 副本：本目录 `src/data/learn_deep/`（Taro 小程序直接消费）
- 同步：仓库根 `learning/scripts/sync-learn-deep.sh`（Phase 2 实现）

| 面试题（interview_bank） | `src/data/interview_bank/` | `.md` 文件 |
| 雷达图数据 | `src/data/radar-data.ts` | TS（配置，非内容） |
| 题库（quiz） | `src/data/quiz-*.ts` | TS（题库数据） |

> ⚠️ **禁止将 md 内容合并成 TS 文件**。如果已有 TS 文件，需拆分回 md。

## 项目铁律（强制）

### 修改源码后的强制验证流程

每次修改源码后，**必须**执行以下验证（按顺序）：

```bash
# 1. TypeScript 类型检查（必跑）
cd c:\code\book\knowledge_hub
bunx tsc --noEmit
```

```bash
# 2. H5 端冒烟测试（12 个页面）
cd c:\code\book\knowledge_hub\web
# 先确保 dev:h5 运行在 5173
node scripts/test-pages.cjs
# 期望: 12/12 ✅
```

```bash
# 3. H5 端构建
cd c:\code\book\knowledge_hub
bun run build:h5
# 期望: 0 错误
```

```bash
# 4. 小程序端构建
cd c:\code\book\knowledge_hub
bun run build:weapp
# 期望: 0 错误
```

```bash
# 5. Playwright E2E（修改了对应页面时）
cd c:\code\book\knowledge_hub
bun run test:e2e
# 期望: 所有 spec 通过
```

**任何一步失败 → 禁止提交**，先修复。

### 自动化测试覆盖率

- 每个页面的核心功能必须在 `e2e/<page>.spec.ts` 中有测试
- 每个 store 的关键 action 必须有断言测试
- `test-pages.cjs` 必须覆盖所有 12 个核心页面

### 文档同步（必做）

- 任何架构变更必须同步更新 `docs/ARCHITECTURE.md`
- 任何新增功能必须更新 `docs/TEST_CASES.md` 中的测试用例
- 任何用户可见的字段变更必须更新 `docs/USER_GUIDE.md`（如有）

### 数据迁移规范

- 迁移老项目数据时，脚本必须放在 `web/scripts/migrate-*.cjs`
- 输出到 `src/data/<name>/initial*.ts`
- 脚本必须打印统计信息（条目数 / 覆盖范围 / 跳过文件）
- 必须在 commit 信息中说明来源

### 持久化升级规范

每个 store 升级 schema 时必须：

```typescript
{
  name: 'xxx-storage',
  version: 2,  // 升级
  migrate: (persistedState: any, version: number) => {
    if (version < 2 && persistedState) {
      // 兼容老数据
      return { ...persistedState, /* 修正字段 */ }
    }
    return persistedState
  }
}
```

## 关键命令速查

```bash
# 启动 H5 开发
bun run dev:h5  # → http://localhost:5173

# 启动微信小程序开发
bun run dev:weapp  # → 用微信开发者工具打开 dist/

# 跑数据迁移
node web/scripts/migrate-excerpt.cjs
node web/scripts/migrate-interview_bank.cjs

# 跑测试
bunx tsc --noEmit              # 类型检查
node web/scripts/test-pages.cjs # 冒烟
bun run test:e2e               # E2E

# 构建
bun run build:h5               # H5 产物：dist/
bun run build:weapp            # 小程序产物：dist/

# Git
git status
git add -A
git commit -m "feat: 中文描述"
```

## 目录速查

| 路径 | 用途 |
|------|------|
| `src/pages/<name>/` | 页面组件（19 个） |
| `src/stores/<name>.ts` | Zustand store（12 个） |
| `src/data/` | 静态数据（题库/摘抄/雷达/路径） |
| `src/data/interview_bank/` | 面试题库数据 |
| `src/data/excerpt/initialExcerpts.ts` | 摘抄迁移数据 |
| `web/components/Layout/` | 布局（TopBar/Sidebar） |
| `web/styles/global.scss` | CSS 变量（主题） |
| `web/config/routes.ts` | 路由 + 动态 badge |
| `web/scripts/` | 数据迁移 + 测试脚本 |
| `e2e/` | Playwright e2e 测试 |
| `docs/ARCHITECTURE.md` | 架构设计文档 |
| `docs/TEST_CASES.md` | 测试用例文档 |

## 文档

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - 架构设计
- [docs/TEST_CASES.md](docs/TEST_CASES.md) - 测试用例
- [docs/h5-deploy.md](docs/h5-deploy.md) - H5 部署
- [docs/weapp-deploy.md](docs/weapp-deploy.md) - 小程序部署

## 老项目数据位置

`c:\code\book\project\reading-radar\data\`

- `excerpt/<year>/*.md` - 摘抄（已迁移 149 条）
- `interview/*.md` - 面试题（已迁移 95 题）
- `practice/*.js` - 算法题（已迁移 1050 题）
- `learn_deep/` - 图解（已迁移 396 篇）
- `user-data/state/*.json` - 用户状态（待迁移）

---

## 🔧 工具位置速查（Windows 环境）

> **本节为强制查阅**：执行任何命令前先确认工具路径。本机 Windows 没有 `node` / `npm`，统一用 `bun` 替代。

### Node / Bun / npm 替代

| 工具 | 路径 | 备注 |
|------|------|------|
| `bun` | `C:\Users\yinch\AppData\Roaming\npm\bun` | **唯一可用的 JS 运行时**，替代 node/npm/npx |
| `node` | ⚠️ **不存在** | 系统无 node.exe，所有命令前加 `bun` |
| `npm` | ⚠️ **不存在** | 同上 |
| `npx` | ⚠️ **不存在** | 用 `bunx` 替代：`bunx <pkg>` |
| `tsc` | 通过 `bunx tsc` 调用 | TypeScript 编译器 |
| `playwright` | `bunx playwright` 或 `node_modules\.bin\playwright` | E2E 测试 |

**正确用法示例**：
```bash
# ❌ 错误
node scripts/test-pages.cjs
npx tsc --noEmit
npm run build

# ✅ 正确
"/c/Users/yinch/AppData/Roaming/npm/bun" "$(cygpath -w "$(pwd)/scripts/test-pages.cjs")"
bunx tsc --noEmit
bun run build
```

### Playwright 浏览器

| 浏览器 | 路径 |
|--------|------|
| **Chromium Headless Shell** | `C:\Users\yinch\AppData\Local\ms-playwright\chromium_headless_shell-1223\chrome-headless-shell-win64\chrome-headless-shell.exe` |
| Chromium | `C:\Users\yinch\AppData\Local\ms-playwright\chromium-1223\` |
| Firefox | `C:\Users\yinch\AppData\Local\ms-playwright\ffmpeg-1011\` |
| MCP Chrome | `C:\Users\yinch\AppData\Local\ms-playwright\mcp-chrome-0f3b1d9\` |

**Windows 已知问题**：`--remote-debugging-pipe` 模式下 Chromium 启动会卡 60s+。
**对策**：HTTP 兜底脚本 `web/scripts/e2e-smoke.cjs`，或 WSL 中跑完整 Playwright。

### OpenSpec CLI

| 工具 | 路径 |
|------|------|
| `openspec.js` | `C:\Users\yinch\AppData\Roaming\npm\node_modules\@fission-ai\openspec\bin\openspec.js` |

**用法**（系统无 node，直接用 bun）：
```bash
"/c/Users/yinch/AppData/Roaming/npm/bun" \
  "/c/Users/yinch/AppData/Roaming/npm/node_modules/@fission-ai/openspec/bin/openspec.js" \
  list --json
```

### 计划文件 / 记忆文件

| 路径 | 用途 |
|------|------|
| `C:\Users\yinch\.claude\plans\` | Plan Mode 计划文件（按 session 命名） |
| `C:\Users\yinch\.claude\projects\c--code-book\memory\` | 项目记忆（每个 fact 一个 .md） |
| `c:\code\book\.remember\` | 滚动 buffer（now.md / today-*.md / recent.md） |

### 其他常用路径

| 资源 | 路径 |
|------|------|
| Vite dev server | `http://localhost:5173` |
| Taro h5 dev (旧) | `http://localhost:10086` |
| Taro CLI 配置 | `c:\code\book\project.config.json` |

---

## 🤖 Claude 协作铁律（基于 rr2-ui-bugfix-round-2 实战经验）

### 铁律 1：源码变更必须同步测试

**任何对页面、组件、store 的修改 → 必须同步更新 `e2e/` 对应的 spec**。

- 修改 `src/pages/quiz/Quiz.tsx` → 改 `e2e/quiz.spec.ts`
- 修改 `src/stores/learningPathStore.ts` → 改相关 spec 的 localStorage 注入
- 新增页面 → 新建 `e2e/<page>.spec.ts`

**新增按钮 = 新增测试断言**（不能遗漏）。

### 铁律 2：功能变更必须更新 BUTTONS.md

**任何按钮的 新增 / 删除 / 修改 / 重命名 → 必须同步更新 [`docs/BUTTONS.md`](docs/BUTTONS.md)**。

`BUTTONS.md` 是 192 个按钮的全量清单，是自动化测试的设计依据。代码改了不更新文档 = 文档与代码脱节 = 下次 E2E 失基线。

**具体场景**：
- 加一个 `<Button onClick={...}>` → BUTTONS.md 加一行
- 删一个 onClick → BUTTONS.md 删一行
- 改一个 onClick 行为 → BUTTONS.md 改一行的"行为"列
- 重命名 className → BUTTONS.md 选择器路径同步更新

### 铁律 3：变更流程必须更新 tasks.md

**OpenSpec 变更的每个任务执行完成后 → 立即更新 `openspec/changes/<name>/tasks.md`**：

- 完成 → `- [x]`
- 新增任务 → 在合适章节加 `- [ ]`
- 状态变化 → 实时同步

**禁止批量勾选**（违反"逐任务执行"原则）。每个子任务结束立刻改 tasks.md。

### 铁律 4：构建验证铁律（每次提交前必跑）

```bash
# 最小验证三件套（按时间从短到长）
bunx tsc --noEmit                                              # 1-2 分钟
node web/scripts/test-pages.cjs  # H5 冒烟，12 页  # 2-3 分钟
bun run build:h5                                              # 3-5 分钟
bun run build:weapp                                           # 5-8 分钟（可选）
```

**任一失败 → 禁止 commit**。先修复，再走完整流程。

### 铁律 5：Windows + Chromium 兼容问题

- `node_modules/.bin/playwright test` 在 Windows 直接跑会卡 Chromium 启动（pipe 通信问题）
- 用 HTTP 兜底：`web/scripts/e2e-smoke.cjs`（验证页面 + 模块能加载 + 编译无错误）
- 完整 Playwright 在 WSL 中跑

### 铁律 6：TS 错误分类处理

```bash
bunx tsc --noEmit  # 看完整错误列表
```

- **本次改的文件报错** → 必须修复
- **预存在的错误**（如 `dataService.ts`、`NotFound.tsx` 双 default export、`@tarojs/runtime` 模块解析）→ 标注 TODO，不阻塞提交
- **旧 Taro 组件 import 错误**（小程序端才有，Vite 端走适配层）→ 可忽略

### 铁律 7：OpenSpec 同步流程

修改 capability 时：

1. 在 `openspec/changes/<name>/specs/<cap>/spec.md` 写 delta（ADDED/MODIFIED/REMOVED/RENAMED）
2. 实现完代码后用 `/opsx:sync <name>` 同步到 `openspec/specs/<cap>/spec.md`
3. 用 `/opsx:archive <name>` 归档

**禁止直接改 main specs 而不走 change 流程**。

---

## 📝 常见误操作清单（务必避开）

| 误操作 | 后果 | 正确做法 |
|--------|------|----------|
| `node xxx.cjs` | "node: not found" | 用 `"/c/Users/yinch/AppData/Roaming/npm/bun" "$(cygpath -w "$(pwd)/xxx.cjs")"` |
| `npx playwright test` | 同上 + 浏览器启动超时 | HTTP 兜底 / WSL |
| `npm run dev:h5` | "npm: not recognized" | `bun run dev:h5` |
| `taskkill /F /IM chrome.exe` | **被沙箱拒绝**（会杀用户浏览器） | 提示用户手动关 |
| 改代码不更新 BUTTONS.md | 文档与代码脱节 | 每次 button 改动同步 BUTTONS.md |
| 改代码不更新 spec | 测试覆盖缺失 | 同步更新 e2e/<page>.spec.ts |
| 批量勾选 tasks.md | 失"逐任务"原则 | 每个任务完成立刻勾 |
