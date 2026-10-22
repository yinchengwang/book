# knowledge-hub 原生重写设计

> 目标：把 `engineering/apps/web/knowledge_hub/`（Taro 3.6 + React 18 + Zustand，H5+小程序双端）
> 重写为**原生微信小程序**，与 programmer-workbench 并列，共享同一套 core 代码层。
>
> 决策日期：2026-10-22

---

## 1. 为什么不做成「一个大而全的应用」

评估过把 knowledge-hub 并入 programmer-workbench，结论是**不合并应用，只共享代码层**：

| 约束 | 说明 |
|---|---|
| **tabBar 上限 5 项** | 微信原生硬限制。PWB 已用满 5（今日/技术/生活/工具/我的），KH 也用满 5（首页/复习/面试/路径/设置）。合并 = 10 个 tab 挤进 5 个，必须重做两边的信息架构 |
| **产品心智模型不同** | PWB 是「个人工作台」——管生活（读书/健身/心情/日程）与工作（待办/笔记/工具）；KH 是「知识管理与学习系统」——管学习路径、复习调度、能力雷达、五年计划。打开时应该知道自己来干什么 |
| **体量翻倍** | 合计 22K 行、32+ 页面。KH 的 mock_interview、knowledge_graph、five_year_plan 都是重模块 |
| **主包 2MB 限制** | 合并后接近上限，需引入分包，徒增复杂度 |

### 但有 4 个数据域是真重叠

| 域 | KH store | PWB 域 | 处置 |
|---|---|---|---|
| 面试题 | `interviewBankStore` + `interviewStore` | `interview` | KH 侧复用 PWB 实现 |
| 摘抄/闪念 | `excerptStore` | `insights` | KH 侧复用 PWB 实现 |
| 设置 | `settingsStore` | `settings` | 同一套 `createObjectStore` |
| 用户资料 | `userStore` | `profile` | KH 侧复用 PWB 实现 |

### 陷阱：同名不同义

```
KH  projectStore  = 学习项目追踪   {status: 规划中/进行中/已完成, priority, knowledgeLinks}
PWB projects      = 作品集展示     {techStack, repoUrl, demoUrl, stars, highlights}
```

名字一样语义完全不同。KH 侧改名为 `study_projects`，避免混淆。

---

## 2. 共享代码层

复制自 programmer-workbench，**逐字节相同**（不修改即为共享，修改即分叉）：

```
core/
├── id.js          # 短 ID 生成
├── date.js        # 日期工具（含 weekRange / humanDay / greeting）
├── format.js      # clamp / percent / duration / bytes / truncate
├── md2html.js     # Markdown → HTML 解析器（本次从 codetools 抽出的独立模块）
├── md.js          # Markdown → rich-text 内联样式 + 纯文本摘要
├── storage.js     # db: 前缀存储封装 + 导入导出
├── store.js       # createListStore / createObjectStore 工厂
├── theme.js       # 主题解析 + 导航栏/tabBar 同步
├── with-theme.js  # 页面主题包装器
└── constants.js   # 分类/状态映射（KH 在此基础上扩展）

components/
├── card / tag / empty / progress-bar / header / fab
└── capture-sheet   # 全局捕获浮层（KH 侧按需接入）

tests/
├── validate.js / wx-stub.js / page-harness.js / run-all.js
```

### KH 不共享的部分

| 模块 | 原因 |
|---|---|
| `codetools.js` | 10 个开发工具是 PWB 工具箱专属，KH 无此板块 |
| `seed.js` | 种子数据完全不同 |
| `stores.js` | 14 个数据域与 PWB 的 12 个不同 |

> 重构说明：`md2html` 原在 `codetools.js` 内，本次抽出为独立模块，
> 使 KH 复用 Markdown 解析时不必连带引入整个工具箱。

---

## 3. 页面架构（5 tab / 17 页）

tabBar 上限 5 项，按 KH 的四个功能域 + 我的划分：

```
首页          学习            面试            知识            我的
dashboard     learn(聚合)      interview(聚合) knowledge(聚合) profile
              ├ quiz           ├ tracker       ├ graph
              ├ review         ├ bank          ├ excerpt
              ├ deep           └ mock          ├ radar
              └ path                           └ (gap 并入 radar)
                                              plan/five-year
                                              plan/roadmap
```

| # | 页面 | 路径 | 来源 | 说明 |
|---|---|---|---|---|
| 1 | 仪表盘 | `pages/dashboard/index` | `/` | tab。学习概览、今日任务、5 项统计、技术栈进度 |
| 2 | 学习 | `pages/learn/index` | 新增 | tab。4 入口聚合 |
| 3 | 题库练习 | `pages/learn/quiz/index` | `/quiz` | 29 按钮，最重。分类题库、答题、错题本 |
| 4 | 复习计划 | `pages/learn/review/index` | `/review` | **SM-2 间隔重复**、到期队列 |
| 5 | 图解系列 | `pages/learn/deep/index` | `/learn_deep` | Markdown 文章阅读器（复用 md.js） |
| 6 | 学习路径 | `pages/learn/path/index` | `/learning_path` | 路径节点、进度、依赖 |
| 7 | 面试 | `pages/interview/index` | 新增 | tab。3 入口聚合 |
| 8 | 面试追踪 | `pages/interview/tracker/index` | `/interview_tracker` | 20 按钮。面试记录、状态、反馈 |
| 9 | 面试题库 | `pages/interview/bank/index` | `/interview_bank` | 复用 PWB interview 范式 |
| 10 | 模拟面试 | `pages/interview/mock/index` | `/mock_interview` | 模拟场景、评分、报告 |
| 11 | 知识 | `pages/knowledge/index` | 新增 | tab。4 入口聚合 |
| 12 | 知识图谱 | `pages/knowledge/graph/index` | `/knowledge_graph` | 节点关系。小程序侧用 Canvas 或简化列表 |
| 13 | 摘抄管理 | `pages/knowledge/excerpt/index` | `/excerpt` | 16 按钮。复用 PWB insights 范式 |
| 14 | 技术雷达 | `pages/knowledge/radar/index` | `/radar` + `/gap_analysis` | 两页合并（gap_analysis 仅 2 按钮） |
| 15 | 我的 | `pages/profile/index` | `/settings` | tab。资料 + 主题 + 数据管理（同 PWB） |
| 16 | 五年计划 | `pages/plan/five-year/index` | `/five_year_plan` | 年度目标、里程碑、streak |
| 17 | 项目路线 | `pages/plan/roadmap/index` | `/project_roadmap` | 学习项目（非作品集） |

### 与原版的页面映射差异

- **合并**：`radar` + `gap_analysis` → `knowledge/radar`（gap 仅 2 按钮，并入雷达页做「差距」分区）
- **新增**：3 个聚合页（learn / interview / knowledge），沿用 PWB 的 tab 内导航模式
- **合并**：`settings` 并入 `profile`（PWB 已验证此模式）
- **丢弃**：`not-found`（小程序用 app.json 兜底）

---

## 4. 数据域规划（14 域）

| # | 存储键 | 来源 store | 说明 |
|---|---|---|---|
| 1 | `db:daily` | `dailyDataStore` | 每日学习数据（打卡、时长） |
| 2 | `db:excerpts` | `excerptStore` | 摘抄收藏 |
| 3 | `db:five_year_plan` | `fiveYearPlanStore` | 五年计划、年度目标、里程碑 |
| 4 | `db:interview_bank` | `interviewBankStore` | 面试题库（viewed / bookmarks） |
| 5 | `db:interviews` | `interviewStore` | 面试追踪记录 |
| 6 | `db:knowledge` | `knowledgeStore` | 知识图谱节点与边 |
| 7 | `db:learning_paths` | `learningPathStore` | 学习路径 |
| 8 | `db:mock_interviews` | `mockInterviewStore` | 模拟面试记录 |
| 9 | `db:notifications` | `notificationStore` | 通知 |
| 10 | `db:study_projects` | `projectStore` | **学习项目**（改名，避免与 PWB 作品集混淆） |
| 11 | `db:quizzes` | `quizStore` | 题库与答题记录 |
| 12 | `db:reviews` | `reviewStore` | 复习调度（SM-2 参数） |
| 13 | `db:settings` | `settingsStore` | 主题、通知偏好 |
| 14 | `db:user` | `userStore` | 用户信息 |

> 详细字段结构待现状盘点完成后补入 `docs/superpowers/plans/2026-10-22-knowledge-hub-data-model.md`。

---

## 5. 新增能力（PWB 没有的）

### 5.1 SM-2 间隔重复算法

`reviewStore` 内含 SuperMemo-2 算法的 interval / easeFactor / repetitions 计算。
这是要**逐字保留**的真算法，抽为 `core/review.js` 纯函数模块，配单测。

### 5.2 Canvas 图表

原版 H5 用 SVG，小程序端标注「用 Canvas」。需新建 3 个图表组件：

| 组件 | 用途 | 实现要点 |
|---|---|---|
| `components/heatmap/` | 学习热力日历 | `<canvas type="2d">`，按日期网格上色 |
| `components/trend-chart/` | 学习趋势折线 | 2d context 画折线 + 坐标轴 |
| `components/radar-chart/` | 技术雷达 | 多边形网格 + 数据多边形 |

Canvas 2d 在小程序的注意事项：
- 必须 `wx.createSelectorQuery().select('#id').fields({ node: true, size: true })` 取 node
- 画布尺寸用 **px 而非 rpx**，需乘 `pixelRatio` 保证高清屏不模糊
- 节点在 `ready` 后才能取到，需处理时序

### 5.3 答题交互

quiz 页 29 按钮，含选项选择、即时判分、错题本。这是 KH 最复杂的单页交互。

---

## 6. 分期实施

沿用 PWB 的六段式，每段跑测试与静态校验：

| 阶段 | 内容 | 产出 |
|---|---|---|
| **P0** | 骨架：app 三件套、core 共享层、stores/seed、测试基建 | 可运行空壳 |
| **P1** | 首页仪表盘 + 学习 tab（quiz / review / deep / path）+ `core/review.js` | 5 页 |
| **P2** | 面试 tab（tracker / bank / mock） | 4 页 |
| **P3** | 知识 tab（graph / excerpt / radar）+ 3 个图表组件 | 5 页 |
| **P4** | 我的 + 五年计划 + 项目路线 | 3 页 |
| **P5** | 收尾：删旧工程、README、全量验收 | — |

---

## 7. 风险与待确认

| 项 | 说明 |
|---|---|
| Canvas 时序 | 小程序 canvas node 异步获取，图表组件需处理好首次渲染时机 |
| 知识图谱复杂度 | 原版节点关系图在小屏上的可读性存疑，可能降级为「节点列表 + 关系清单」 |
| 数据迁移 | 旧版 persist 键名与新版 `db:` 不同，同 PWB 一样不自动迁移 |
| 模拟面试 | 若含计时/语音等高交互，可能降级为「自测模式」 |
