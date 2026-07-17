# 提案：reading-radar 基础设施重构 + Linux 技术栈 + 学习路线图

## Why

reading-radar 目前有 4 个架构层面的技术债，直接影响后续所有功能开发：

1. **index.html 是孤立岛屿**：6 个页面之间缺乏统一导航，用户无法在页面间自由跳转，每次需要手动改 URL
2. **三份重复的数据源**：`radar-tech.js`、`quiz-tech.js`、`kanban-data.js` 各自定义同一批知识点（C/CPP/DS/DB/PY），只是字段不同——新增一个技术栈需要在 3 个地方重复定义 ID 和元数据
3. **两套看板渲染实现**：index.html 内嵌一套、learning-kanban.html 独立一套，逻辑重复，改一处漏一处
4. **quiz-system.html 是 2909 行的单体文件**：JS 全部内联，难以维护和扩展

以上问题不解决，每新增一个技术栈（如本次的 Linux）都会在 3 个文件中重复定义 56 个知识点，且在 2909 行的文件中新增空题库处理逻辑。

此外，现有五年计划页的"能力精进"打卡项过于笼统（"深度学习 2h+"），没有跟看板学习进度联动。

**本次变更分两个阶段解决：**

| 阶段 | 内容 | 解决的问题 |
|------|------|-----------|
| **Phase 0：基础设施重构** | P0 统一导航 → P1 统一数据源 → P2 看板渲染模块化 → P3 测验系统模块化（部分提取） | 4 个架构技术债 |
| **Phase 1-6：Linux + 路线图** | Linux 技术栈 56 知识点 + 月度学习路线图 + 五年计划联动 | 功能增量 |

Phase 0 完成后，Phase 1 添加 Linux 数据只需在 items-registry.js 中定义一次，3 个消费者自动生效。

---

## 使用者画像

- 数据库内核开发者，C 是日常语言，C++ 和 Python 也会用
- DB 知识点已大部分掌握（59 个，均 ≥10 题），需要在 **Linux 内核行为理解** 上系统补课
- 学习优先级：**DB（补深）> Linux（新学）> C（深化）> DS（补理论）> C++（工具）> Python（辅助）**

---

## 现有系统设计（基线）

```
project/reading-radar/
├── index.html              # 雷达图 + 看板嵌入（C/C++/DS/DB/PY 五个模式）
├── learning-kanban.html    # 独立看板页
├── quiz-system.html        # SPA 测验系统（2909 行单体）
├── dashboard.html          # 聚合仪表盘（含五年计划摘要）
├── five-year-plan.html     # 五年计划（每日打卡 + 日历）
├── learn.html              # 学习内容页
├── data/
│   ├── quiz-tech.js        # 元数据（C/C++/DS/DB/PY）—— 第 1 份重复
│   ├── quiz-static.js      # 静态配置（CATEGORIES/QUADRANT_LABELS/DAILY_TIPS）
│   ├── quiz-questions-*.js # 题库（c/cpp/ds/db/py，全部 ≥10 题，单文件 2000-4000 行）
│   ├── radar-tech.js       # 书籍雷达元数据 —— 第 2 份重复
│   ├── kanban-data.js      # 看板卡片 —— 第 3 份重复
│   └── learn-content-*.js  # 学习内容数据
└── user-data/              # localStorage 持久化
```

**关键约束**：纯静态站 / 无构建工具 / `<script>` 全局挂载 / localStorage 共享 / 不引入外部依赖。

**三层数据重复示意**（以 C 语言为例）：

```
quiz-tech.js:    C_TECH_DATA[40]    = [{id:"c-syntax", title, quadrant, ring, desc}]
radar-tech.js:   C_RADAR_DATA[40]   = [{id:"c-syntax", title, quadrant, ring, books}]
kanban-data.js:  C_KANBAN_DATA[40]  = [{id:"c-syntax", title, quadrant, ring, tags, status}]
                 ─────────────────────────────────────────────────
                 同一个 id 在三处各定义一遍，字段不同但 id/title/quadrant/ring 完全重复
```

---

## What Changes

### Phase 0：基础设施重构（前置阶段）

#### P0 — 统一导航栏

- **新增** `data/nav-component.js`：导航配置数据（页面列表、标签、图标）
- **修改** 全部 6 个 HTML 页面：在 `<header>` 中插入统一的导航栏 DOM + CSS
- 导航栏内容：Logo → [雷达图] [看板] [测验] [学习内容] [仪表盘] [五年计划]
- 当前页面高亮 + 移动端汉堡菜单

#### P1 — 统一数据源

- **新增** `data/items-registry.js`：所有技术栈知识点的**唯一定义处**
  - 包含公共字段：`id`、`title`、`stack`、`quadrant`、`ring`、`desc`
  - 现有 5 个技术栈的数据迁移到 registry（C 44 个 + CPP 42 个 + DS 52 个 + DB 59 个 + PY 43 个 = 240 个条目）
- **重构** `quiz-tech.js`：从 ITEMS_REGISTRY 生成 `C_TECH_DATA` 等（过滤 + 映射）
- **重构** `radar-tech.js`：从 ITEMS_REGISTRY 生成，追加 `books` 字段
- **重构** `kanban-data.js`：从 ITEMS_REGISTRY 生成，追加 `tags`/`status` 字段
- **向后兼容**：全局变量名不变（`C_TECH_DATA`、`C_RADAR_DATA`、`C_KANBAN_DATA` 等），现有页面无需改动

#### P2 — 看板渲染模块化

- **新增** `data/kanban-render.js`：共享看板工具库
  - `updateKanbanStatus(itemId, newStatus)` — 状态更新 + localStorage 同步
  - `getKanbanProgress(stackFilter)` — 进度计算
  - `renderKanbanBoard(containerId, items, options)` — 通用渲染函数（已定义，供后续统一使用）
- **发现**：两个看板数据模型不兼容——index.html 采用图书模型（author/rec/stars/drag-drop），learning-kanban.html 采用技能学习模型（ITEMS_REGISTRY/quiz badges/status columns），无法直接合并渲染函数
- **实际做法**：kanban-render.js 作为共享模块被两页面加载，各自保留内联实现。共享辅助函数（状态更新、进度计算）可直接复用，renderKanbanBoard() 留待数据模型统一后切换

#### P3 — 测验系统模块化（部分提取）

- **新增** `data/quiz-core.js`：测验核心逻辑（部分提取）
  - `QuizState` 状态管理对象（挂在 `window` 下，保持全局可访问）
  - `loadQuestionBank(categoryId)` — 题库加载与合并（支持按象限拆分文件）
  - 答题逻辑（选题、判分、进度、错题本）暂留 quiz-system.html 中（与计时器/服务器同步深度耦合）
- **新增** `data/quiz-ui.js`：测验 UI 渲染
  - `QuizUI.renderCategorySelector()` / `renderQuadrantSelector()` / `renderQuestion()` / `renderResult()`
  - `showEmptyBankNotice()` — 题库暂未开放提示
  - 事件绑定与动画过渡效果
- **实际结果**：quiz-system.html 从 2909 行变为 2922 行（+13 行，因 `nav-component` 脚本标签和加载标签的引入）。提取了约 500 行到 quiz-core.js + quiz-ui.js，但计时器、服务器同步（`syncQuizFromServer`）、项目管理、错题本等核心功能因与页面深度耦合尚未迁移
- 全局访问点保持兼容：`window.QuizState`、`window.QuizUI` 供调试
- **后续迭代**：继续将答题逻辑（selectQuestions/submitAnswer/calculateScore）和服务器同步迁移到 quiz-core.js，目标将 quiz-system.html 缩减到 ~500 行

#### P4 — 题库文件按象限拆分

- **新增策略**：每个技术栈的题库文件按 4 个象限拆分为 4 个文件
  - 命名规范：`quiz-questions-<stack>-<quadrant>.js`
  - 例如 Linux：`quiz-questions-linux-observability.js`、`quiz-questions-linux-os-internals.js`、`quiz-questions-linux-storage.js`、`quiz-questions-linux-networking.js`
- **现有题库**：保持现状，后续按需迁移（C/CPP/DS/DB/PY 各 2000-4000 行，暂不拆分，避免变更范围过大）
- **Linux 题库**：从 day 1 就按象限拆分为 4 个文件
  - 每个文件 ~14 个知识点，添加题目时 Claude 只需读取目标象限文件
  - 使用 `Object.assign(QUESTION_BANK.linux || {}, {...})` 安全合并

**题库拆分的 Token 效率分析**：

| 方案 | 单文件大小 | 读取 Token（估） | 新增 5 题消耗 |
|------|-----------|-----------------|-------------|
| 现状（单文件） | ~380KB (DB) | ~95K token | 每次都读整文件 |
| 按象限拆分 | ~95KB/文件 | ~24K token | 只读目标象限 |
| 节省 | — | **~75%** | 新增题目只需读 1/4 文件 |

### Phase 1-6：Linux 技术栈 + 学习路线图（原提案，受益于 Phase 0）

#### 新增

- **Linux 技术栈**（56 个知识点 × 4 象限）：面向数据库内核开发者的 Linux 知识体系
  - 象限1：资源观测与诊断（/proc、iostat、perf、eBPF、NUMA……）
  - 象限2：内核行为理解（Page Cache、Direct I/O、fsync 链路、io_uring、cgroup……）
  - 象限3：存储与文件系统（ext4/xfs、fio、fallocate、SPDK、NVMe……）
  - 象限4：网络与系统编程（TCP 状态机、epoll、零拷贝、DPDK、RDMA……）
  - **得益于 Phase 0 P1**：只需在 `items-registry.js` 中追加 56 个条目 → quiz-tech/radar-tech/kanban-data 自动生效
  - **得益于 Phase 0 P4**：题库按 4 象限拆分为 4 个文件，新增题目只读取目标文件
- **学习路线图数据** `data/learn-roadmap.js`：2026 年 12 个月的月度学习计划
  - 每个知识点标注：`difficulty`（easy/medium/hard）、`estDays`（预估天数）
  - 按优先级排列：DB 深化 → Linux 新学 → C 深化 → DS 理论 → C++ 现代特性 → Python 工具
- **自适应计划机制**：提前完成当月内容时，弹出确认对话框，支持 pull-forward 下月内容
- **五年计划页面中的月度学习清单**：在主题卡下方展示当月学习目标，每日打卡中增加"📚 今日学习"项

#### 修改

- `items-registry.js`：追加 56 个 Linux 知识点（唯一数据源，Phase 0 P1 已建立）
- `quiz-static.js`：追加 `QUADRANT_LABELS.linux`
- `five-year-plan.html`：新增学习路线图渲染逻辑 + 自适应计划机制 + 弹窗"今日学习"区
- `dashboard.html`：五年路线摘要更新，加入 Linux 技术栈卡片
- **得益于 Phase 0 P0**：所有页面已有统一导航栏，只需在 nav-component.js 中加 Linux 入口

### 新增文件清单

| 文件 | 用途 | 阶段 |
|------|------|------|
| `data/nav-component.js` | 统一导航栏配置 | Phase 0 P0 |
| `data/items-registry.js` | 所有技术栈知识点唯一定义处（240 条迁移 + 56 条 Linux = 296 条） | Phase 0 P1 |
| `data/kanban-render.js` | 共享看板渲染逻辑 | Phase 0 P2 |
| `data/quiz-core.js` | 测验核心逻辑（状态管理 + 题库加载） | Phase 0 P3 |
| `data/quiz-ui.js` | 测验 UI 渲染（DOM + 事件 + 动画） | Phase 0 P3 |
| `data/learn-roadmap.js` | 2026 年月度学习路线图数据 | Phase 2 |
| `data/quiz-questions-linux-observability.js` | Linux 象限1 题库空框架 | Phase 1 |
| `data/quiz-questions-linux-os-internals.js` | Linux 象限2 题库空框架 | Phase 1 |
| `data/quiz-questions-linux-storage.js` | Linux 象限3 题库空框架 | Phase 1 |
| `data/quiz-questions-linux-networking.js` | Linux 象限4 题库空框架 | Phase 1 |

---

## Capabilities

### New Capabilities（Phase 0 新增）

- `unified-navigation`：统一导航栏——所有页面共享一致的导航组件，支持当前页高亮和移动端适配
- `data-item-registry`：统一数据注册中心——所有知识点在 items-registry.js 中唯一定义，quiz-tech/radar-tech/kanban-data 自动派生
- `kanban-render-merge`：看板渲染模块化——创建 kanban-render.js 共享工具库，两页面因数据模型不兼容各自保留内联实现，共享状态更新与进度计算函数
- `quiz-system-split`：测验系统模块化（部分提取）——从 quiz-system.html 抽出 QuizState 状态管理和 QuizUI 渲染对象到独立文件，答题逻辑/计时器/服务器同步留待后续迭代
- `quiz-file-by-quadrant`：题库文件按象限拆分——每个技术栈的题库按 4 象限拆分文件，减少单文件体积和 Token 消耗

### New Capabilities（Phase 1-6 原提案）

- `linux-tech-stack`：Linux 技术栈——56 个面向数据库内核开发者的知识点，含元数据/看板卡片/题库框架
- `adaptive-learning-roadmap`：自适应学习路线图——难度分级的月度计划 + 提前完成后的 pull-forward 机制
- `five-year-learning-layer`：五年计划中的月度学习层——日历叠加学习标记 + 弹窗"今日学习"选择区

### Modified Capabilities

- `five-year-plan-integration`：扩展仪表盘中的五年计划摘要，加入当月学习进度显示
- `quiz-system-navigation`：各页面通过统一导航栏自动获得 Linux 入口（不单独修改每个页面）

---

## Impact

- **新增文件**：10 个（Phase 0: 5 个 + Phase 1-2: 5 个）
- **修改文件**：
  - Phase 0：6 个 HTML + 3 个数据文件（quiz-tech.js、radar-tech.js、kanban-data.js）+ quiz-static.js
  - Phase 1-6：items-registry.js、five-year-plan.html、dashboard.html、nav-component.js
- **向后兼容**：
  - 全局变量名不变（C_TECH_DATA、C_RADAR_DATA、C_KANBAN_DATA 等）
  - localStorage 键名不变
  - 现有用户数据不受影响
- **破坏性变更**：无
- **风险点**：
  - Phase 0 涉及 6 个 HTML 页面和 3 个核心数据文件，需要逐个页面验证
  - items-registry.js 的数据迁移需要确保 296 个条目的字段完整性
  - quiz-system 拆分后需要确保全局状态共享不中断
- **无外部依赖**：纯静态站，不引入新的第三方库
