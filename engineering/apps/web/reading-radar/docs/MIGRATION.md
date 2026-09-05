# Migration Guide

本文档记录 reading-radar 从「11 个静态 HTML + window 全局变量」迁移到「React 18 + TS + Vite SPA」的背景、策略与踩坑案例。

目标读者：未来维护者、想了解 v1 → v2 差异的贡献者、以及被历史决策卡住时需要溯源的人。

## 1. 背景

v1（重构前）是一个纯静态学习站点：

- **6 个核心 HTML**：`index.html`（≈ 2141 行）、`quiz-system.html`（≈ 3000 行）、`learning-kanban.html`（≈ 650 行）、`learn.html`（≈ 1200 行）、`dashboard.html`（≈ 800 行）、`five-year-plan.html`（≈ 850 行）。
- **5 个次要 HTML**：`interview.html`、`interview-tracker.html`、`practice.html`、`grok.html`、`excerpt.html`。
- 共 **~30 个数据 / 逻辑 JS 文件**，全部以 `window.X = ...` 或 `const X = ...` 暴露在全局，跨文件依赖靠"先加载顺序"。
- localStorage 持久化状态；可选 `server.js`（Node）把状态序列化到 `user-data/`，支持局域网同步。

痛点：

1. 巨型 HTML（quiz-system.html 单文件 117 KB）难以维护；
2. 全局变量 + 加载顺序约束使重构风险高；
3. 知识点元数据在 3 个文件里重复定义（`quiz-tech.js` / `radar-tech.js` / `kanban-data.js`）；
4. 没有任何类型保护，浏览器控制台即唯一"测试"。

v2 目标：保留 `data/` 内容资产与 `user-data/` 状态格式，UI 层完全重写为 React + TS SPA。

## 2. 页面映射

| Legacy HTML（v1） | 新 SPA 路由（v2） | 入口组件 |
|------|------|------|
| `index.html` | `/` | `src/pages/Home/index.tsx` |
| `quiz-system.html` | `/quiz`、`/quiz/:cat/:item` | `src/pages/Quiz/index.tsx` |
| `learn.html` | `/learn`、`/learn/:cat`、`/learn/:cat/:item` | `src/pages/Learn/index.tsx` |
| `learning-kanban.html` | `/kanban` | `src/pages/Kanban/index.tsx` |
| `dashboard.html` | `/dashboard` | `src/pages/Dashboard/index.tsx` |
| `five-year-plan.html` | `/five-year-plan` | `src/pages/FiveYearPlan/index.tsx` |
| `interview.html` | `/interview` | `src/pages/Interview/index.tsx` |
| `interview-tracker.html` | `/interview-tracker` | `src/pages/InterviewTracker/index.tsx` |
| `practice.html` | `/practice` | `src/pages/Practice/index.tsx` |
| `grok.html` | `/grok` | `src/pages/Grok/index.tsx` |
| `excerpt.html` | `/excerpt` | `src/pages/Excerpt/index.tsx` |

11 个 legacy HTML 在仓库根仍保留，作为**回滚与参考**——`index.html` 因与 SPA 入口同名，已重命名为 `index-legacy.html`。其余 10 个保留原文件名（路径与 SPA 路由不冲突）。

## 3. 数据兼容

### 3.1 `data/` 保持只读

`data/` 目录下所有 legacy 文件（`items-registry.js`、`quiz-tech.js`、`quiz-static.js`、`kanban-data.js`、`radar-tech.js`、`quiz/questions/**/*.js`、`learn-deep/**/*.md`）**均保留原貌**——这是 v1 长期沉淀的内容资产，重写代价巨大。

新 SPA **不直接 import** 这些 CommonJS 文件（`export` 缺失），而是通过 `src/data/*.ts` 访问层读取：

```ts
// src/data/tech.ts
import itemsRegistrySource from '@data/app/items-registry.js?raw';
const body = `${itemsRegistrySource}\nreturn ITEMS_REGISTRY;`;
const fn = new Function(body);
const registry = fn() as RawRegistry;
```

要点：

- **`?raw` 让 Vite 把文件作为字符串引入**，绕过 ESM 解析。
- **`new Function()` 在沙箱中执行**，不会污染全局。
- **追加 `return ITEMS_REGISTRY;`** 是关键——文件本身只是赋值语句，需要把局部 const 暴露出去。
- 异常路径：`src/data/tech.ts` 用了 try/catch 包住求值，解析失败时返回空 registry 并 `console.error`，保证 UI 仍能渲染（空雷达而非白屏）。

题库加载（`src/data/questions.ts`）稍有不同：legacy 文件做的是 `QUESTION_BANK.c = Object.assign(QUESTION_BANK.c || {}, {...})`，需要**外部传入一个可写对象**：

```ts
const fn = new Function('QUESTION_BANK', `${source}\n;`);
fn(bank);   // bank 是访问层持有的一次性 map
```

### 3.2 派生关系（v1 → v2）

v1 的派生图（已**仅作历史参考**——v2 不再消费派生文件）：

```text
ITEMS_REGISTRY (items-registry.js)     ← 唯一定义源
    │
    ├── quiz-tech.js   ──→ C_TECH_DATA / CPP_TECH_DATA / ...
    ├── radar-tech.js  ──→ BOOK_DATA + C_TECH_DATA / ...
    ├── kanban-data.js ──→ C_DATA / CPP_DATA / ...
    └── quiz-static.js ──→ CATEGORIES / QUADRANT_LABELS (独立配置)
```

v2 直接消费 `ITEMS_REGISTRY` 与 `quiz-static.js`，把 `quiz-tech.js` / `radar-tech.js` / `kanban-data.js` 视为**冻结**的快照。如需修改题目元数据，请改 `ITEMS_REGISTRY`（`data/app/items-registry.js`），访问层会自动看到。

### 3.3 URL 命名空间

- `ITEMS_REGISTRY` 的 key 用 **snake_case**（`control_flow`、`ds_basic`）。
- `learn-deep/<cat>/<quadrant>/<cat>-<itemId>.md` 的文件名用 **kebab-case**（`c-control-flow.md`），且带 `<cat>-` 前缀。
- `loadLearnContent` 会尝试三种 candidate：`itemId`、`itemId.replace(/_/g, '-')`、`itemId.replace(/-/g, '_')`，保证两边命名空间都能解析。
- 路由 URL 使用 **registry 原 id**（snake_case 不强制转 kebab），否则 Learn 页的 quadrant 推断会失效。

## 4. 踩坑案例（必读）

### 4.1 `import.meta.glob` 必须 `eager: true` 配合 `?raw`

```ts
const rawLearnFiles = import.meta.glob<string>(
  '@data/learn-deep/**/*.md',
  { query: '?raw', import: 'default', eager: true }  // eager 不能省
);
```

**症状**：忘写 `eager: true`，得到的是 `[path: () => Promise<string>]` 的懒加载 map。在 module init 阶段同步取值时，`load()` 返回的 Promise 还没 resolve，`String.replace` / `JSON.parse` 直接抛 `replace is not a function`，整树白屏。

**解决**：当前所有 `?raw` glob 都用 `eager: true`，配合 `import: 'default'`。代价是首屏会把所有 `*.md` 打进 chunk（MVP-6 体量可接受；语料增长可改 lazy + 按需 fetch）。

### 4.2 SPA fallback 与同名静态 HTML 的冲突

Vite 默认会把请求 URL 优先匹配 `public/` 或根目录的同名静态文件，再走 SPA fallback。

**症状**：v1 时根目录有 `index.html`（legacy SPA 入口）；新建 React SPA 用 `index.html` 启动后，访问 `/quiz` 这种子路由，dev server 仍会 200 返回根 `index.html`，但生产 build 时同样的 URL 可能被根 `index.html` 之外的 legacy 文件（如某些场景下存在过的 `games.js`）抢走。

**解决**：

- `index.html` 已被替换为 SPA 入口（`<script type="module" src="/src/main.tsx"></script>`）。
- `index-legacy.html` 是保留的 v1 SPA 入口。如果未来再遇到冲突，把同名文件改名为 `*-legacy.html`。
- 不要在根目录放 `games.js` / `games.wasm` 这类自动 fetch 的产物——若来自 games/web，请放到 `public/` 下。

### 4.3 跨项目共享代码的相对路径

`@shared` 别名在 reading-radar 下指向：

```ts
// vite.config.ts
'@shared': path.resolve(__dirname, '../../games/web/shared/web/src')
```

**症状**：写成 `'../games/web/shared/web/src'`（1 级 up）会指向 `engineering/apps/web/games/web/shared/...`——错的，`apps/web/` 下没有 `games` 兄弟目录。`tsc` 与 `vite build` 都会失败：路径解析时报 `Cannot find module`。

**解决**：永远是 2 级 up：`../../games/web/shared/web/src`。`tsconfig.json` 的 `paths` 也保持一致：

```json
"@shared/*": ["../../games/web/shared/web/src/*"]
```

### 4.4 CommonJS 数据加载只能用 `?raw` + `new Function()`

legacy 文件没有 `export`、依赖全局对象赋值（`QUESTION_BANK.x = ...`），直接 ESM import 必然失败：

- Vite 会尝试把 `.js` 当 ESM 解析，遇到 `const QUESTION_BANK = {}` 无 export 视为"空模块"；
- `require()` 在 ESM-only 的 Vite 环境里不支持。

**正确做法**：见 §3.1，`?raw` + `new Function('QUESTION_BANK', `${source}\n;`)`。**不要重写数据文件**——CJS 格式是历史约束，与 `server.js` / 其他消费者兼容。

### 4.5 `Question.options` 必须可选

题库里 2693 题中约 676 题（`true_false` / `fill_blank` / `predict_output` 等）**没有 `options` 字段**。`Question.options: string[]` 声明为必填时，访问 `q.options.length` 抛 `Cannot read properties of undefined`——`QuestionCard` 没有 ErrorBoundary，整树白屏。

**修复**（已在 v2 落地）：

- `src/data/types.ts:48` 改为 `options?: string[]`，附 JSDoc 说明；
- `src/components/Quiz/QuestionCard.tsx:72` 加 `q.options && q.options.length > 0` 守卫；fallback 文本展示「（该题无选项，参考答案为：...）」。

> `tsc --noEmit` 编译期即可拦截全部 `.options` 消费点（仅 2 处）。这是为什么 6.3 测试覆盖率 98.34% 还专门覆盖了这条路径。

### 4.6 itemId 命名空间的蛇形 / 烤串桥接

详见 §3.3。简言之：**URL 用 registry 原 id**（snake_case），Markdown 文件名用 kebab，访问层负责双向桥接。**不要**在路由层强制转 kebab，会让 Learn 页的 quadrant 推断失败。

### 4.7 路由 `appType: 'spa'` 与同名静态资源

vite 默认就是 SPA fallback，不需要显式配置 `appType`。但若同一 URL 下存在 `games.js` / `games.wasm` 这类会被自动 fetch 的资源，会触发 dev server 拦截。**不要**在根目录放这类产物。

## 5. 回滚路径

启用 v1 很简单（任何一项即可）：

1. **改入口**：`mv index.html index-spa.html && mv index-legacy.html index.html`，重启 dev server。
2. **临时禁用 SPA 路由**：编辑 `src/router.tsx`，把 `createBrowserRouter` 换成 `createHashRouter` 不解决问题——v1 用的是 hash 路由，但 URL 是 `learn.html#<cat>/<item>` 而非 SPA hash。**正确做法**是直接切回 v1 HTML。
3. **版本控制**：`git checkout <v1-commit> -- *.html data/app/*.js data/quiz/*.js data/quiz/questions/` 可单独回滚数据层（v1 / v2 内容兼容）。

**无需改任何代码**——v1 / v2 共存，`data/` 是只读资产。回滚 = 把入口 HTML 换回去。

## 6. 后续维护的边界

- `data/` 目录下的 legacy JS 文件**可以增量修改**（新增题目 / 新增知识点），但**不要改 schema**（`ITEMS_REGISTRY` 的字段集合、`QUESTION_BANK` 的赋值模式）。改 schema 必须同步改 `src/data/*.ts` 解析逻辑。
- `src/data/*.ts` 是 v2 的访问层，自由修改。改动后必须更新 `tests/unit/data/*.test.ts`。
- `src/components/`、`src/pages/` 是 v2 UI 层，完全自由发挥。
- `server/` 是 v2 新增；v1 的 `server.js` 仍保留（不再维护）。
- `user-data/state/` 下的 JSON 文件是用户运行时产物，建议加入 `.gitignore`（6.3 已记录这条 backlog，未执行）。
