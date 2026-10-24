# programmer-workbench 原生小程序重写 · 设计方案

> 日期：2026-10-22
> 目标：把 `apps/programmer-workbench/`（Taro 3.6 + React 18 + Zustand）重写为**原生微信小程序**，功能完整对等。
>
> **⚠️ 2026-10-22 更新**：UI 层改为**全新设计**，不再照搬旧版视觉。视觉规范、信息架构与页面布局以
> [`2026-10-22-devbench-ui-design.md`](./2026-10-22-devbench-ui-design.md) 为准。本文档的「功能对等」仍有效
> （18 页 / 5 tab / 10 工具 / 12 数据域），但「样式搬运」策略作废，改为按新设计语言重建 WXSS。

---

## 1. 背景与目标

### 现状

`programmer-workbench`（DevBench · 程序员个人工作台）目前是 Taro 3.6 + React 18 + TypeScript + Zustand 4 实现：

- **4190 行** TS/TSX，18 个页面、5 个 tab、12 个 store、10 个开发工具
- 构建链：Taro → Webpack 4 → `dist/`，需 `pnpm install`，Node 17+ 还要 `NODE_OPTIONS=--openssl-legacy-provider`
- README 记录了 7 个「已修复的环境坑位」，其中 tabBar 图标缺失、workspace 包不在 registry、Webpack 4 + 新 Node 不兼容等，**全部是构建链带来的**

### 目标

改为**纯原生小程序**（`.wxml` / `.wxss` / `.js` / `.json`）：

- 微信开发者工具直接打开即运行，无需 `npm install`、无构建步骤
- 功能完整对等：18 页 / 5 tab / 10 工具 / 12 数据域 / 双主题
- 消除全部构建链问题（上述 7 个坑随之消失）

### 已确认的决策

| 决策项 | 结论 |
|---|---|
| 功能范围 | **完整对等**，18 页全做，不精简 |
| 存储兼容 | **换新前缀 `db:`**，不读旧 `devbench:` 数据 |
| 主题 | **双主题（浅色/深色/跟随系统）+ 主色可换**，完整保留 |
| 种子数据 | **保留**首次启动写入示例内容 |
| 数据层架构 | **统一 `createStore` 工厂**，12 个域各自声明 key 与默认值 |

---

## 2. 目录结构

原地重写 `apps/programmer-workbench/`。

**删除**：`src/`、`dist/`、`.swc/`、`package.json`、`pnpm-lock.yaml`、`babel.config.js`、`config/`、`tsconfig.json`、`.npmrc`

**保留并改写**：`project.config.json`（`miniprogramRoot` 改为 `./`）、`README.md`

```
programmer-workbench/
├── app.js                    # onLaunch: 主题初始化 + 种子数据
├── app.json                  # 18 页注册 + 5 tab（纯文字，零图标资源）
├── app.wxss                  # 主题变量 + 全局组件类
├── project.config.json
├── project.private.config.json
├── sitemap.json
├── core/                     # 无 UI 的纯逻辑层，可单测
│   ├── storage.js            # 底层读写 + 导入导出（db: 前缀）
│   ├── store.js              # createStore 工厂（CRUD + 持久化）
│   ├── theme.js              # 主题解析 / 应用 / 系统跟随
│   ├── seed.js               # 种子数据（11 类示例内容）
│   ├── codetools.js          # 10 个开发工具（纯函数，原样移植）
│   ├── date.js               # todayStr / weekRange / formatRelative
│   ├── format.js             # percent / duration
│   └── id.js                 # genId
├── components/
│   ├── card/                 # 通用卡片
│   ├── tag/                  # 标签
│   ├── empty/                # 空状态
│   ├── progress-bar/         # 进度条
│   ├── header/               # 页头（含返回）
│   └── fab/                  # 悬浮新建按钮
├── pages/                    # 18 页，每页 4 文件
└── tests/
    └── core.test.js          # 核心逻辑回归测试（Node 环境）
```

---

## 3. 数据层设计

### 3.1 存储

前缀 `db:`，12 个 key（去掉旧版的 `devbench:` 前缀与冗余命名）：

```
db:todos  db:tech_notes  db:snippets  db:interview  db:resources
db:reading  db:fitness  db:mood  db:schedule  db:projects
db:insights  db:settings  db:__seeded__
```

`core/storage.js` 提供：`get / set / remove / clearAll / exportAll / importAll`。

导入导出格式沿用原设计（`{ exportedAt, version, data }`），支持 `merge` / `overwrite` 两种模式——这是「我的」页的备份恢复功能，属于对等范围。

### 3.2 store 工厂

原版 12 个 Zustand store 共 590 行，模式高度一致（`Storage.get` 初始化 + `persist` 写回）。原生版抽成一个工厂：

```js
// core/store.js
createListStore({ key, defaultValue: [] })  // → getAll / setAll / add / update / remove / clear
createObjectStore({ key, defaultValue })     // → get / patch / reset（settings 用）
```

每个域只需声明 key 与默认值，例如：

```js
// core/stores.js
const todos = createListStore({ key: 'todos' })
module.exports = { todos, notes, snippets, ... }
```

页面调用 `stores.todos.getAll()` / `stores.todos.update(id, patch)`，每次写操作同步落盘并返回最新数组，页面自行 `setData`。

这样 12 个域约 150 行即可覆盖，且与 Zustand 的调用手感接近。

### 3.3 主题

**这是原生改造最大的结构性差异。** 原版用 `[data-theme='dark']` 选择器驱动 CSS 变量，而原生小程序的 WXSS 不支持属性选择器。

替代方案：

1. 变量定义在 class 下：`.theme-light { --bg-primary: #f8fafc; ... }` / `.theme-dark { ... }`
2. 每个页面根 view 挂 `class="page {{themeClass}}"`
3. 主色通过行内变量覆盖：`style="--brand: {{primaryColor}}"`
4. `core/theme.js` 负责解析：`auto` → 读 `wx.getSystemInfoSync().theme`，并订阅 `wx.onThemeChange`
5. 页面通过 `withTheme(pageOptions)` 包装器注入 `onShow` / 主题变更回调，避免 18 个页面各写一遍

---

## 4. 页面清单（18 页）

| # | 路径 | 类型 | 说明 |
|---|---|---|---|
| 1 | `pages/dashboard/index` | Tab | 工作台：问候 + 4 项统计 + 待办 + 8 快捷入口 |
| 2 | `pages/tech/index` | Tab | 技术栈聚合（4 个入口 + 计数） |
| 3 | `pages/life/index` | Tab | 生活聚合（4 个入口 + 计数） |
| 4 | `pages/tools/index` | Tab | 工具箱（10 工具切换面板） |
| 5 | `pages/profile/index` | Tab | 我的：资料 / 主题 / 主色 / 导出恢复 / 清空 |
| 6 | `pages/tech/notes/index` | 子页 | 笔记列表 |
| 7 | `pages/tech/notes/detail` | 子页 | 笔记编辑 + Markdown 预览 |
| 8 | `pages/tech/snippets/index` | 子页 | 代码片段列表 |
| 9 | `pages/tech/snippets/detail` | 子页 | 片段详情 / 编辑 |
| 10 | `pages/tech/interview/index` | 子页 | 面试题列表 |
| 11 | `pages/tech/interview/detail` | 子页 | 面试题详情 |
| 12 | `pages/tech/resources/index` | 子页 | 资源收藏 |
| 13 | `pages/life/reading/index` | 子页 | 读书 |
| 14 | `pages/life/fitness/index` | 子页 | 健身记录 |
| 15 | `pages/life/mood/index` | 子页 | 心情日记 |
| 16 | `pages/life/schedule/index` | 子页 | 日程 |
| 17 | `pages/projects/index` | 子页 | 项目作品集 |
| 18 | `pages/insights/index` | 子页 | 闪念便签 |

tabBar 保持**纯文字 5 tab**（工作台/技术栈/生活/工具箱/我的），零图标资源——沿用原版 2026-10-19 的修复方案。

---

## 5. 工具箱（10 个工具）

`core/codetools.js` 是纯函数模块，**几乎原样移植**（仅去掉 TS 类型标注）：

🧾 JSON 格式化 · 🔤 Base64 编解码 · 🔗 URL 编解码 · ⏰ 时间戳互转 · 🆔 UUID 生成 · 🧪 正则测试 · 🎨 颜色转换（HEX↔RGB）· #️⃣ Hash（djb2/crc32）· 🔁 文本 Diff · ⏳ Cron 解析

`pages/tools/index` 用单个页面 + 工具切换（原版即如此），每个工具的面板用 `wx:if` 切换。

---

## 6. 关键技术映射

| 原实现 | 原生写法 |
|---|---|
| `Taro.showToast / showModal / showActionSheet` | `wx.showToast / showModal / showActionSheet` |
| `Taro.navigateTo / navigateBack / setNavigationBarTitle` | 同名 `wx.*` |
| `Taro.setClipboardData` | `wx.setClipboardData` |
| `Taro.getCurrentInstance().router.params.id` | `onLoad(options)` 取 `options.id` |
| `Taro.getSystemInfoSync` | `wx.getSystemInfoSync` |
| `<RichText nodes={md2html(md)} />` | `<rich-text nodes="{{html}}" />`（`md2html` 直接复用） |
| `[data-theme]` CSS 变量 | class 作用域 + 行内 `--brand` 覆盖 |
| `dangerouslySetInnerHTML` 注入 `<style>` | 移入对应页面的 `.wxss` |
| React `useState` / `setData` | 页面 `data` + `this.setData` |
| `useMemo` 派生统计 | `onShow` 中计算后 `setData` |
| 长列表 | `<scroll-view scroll-y>` |
| `Input` / `Textarea` | `<input>` / `<textarea bindinput>` |

---

## 7. 主要风险

### R1：样式需要整体重建（最大工作量，但性质已变）

18 个页面共约 470 个内联 style 对象，`pages/tools/index.tsx` 单文件 101 处。

**由于 UI 改为全新设计，这 470 处不再是"搬运对象"，而是"作废重写"。** 工作量性质从「机械翻译」变为「按设计语言重新表达」，好处是不必迁就旧版的圆角/字阶/间距混乱，坏处是不能靠对照旧代码来校验视觉还原度。

策略：

- **设计令牌先落地**：`app.wxss` 里先定义好色彩/字阶/间距/圆角变量与 6 个通用组件类（卡片/标签/按钮/列表项/FAB/空态）
- **共享模式进全局**：卡片、列表项、标签、按钮、FAB、空状态 → `app.wxss`
- **页面一次性样式**：该页 `.wxss` 中的语义化类
- **真动态值**：进度条百分比、分类色、优先级色、星级 → WXML 行内 `style` 绑定 data

### R1b：tabBar 是否自定义

新设计的 tabBar 带选中指示条与品牌色文字。原生 tabBar 只能改 `color` / `selectedColor`，做不出指示条。

**决策：首版用原生 tabBar（纯文字 + 品牌色选中态），零风险；自定义 tabBar 列为可选升级项。** 理由：自定义 tabBar 需 `custom: true` + 组件 + 每页 `getTabBar()` 同步选中态，是已知的坑位密集区，不该阻塞首版交付。

### R2：主题变量作用域

需验证 `style="--brand: #xxx"` 在真机的变量继承是否稳定。若不稳定，退化为「预设 5 个主色 class」（`.brand-sky` / `.brand-violet` …），损失自定义任意色能力但保证可靠。

### R3：表单与输入体验

原生 `<textarea>` 是原生组件，层级最高，会盖住弹层。笔记编辑页的 Markdown 预览若用弹层，需改用页面内切换（原版就是 `preview` 状态切换，天然规避）。

### R4：`scroll-view` 高度

原生页面滚动与 `scroll-view` 混用易出现双滚动条。约定：tab 页用页面级滚动，子页长列表用 `scroll-view` 并显式设高。

---

## 8. 实施阶段

| 阶段 | 内容 | 产出 |
|---|---|---|
| **P0** | 骨架：设计令牌落地（`app.wxss` 变量 + 6 个通用类）、`app.*`、`core/*`、6 个组件、主题体系 | 可运行空壳，tab 可切换，组件可预览 |
| **P1** | 工具箱（10 工具，搜索 + 全屏面板）+ `tests/core.test.js` | 最快验证组件与样式体系，纯函数易测 |
| **P2** | 今日页 + 我的 | 验证数据层与多 store 聚合，落地首屏设计 |
| **P3** | 技术栈 4 模块（列表 + 详情，8 页） | 列表卡范式定型，后续复用 |
| **P4** | 生活 4 模块（4 页） | 复用 P3 范式 |
| **P5** | 项目集 + 闪念（2 页）+ 全局捕获浮层 | 补全 18 页 |
| **P6** | 测试补齐 + README 重写 + 删除旧 `src/`、`dist/` | 交付 |

每阶段结束跑一次全量校验，不通过不进入下一阶段。

---

## 9. 验收标准

1. `node --check` 全部 `.js` 通过
2. 全部 `.json` 可 `JSON.parse`
3. `tests/core.test.js` 全绿，覆盖：10 个工具函数的确定性输出、日期/格式化工具、store 工厂 CRUD、存储导入导出、主题解析
4. `app.json` 注册的 18 页与磁盘文件一一对应，无缺失、无多余
5. **WXML handler 交叉校验脚本**：扫描每个页面的 `.wxml` 里 `bindtap` / `bindinput` / `catchtap` 等绑定的方法名，与对应 `.js` 中的方法比对，缺失即报错（这类错误微信开发者工具只在运行时才提示，容易漏）
6. 每页 4 文件齐全（`.js` / `.wxml` / `.wxss` / `.json`）

---

## 10. 不做什么（YAGNI）

- 不引入任何 npm 依赖（原版唯一的运行时依赖是 Zustand，已被 store 工厂替代）
- 不做自定义 tabBar（纯文字 tabBar 够用且零资源）
- 不保留 H5 端构建能力（原版可 `build:h5`，但本项目定位是小程序）
- 不迁移旧 `devbench:` 数据（已确认换前缀）
- 不实现原版 `enableDailyReminder` 的实际推送（原版也只是存了个开关，没有真实订阅消息逻辑）
