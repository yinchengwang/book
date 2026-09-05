# Development Guide

本文档面向在 `engineering/apps/web/reading-radar/` 上做日常开发的工程师，覆盖：添加新页面、修改数据层、复用共享组件、本地存储约定、测试约定。

## 1. 添加新页面

### 1.1 创建组件

页面文件位置约定为 `src/pages/<Name>/index.tsx`（小目录 + 入口文件，便于同一页面内多个子组件共存）。一个最小页面示例：

```tsx
// src/pages/MyNewPage/index.tsx
import { Card } from '@shared/ui/Card';

export function MyNewPage() {
  return <Card className="p-6">My new page</Card>;
}
```

页面组件**必须是 default export 之外的具名 export**（方便 `router.tsx` 与未来的 lazy import 引用）。所有页面通过 `Layout` 的 `<Outlet />` 渲染，不需要自己写 `<html>` / `<body>`。

### 1.2 注册路由

打开 `src/router.tsx`，在 `children` 数组中加一行（保持路由按字母 / 业务分组排列）：

```tsx
import { MyNewPage } from '@/pages/MyNewPage';
// ...
{ path: 'my-new-page', element: <MyNewPage /> },
```

带参数的路由（参见 `/quiz/:cat/:item`、`/learn/:cat/:item`）直接用 react-router-dom 6 的 path params 语法即可。

### 1.3 加入导航

`src/components/Layout.tsx` 维护两份导航清单：

- `navItems` — 顶部 4 个主入口（测评 / 学习 / 看板 / 仪表盘）。新核心页面放这里。
- `moreItems` — 「更多 ▾」下拉收纳次要页面。每条形如 `{ to: '/my-new-page', label: '🆕 我的页面' }`。

放进 `moreItems` 后，`NavLink` 会自动处理 active 样式与 `<Outlet>` 的路由匹配；不需要额外动作。

### 1.4 复用共享组件

`@shared/*` 别名（见 `vite.config.ts` 与 `tsconfig.json`）指向 `../../games/web/shared/web/src/`，**2 级 up**（不要写成 1 级，否则会指向 `engineering/apps/web/shared/...` 这种错误路径）。当前可用的子模块：

| 路径 | 用途 |
|------|------|
| `@shared/ui/Card` | 卡片容器 |
| `@shared/ui/Button` | 按钮 |
| `@shared/components/Markdown` | Markdown 渲染（基于 react-markdown） |
| `@shared/theme/ThemeProvider` | 主题 Provider（在 `src/main.tsx` 已挂载，页面里 `useTheme()` 即可） |
| `@shared/storage/safeStorage` | 安全的 localStorage 读写 |

主题由 `ThemeProvider` 通过 `class="dark"` 切换；Tailwind 的 `dark:` 前缀会自动生效。

> 共享组件的宿主依赖（`react-markdown`、`remark-gfm`、`@tailwindcss/typography` 等）必须在**本项目**的 `package.json` 单独安装，Vite 不会跨项目继承。详见 [./SHARED-DEPS.md](./SHARED-DEPS.md)。

## 2. 修改数据层

数据层是 `src/data/` 下的 4 个文件：

- `types.ts` — 共享类型（`TechCategory`、`TechItem`、`Question`、`QuestionBank`）。
- `tech.ts` — 知识点注册表的加载器（从 `data/app/items-registry.js` 取）。
- `questions.ts` — 题库加载器（从 `data/quiz/questions/<cat>/quiz-questions-<cat>-<section>.js` 取）。
- `learn.ts` — 学习内容 Markdown 加载器（从 `data/learn-deep/<cat>/<quadrant>/<cat>-<itemId>.md` 取）。

### 2.1 新增一个 tech category（如 `rust`）

1. 在 `src/data/types.ts` 的 `TechCategory` 联合类型中加 `'rust'`：
   ```ts
   export type TechCategory = 'c' | 'cpp' | 'ds' | 'db' | 'py' | 'linux' | 'vdb' | 'grok' | 'rust';
   ```
2. 在 `src/data/tech.ts` 的 `loadAllTechItems` 返回 map 中加 `rust: []`。
3. 在 `data/app/items-registry.js` 的 `ITEMS_REGISTRY` 中添加以 `rust` 为 `stack` 的若干条目。
4. 如有题库，新建 `data/quiz/questions/rust/quiz-questions-rust-<section>.js`，并确保文件名匹配 `import.meta.glob('@data/quiz/questions/*/quiz-questions-*-*.js')`（见 `src/data/questions.ts`）。
5. 测试：在 `tests/unit/data/tech.test.ts` 加对应断言。

### 2.2 新增一个知识点

只需修改 `data/app/items-registry.js` 一处。`quiz-tech.js` / `kanban-data.js` / `radar-tech.js` 会从 registry 派生（这些文件在新版 SPA 中已不再被消费；当前访问层直接走 registry）。

如果该知识点还需要学习内容，新建 `data/learn-deep/<cat>/<quadrant>/<cat>-<itemId>.md`。文件名格式见 `src/data/learn.ts` 顶部的注释（`<cat>-` 前缀是历史约束，**不能漏**）。

### 2.3 数据文件格式

#### items-registry.js（CommonJS，IIFE）

```js
"use strict";
const ITEMS_REGISTRY = {
  "syntax": { stack:"c", quadrant:"language", ring:"basic", title:"...", desc:"...", tags:["..."] },
  // ...
};
```

文件无 `export` 语句——这是为什么不能直接 `import`，必须用 `?raw` + `new Function()` 沙箱求值（见 `src/data/tech.ts`）。

#### quiz-questions-<cat>-<section>.js（CommonJS）

```js
"use strict";
QUESTION_BANK.c = Object.assign(QUESTION_BANK.c || {}, {
  pointer: [
    {
      id: "pointer-q1",
      type: "choice",
      difficulty: "intermediate",
      scenario: "...",
      stem: "题干",
      code: "可选",
      options: ["A. ...", "B. ..."],
      answer: "A",
      explanation: "..."
    }
  ]
});
```

注意：`type: "true_false"` / `"fill_blank"` 的题目**不写 `options` 字段**；`QuestionCard.tsx` 会自动展示 fallback（见 `src/components/Quiz/QuestionCard.tsx`）。在 `src/data/types.ts` 中 `Question.options` 是可选字段。

#### learn-deep/<cat>/<quadrant>/<cat>-<itemId>.md

纯 Markdown，文件名必须带 `<cat>-` 前缀（如 `c-pointer.md`）。每篇遵循「说人话开场白 → ASCII 图解 → 对比表 → 面试追问 → 代码示例 → 一句话总结」六段式结构（参见既有文件）。

## 3. localStorage 约定

**禁止**直接 `localStorage.getItem / setItem`——可能抛异常（隐私模式、quota 超限、SSR 等）。统一使用：

```ts
import { safeGet, safeSet } from '@shared/storage/safeStorage';

safeSet('rr:kanban:state-v1', { c: { syntax: 'done' } });
const state = safeGet('rr:kanban:state-v1', {} as Record<string, unknown>);
```

Key 命名规则：**`rr:<scope>:<key>`**（rr = reading-radar），如 `rr:kanban:state-v1`、`rr:quiz:last-cat`、`rr:excerpt:groupings`。

> 版本字段（`-v1`、`-v2`）：当 schema 变更时升级版本号，旧的 key 自然失效，不需要写迁移代码。

## 4. 服务端状态（Express API）

`server/index.ts` 启动后暴露 `GET /api/state/:key` 与 `PUT /api/state/:key`，把 JSON 写入 `user-data/state/<key>.json`。Key 必须匹配 `/^[a-zA-Z0-9_-]+$/` 且 ≤ 200 字符（见 `server/storage/jsonStore.ts`）。

常见用途：跨设备同步本地状态、未来接入服务端学习进度。MVP-4 起 Express 仅在显式 `npm run server:dev` 时启动，不参与 Vite dev 主流程。

## 5. 测试

- 配置文件：`vite.config.ts` 的 `test` 字段（Vitest 1.x）。
- 测试位置：`tests/unit/<area>/*.test.ts`（当前仅 `tests/unit/data/`）。
- 覆盖率口径：`src/data/**`（UI 组件不在本任务范围）。
- 命名：与被测文件同名（`tech.ts` → `tech.test.ts`）。

跑测试：

```bash
npm test                  # watch 模式（开发时）
npm run test -- --coverage # 一次性跑 + 覆盖率报告
```

新增数据加载器时务必补一个 `*.test.ts`，至少覆盖：

- 正常路径：能取到预期数据；
- 边界：空 registry、空 category、缓存是否生效；
- 异常：源文件不存在 / 解析抛错时是否降级（`safeGetRegistry` 之类的 fallback 路径）。

## 6. 构建 / 部署

```bash
npm run build   # tsc -b + vite build → dist/
npm run preview # 本地预览 dist/
```

`dist/` 是纯静态产物（HTML / JS / CSS / 资产），可丢到任意静态托管。Express API (`server/dist/`) 与静态资源**通常分开部署**——MVP-4 没有提供 `Dockerfile` 或 `pm2` 配置，部署由运维侧自行组合。

构建时 `import.meta.glob('?raw')` 会把所有 `data/quiz/questions/**/*.js` 与 `data/learn-deep/**/*.md` 打进 bundle（eager）。当前体量无碍；语料增长后考虑改 lazy。

## 7. 路由 vs 静态资源的冲突

vite 默认把同名静态文件优先于 SPA fallback。若 `public/` 或根目录存在 `games.js` / `games.wasm` 这类产物，Vite dev 会拦截 `/games.js` 请求，路由失效。**当前根目录仍保留 11 个 legacy HTML**（`quiz-system.html` / `learn.html` 等），它们与 SPA 路由不冲突（路径不同）；但若未来新增同名静态资源，请放在 `public/` 并注意这一点。

详细背景见 [./MIGRATION.md](./MIGRATION.md)。
