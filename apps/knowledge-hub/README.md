# Reading Radar · 知识管理中心（原生微信小程序）

原 `engine/apps/web/knowledge_hub/`（Taro 3.6 + React 18 + Zustand）的**原生小程序重写版**。

- 零依赖、零构建：纯 JavaScript + WXML + WXSS，无 npm、无打包步骤
- 数据全本地：`wx.setStorageSync` + `db:` 前缀，离线可用
- 与 `programmer-workbench` 并列的**独立应用**，共享同一套 `core/` 与 `components/`

## 快速开始

用微信开发者工具「导入项目」，选择本目录（`appid` 默认为 `touristappid`，可换成自己的）。

```bash
node tests/run-all.js    # 跑全部单测 + 静态校验
```

## 目录结构

```
knowledge-hub/
├── app.js / app.json / app.wxss / sitemap.json
├── core/                 # 与 programmer-workbench 共享的运行时
│   ├── id / date / format / storage / store / theme / with-theme
│   ├── md2html.js        # Markdown → HTML
│   ├── md.js             # Markdown → rich-text 内联样式
│   ├── constants.js      # 通用令牌（主题/主色）
│   ├── kh-constants.js   # KH 领域常量（栈/状态/难度…）
│   ├── review.js         # SM-2 间隔重复（纯函数）
│   ├── graph-layout.js   # 知识图谱坐标（纯函数）
│   ├── radar-layout.js   # 技术雷达坐标 + 碰撞（纯函数）
│   ├── seed.js           # 结构性种子（图谱/路径/项目/面试）
│   ├── seed-data.js      # 内容引擎（题库/文章/雷达/五年/题库/摘抄）
│   └── stores.js         # 14 个数据域
├── components/           # card / tag / empty / progress-bar / header / fab
│   ├── knowledge-graph/  # Canvas 2d 关系图
│   └── radar-chart/      # Canvas 2d 气泡图 / 雷达
├── pages/                # 5 tab / 17 页
└── tests/                # validate + 6 个测试套件（140 断言）
```

## 页面地图（5 tab / 17 页）

| tab | 页面 | 说明 |
|---|---|---|
| 首页 | `dashboard` | 学习概览、今日计划、5 项统计、栈进度、待补强 |
| 学习 | `learn` | 4 入口聚合 |
| | `learn/quiz` | 题库练习（5 模式：计划/题库/答题/错题/统计） |
| | `learn/review` | SM-2 复习计划 |
| | `learn/deep` | 图解长文（rich-text） |
| | `learn/path` | 学习路径与子任务 |
| 面试 | `interview` | 3 入口聚合 |
| | `interview/tracker` | 公司追踪、状态流转、时间线 |
| | `interview/bank` | 面试题库（Markdown 答案、收藏/已读） |
| | `interview/mock` | 模拟面试（倒计时 + 规则评分报告） |
| 知识 | `knowledge` | 3 入口聚合 + 能力差距 |
| | `knowledge/graph` | 知识图谱（Canvas + 节点关系） |
| | `knowledge/excerpt` | 摘抄管理 |
| | `knowledge/radar` | 技术雷达（气泡 / 栈雷达 / 差距） |
| 我的 | `profile` | 资料、主题主色、导入导出 |
| 计划 | `plan/five-year` | 五年计划、月历打卡、连续天数 |
| | `plan/roadmap` | 学习项目路线 |

## 数据域（14 个）

存储前缀 `db:`。**派生态 / UI 态一律不持久化**（`dueReviews`、`currentSession`、`activePathId`、`unreadCount` 等改为页面局部 state 或 getter）。

| 键 | 形态 | 说明 |
|---|---|---|
| `daily` | 对象 | 每日学习日志 + 掌握度快照、热力/趋势 |
| `excerpts` | 列表 | 摘抄（书籍/标签/状态/收藏） |
| `five_year_plan` | 对象 | 年→月→日三层树，打卡记录 |
| `interview_bank` | 对象 | 已浏览 / 已收藏题目 ID |
| `interviews` | 列表 | 公司 + 面试时间线 |
| `knowledge` | 列表 | 知识节点（边为静态） |
| `learning_paths` | 列表 | 路径与步骤、`completedSubSteps` |
| `mock_interviews` | 列表 | 模拟面试记录与评分 |
| `notifications` | 内存 | 由复习/打卡派生，不落盘 |
| `study_projects` | 列表 | **学习项目**（非作品集） |
| `quizzes` | 对象 | 答题记录 + 每日计划 |
| `reviews` | 列表 | SM-2 复习调度 |
| `settings` | 对象 | 主题、主色、通知 |
| `user` | 对象 | 用户资料 |

> `study_projects` 特意改名，避免与 `programmer-workbench` 的作品集 `projects`（同名异义）混淆。

## 与原版的差异

| 项 | 原版（Taro） | 原生版 |
|---|---|---|
| 图表 | SVG / Recharts | `<canvas type="2d">`（坐标算法抽为纯函数模块） |
| 路由 | react-router | `wx.navigateTo` / `switchTab` |
| 数据 | Zustand persist（各store 自有键名） | `createListStore` / `createObjectStore` 工厂，统一 `db:` 前缀 |
| 导出 | localStorage + Blob + download | `wx.setClipboardData` + 粘贴导入 |
| 剪贴板 | `navigator.clipboard` | `wx.setClipboardData` |
| 页面 | 14 页 + settings | 17 页（合并 radar+gap、settings→profile，新增 3 个聚合页） |

### 内容数据说明

原版 `@/data/*`（全量题库、396 篇图解正文、雷达数据等）在当前代码库快照中**缺失**，无法还原。
`core/seed-data.js` 提供**结构一致、规模精简**的代表性内容（7 栈 × 5 题型题库、6 篇图解、18 个雷达知识点、5 年计划配置、8 道面试题、4 条摘抄），
足以驱动全部功能；后续拿到原始数据集时替换该文件即可，页面无需改动。

## 测试

```bash
node tests/run-all.js
```

| 套件 | 覆盖 |
|---|---|
| 静态校验 | JS 语法、JSON、页面注册双向一致、组件引用、WXML handler 交叉校验 |
| review | SM-2 间隔递进、低分重置、easiness 下限、到期判断 |
| stores | 14 个域的读写、判分、统计、错题本、打卡、进度 |
| graph | 图谱坐标：分列、半径公式、悬空边、边界 |
| pages | P1：仪表盘 / 复习 / 题库 / 图解 / 路径 |
| pages2 | P2–P4：面试三页 / 知识三页 / 我的 / 五年计划 / 项目路线 |