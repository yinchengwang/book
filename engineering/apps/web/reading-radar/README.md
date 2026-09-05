# Reading Radar

读书雷达（Reading Radar）是一个面向 C / C++ / 数据结构 / 数据库内核 / Python / Linux 六大技术栈的本地学习 SPA，覆盖知识雷达、学习内容、题库测评、看板、仪表盘、五年计划等场景。

本仓库是**第二次重构**的结果（v2.0）：从一份无构建工具、由 `window` 全局变量驱动的 11 个静态 HTML（见 `quiz-system.html` / `learn.html` / `dashboard.html` 等），迁移到 **React 18 + TypeScript 5 + Vite 5 + Tailwind 3.4** 的 SPA，并复用 `engineering/apps/games/web/shared/web/src/` 的共享设计系统。底层数据（题库、知识点注册表、学习内容 Markdown）以**只读引用**的方式保留在 `data/` 下，`src/data/*.ts` 作为强类型访问层。

## 技术栈

| 类别 | 技术 |
|------|------|
| 构建 | Vite 5 + @vitejs/plugin-react 4 |
| 语言 | TypeScript 5（strict 模式） |
| UI | React 18 + react-router-dom 6 |
| 样式 | Tailwind CSS 3.4（darkMode: class） |
| 状态 | Zustand 4 |
| Markdown | react-markdown 9 + remark-gfm 4 + rehype-mermaid 3 |
| 服务端 | Express 5（tsx watch 模式） |
| 测试 | Vitest 1.x + @vitest/coverage-v8 1.6 |

共享组件来自 [`engineering/apps/games/web/shared/web/src/`](../games/web/shared/web/src/)：UI（Card / Button）、Markdown、ThemeProvider、safeStorage。详见 [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md) 与 [docs/SHARED-DEPS.md](./docs/SHARED-DEPS.md)。

## 快速开始

### 安装

```bash
cd engineering/apps/web/reading-radar
npm install
```

### 启动开发服务（推荐）

打开两个终端：

```bash
# 终端 1：Vite dev server（HMR + TS 类型检查，端口 5173）
npm run dev

# 终端 2：Express API server（用于 user-data 同步，端口 8080）
npm run server:dev
```

浏览器访问 `http://localhost:5173/`。

> 端口说明：Vite 使用默认端口 5173；Express 默认端口由 `process.env.PORT ?? 8080` 决定。两个 server 完全独立，前端通过 `fetch('/api/state/...')` 在同源下访问（Vite dev 端口需配置 proxy，或在生产由同一静态托管处理；MVP-4 起 Express 仅在 `npm run server:dev` 时单独跑）。

### 命令清单

| 命令 | 作用 |
|------|------|
| `npm run dev` | 启动 Vite dev server（默认 5173 端口，HMR） |
| `npm run preview` | 本地预览 `npm run build` 的产物 |
| `npm run build` | `tsc -b && vite build`（类型检查 + 产物输出到 `dist/`） |
| `npm run test` | 跑 vitest 单元测试 |
| `npm run test -- --coverage` | 跑测试并生成覆盖率报告（口径 `src/data/**`） |
| `npm run server:dev` | 启动 Express API（`tsx watch server/index.ts`，热重启） |
| `npm run server` | 启动编译后的 Express（`node server/dist/index.js`，需先 `tsc`） |

## 目录结构

```text
reading-radar/
├── src/                   # 应用源码（React + TS）
│   ├── pages/             # 11 个页面 + NotFound，每个页面一个子目录
│   ├── components/        # 共享组件（Layout / GlobalSearch / Quiz/*）
│   ├── data/              # 数据访问层（tech.ts / questions.ts / learn.ts / types.ts）
│   ├── router.tsx         # 路由表
│   ├── main.tsx           # 入口：注入 ThemeProvider + 挂载 React
│   └── App.tsx
├── data/                  # 旧静态数据（只读引用，不要直接修改！）
│   ├── app/
│   │   └── items-registry.js  # 知识点唯一定义源
│   ├── quiz/                  # 题库（按 cat + section 拆分）
│   ├── learn-deep/            # 学习内容 Markdown（按 cat/quadrant 组织）
│   ├── quiz-static.js         # CATEGORIES / QUADRANT_LABELS 等静态配置
│   └── quiz-tech.js           # tech metadata（已派生自 items-registry）
├── server/                # Express 服务
│   ├── index.ts           # 入口（挂载 /api/state）
│   ├── api/state.ts       # state REST 路由
│   └── storage/jsonStore.ts # JSON 文件读写 + key 校验
├── tests/unit/            # Vitest 单元测试
│   └── data/              # 数据层单测（tech / questions / learn）
├── user-data/             # Express 运行时的状态/文件持久化目录
├── dist/                  # vite build 产物
├── docs/                  # 项目文档（DEVELOPMENT / MIGRATION / SHARED-DEPS）
└── 旧 HTML 文件           # quiz-system.html / learn.html / ... 11 个 legacy 文件
```

`src/data/*.ts` 是**访问层**（强类型 + 缓存 + 错误处理），`data/` 是**真实数据**（CJS 格式 legacy 库）。前者通过 `import.meta.glob('?raw')` + `new Function()` 沙箱求值读取后者，详见 [docs/MIGRATION.md § 数据兼容](./docs/MIGRATION.md#数据兼容)。

## 页面路由

完整路由表见 `src/router.tsx`。MVP-6.1 起 11 个主要页面已全部迁移：

- `/` 首页（雷达 / 主题切换）
- `/quiz`、`/quiz/:cat/:item` 测评
- `/learn`、`/learn/:cat`、`/learn/:cat/:item` 学习
- `/kanban` 看板
- `/dashboard` 仪表盘
- `/five-year-plan` 五年计划
- `/interview`、`/interview-tracker` 面试题与追踪
- `/practice` 练习
- `/grok` Grok 题库
- `/excerpt` 读书摘录

次要页面挂在顶部导航的「更多 ▾」下拉中（见 `src/components/Layout.tsx`）。

## 关联文档

- [docs/DEVELOPMENT.md](./docs/DEVELOPMENT.md) — 开发指南：添加新页面、修改数据层、测试、本地存储约定。
- [docs/MIGRATION.md](./docs/MIGRATION.md) — 从 11 个 legacy HTML 迁移到 React SPA 的踩坑案例与回滚路径。
- [docs/SHARED-DEPS.md](./docs/SHARED-DEPS.md) — `@shared/*` 跨项目共享组件的宿主依赖声明。

## 语言规范

中文对话、注释、commit message。详见 [`CLAUDE.md`](./CLAUDE.md)。
