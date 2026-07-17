# 设计文档：reading-radar 基础设施重构 + Linux 技术栈 + 学习路线图

## Context

reading-radar 已覆盖 C/C++/DS/DB/PY 五个技术栈。本次变更分两个阶段：
- **Phase 0**：解决 4 个架构技术债（孤立导航、三重数据重复、双看板实现、测验单体）
- **Phase 1-6**：新增 Linux 技术栈 + 月度学习路线图 + 五年计划联动

所有改动在纯静态 HTML/JS 约束下进行，不引入构建工具和外部依赖。

## Goals / Non-Goals

**Goals:**
- Phase 0：统一导航、统一数据源、看板渲染模块化、测验系统模块化（部分提取）、题库按象限拆分
- Phase 1-6：新增 Linux 技术栈 56 个知识点、12 个月学习路线图、自适应计划机制

**Non-Goals:**
- 不编写 Linux 题库内容（仅建空框架）
- 不创建 Linux 学习内容页（learn-content-linux.js）
- 不修改 learn.html
- 不引入第三方依赖
- 不修改 localStorage 键名约定
- 现有题库（C/CPP/DS/DB/PY）暂不拆分文件（避免变更范围过大）

---

## Phase 0 设计决策

### D0.1：统一导航栏组件设计

**决策**：所有 6 个页面共享同一套导航栏 HTML + CSS，通过 `<script>` 标签加载配置。

**导航栏配置**（`data/nav-component.js`）：

```js
const NAV_CONFIG = {
  brand: { label: "Reading Radar", icon: "🎯" },
  items: [
    { id: "radar",     label: "雷达图",  href: "index.html",         icon: "📡" },
    { id: "kanban",    label: "看板",    href: "learning-kanban.html", icon: "📋" },
    { id: "quiz",      label: "测验",    href: "quiz-system.html",   icon: "📝" },
    { id: "learn",     label: "学习",    href: "learn.html",         icon: "📖" },
    { id: "dashboard", label: "仪表盘",  href: "dashboard.html",     icon: "📊" },
    { id: "plan",      label: "五年计划", href: "five-year-plan.html", icon: "🗓️" },
  ]
};
```

**渲染方式**：每个 HTML 页面在 `<body>` 顶部放置 `<nav id="global-nav"></nav>`，由 `nav-component.js` 中的 `initGlobalNav(currentPageId)` 函数动态渲染。当前页面对应的导航项添加 `.active` 样式。

**移动端适配**：宽度 < 768px 时，导航项折叠为汉堡菜单（☰），点击展开下拉。

**理由**：集中管理，新增页面只需在 NAV_CONFIG 中加一项。6 个文件共享同一份 HTML/CSS，避免不一致。

### D0.2：items-registry.js 统一数据源设计

**决策**：所有技术栈的知识点元数据在 `items-registry.js` 中唯一定义，quiz-tech.js、radar-tech.js、kanban-data.js 从 registry 派生。

**Registry 数据结构**：

```js
const ITEMS_REGISTRY = {
  // C 语言 — 44 个知识点
  "c-syntax":        { stack:"c", quadrant:"language",    ring:"basic",        title:"数据类型与运算符",     desc:"..." },
  "c-control-flow":  { stack:"c", quadrant:"language",    ring:"basic",        title:"控制流",              desc:"..." },
  // ... 共 240 个条目（C:44 + CPP:42 + DS:52 + DB:59 + PY:43），后续追加 Linux:56 → 296

  // Linux 技术栈 — 56 个知识点（Phase 1 追加）
  "linux-proc-fs":   { stack:"linux", quadrant:"observability", ring:"basic",   title:"/proc 文件系统全景",  desc:"..." },
  // ...
};
```

**消费者适配层**（在各数据文件的末尾）：

```js
// quiz-tech.js — 从 registry 过滤 + 映射为 TECH_DATA 格式
const C_TECH_DATA = Object.entries(ITEMS_REGISTRY)
  .filter(([id, item]) => item.stack === "c")
  .map(([id, item]) => ({ id, title: item.title, quadrant: item.quadrant, ring: item.ring, desc: item.desc }));

// radar-tech.js — 追加 books 字段
const C_RADAR_DATA = Object.entries(ITEMS_REGISTRY)
  .filter(([id, item]) => item.stack === "c")
  .map(([id, item]) => ({ id, title: item.title, quadrant: item.quadrant, ring: item.ring, books: item.books || [] }));

// kanban-data.js — 追加 tags/status 字段
const C_KANBAN_DATA = Object.entries(ITEMS_REGISTRY)
  .filter(([id, item]) => item.stack === "c")
  .map(([id, item]) => ({ id, title: item.title, quadrant: item.quadrant, ring: item.ring, tags: item.tags || [], status: "todo" }));
```

**向后兼容**：
- 全局变量名不变（`C_TECH_DATA`、`C_RADAR_DATA`、`C_KANBAN_DATA` 等）
- 现有页面不改动——它们读取的变量名和结构都不变
- 仅数据来源从"各自手写"变为"从 registry 派生"

**迁移策略**：
1. 先将现有 3 个文件中的 240 个条目逐一手工迁移到 `items-registry.js`
2. 将 3 个消费者文件改为从 registry 派生
3. 浏览器控制台验证 `C_TECH_DATA.length === 44` 等不变性检查
4. 此后新增技术栈只需在 registry 中追加条目，3 个消费者自动生效

**理由**：解决三层数据重复的根本原因。Phase 1 添加 Linux 的 56 个知识点只需在 registry 中定义一次，而非在 3 个文件中各写一遍。

### D0.3：看板渲染模块化

**决策**：创建 `data/kanban-render.js` 作为共享看板工具库，供两个页面加载使用。

**实际共享 API**：

```js
// 更新单个条目的状态 → DOM 更新 + localStorage 持久化
function updateKanbanStatus(itemId, newStatus) {
  // newStatus: "todo" | "learning" | "done" | "review"
}

// 获取进度统计
function getKanbanProgress(stackFilter = null) {
  // → { total, done, learning, todo, review, percent }
}

// 通用看板渲染函数（已定义，供后续统一使用）
function renderKanbanBoard(containerId, items, options = {}) {
  // options: { groupBy: "quadrant"|"ring", showProgress: true, showFilters: true }
}
```

**实际发现**：两个看板的数据模型不兼容，无法直接合并渲染函数。

| 页面 | 数据模型 | 字段 | 交互 |
|------|---------|------|------|
| index.html（图书看板） | book model | author, rec, stars | 拖拽排序，图书推荐 |
| learning-kanban.html（技能学习） | ITEMS_REGISTRY model | quiz badges, links, status | 状态列切换，测验徽章 |

**实际做法**：两个页面均加载 `kanban-render.js` 作为共享模块，但各自保留内联渲染实现。共享库提供了 `updateKanbanStatus()` 和 `getKanbanProgress()` 等可复用的辅助函数。`renderKanbanBoard()` 已作为标准 API 定义——当未来数据模型统一（例如统一为 ITEMS_REGISTRY 格式）后，可直接切换到此共享函数。

**理由**：强行合并两个数据模型不同的看板会产生大量 if/else 分支，得不偿失。保留各自内联实现、共享辅助函数是性价比最高的方案。后续如果在 index.html 中也引入 ITEMS_REGISTRY 数据源，则可以平滑迁移到 renderKanbanBoard。

### D0.4：测验系统模块化（部分提取）

**决策**：从 quiz-system.html（2909 行单体）中提取可独立模块化的部分到 quiz-core.js 和 quiz-ui.js。

**提取边界**：

```
quiz-system.html      # HTML 结构 + <script> 加载标签 + 内联核心逻辑（2922 行）
data/quiz-core.js     # 测验核心逻辑（状态管理对象 + 题库加载合并）
data/quiz-ui.js       # 测验 UI 渲染对象
```

**实际已提取内容**：

已提取到 quiz-core.js：
```js
// 状态管理对象（挂在 window 下，保持全局可访问）
window.QuizState = {
  currentCategory: null,
  currentQuadrant: null,
  currentRing: null,
  questions: [],
  currentIndex: 0,
  answers: {},
  score: 0,
  mode: "practice",
};

// 题库加载与合并（支持按象限拆分文件）
function loadQuestionBank(categoryId) {
  if (!QUESTION_BANK[categoryId] || Object.keys(QUESTION_BANK[categoryId]).length === 0) {
    return { empty: true, message: "题库暂未开放" };
  }
  return { empty: false, questions: QUESTION_BANK[categoryId] };
}
```

已提取到 quiz-ui.js：
```js
window.QuizUI = {
  renderCategorySelector(containerId) { ... },
  renderQuadrantSelector(containerId) { ... },
  renderQuestion(containerId, question) { ... },
  renderResult(containerId, score, wrongQuestions) { ... },
  showEmptyBankNotice(containerId, categoryLabel) { ... },
  bindEvents() { ... },
};
```

**留在 quiz-system.html 中的核心功能**（因与页面深度耦合，尚未迁移）：
- 计时器（testTimerId / elapsedSeconds）与倒计时渲染
- 服务器同步（`syncQuizFromServer()` / `_scheduleServerSync()`）
- 答题判分逻辑（selectQuestions / submitAnswer / calculateScore / getWrongQuestions）
- 项目追踪模块（projects）
- 错题本渲染与复习模式
- 考试/练习模式切换逻辑

**关键约束**：
- 状态挂在 `window` 下，确保 inline script 和外部文件都能访问
- 文件加载顺序：`quiz-core.js` → `quiz-ui.js` → `quiz-system.html` 中的内联初始化
- 现有页面行为不变——所有功能、事件、动画保持一致

**理由**：2909 行的单体文件无法一次性拆分完成。本次提取了状态管理壳和 UI 渲染对象（~500 行），降低了 QuizState 和 UI 操作的耦合度。后续迭代继续将答题逻辑和服务器同步迁移到 quiz-core.js，最终目标将 quiz-system.html 缩减到 ~500 行。当前每次读取 quiz-system.html 仍需 ~730K token，但修改 UI 已可以只读 quiz-ui.js（340 行），修改状态管理只需读 quiz-core.js（155 行），日常编辑效率已有提升。

### D0.5：题库文件按象限拆分策略

**决策**：每个技术栈的题库按 4 个象限拆分为 4 个独立文件。Linux 从 day 1 就采用此模式；现有题库（C/CPP/DS/DB/PY）保持单文件，后续按需迁移。

**文件命名规范**：`quiz-questions-<stack>-<quadrant>.js`

**Linux 题库文件结构**（4 个文件，每文件 ~14 个知识点）：

```
quiz-questions-linux-observability.js   → 象限1: 资源观测与诊断 (16 题)
quiz-questions-linux-os-internals.js    → 象限2: 内核行为理解 (16 题)
quiz-questions-linux-storage.js         → 象限3: 存储与文件系统 (12 题)
quiz-questions-linux-networking.js      → 象限4: 网络与系统编程 (12 题)
```

**每个文件内部格式**（使用 Object.assign 安全合并）：

```js
// quiz-questions-linux-observability.js
"use strict";
QUESTION_BANK.linux = Object.assign(QUESTION_BANK.linux || {}, {
  "linux-proc-fs": [
    { id:"linux-proc-fs-q1", type:"choice", difficulty:"basic",
      scenario:"...", stem:"...", options:[...], answer:"...", explanation:"..." },
    // ... 每个知识点 ≥10 题
  ],
  "linux-cpu-diag": [ ... ],
  // ... 本象限 ~14 个知识点
});
```

**quiz-core.js 中的加载逻辑**：

```js
function loadQuestionBank(categoryId) {
  // QUESTION_BANK[categoryId] 已由多个 <script> 标签通过 Object.assign 合并
  // 只需检查是否存在
  if (!QUESTION_BANK[categoryId] || Object.keys(QUESTION_BANK[categoryId]).length === 0) {
    return { empty: true, message: "题库暂未开放" };
  }
  return { empty: false, questions: QUESTION_BANK[categoryId] };
}
```

**Token 效率分析**：

| 场景 | 单文件方案 | 象限拆分方案 | 节省 |
|------|-----------|-------------|------|
| 新增 5 题到 observability 象限 | 读取整文件 ~95K token | 只读 observability 文件 ~24K token | **75%** |
| 修改 3 题跨 3 个象限 | 读取整文件 ~95K token | 读 3 个象限文件 ~72K token | 24% |
| 批量新增 20 题到全部象限 | 读取整文件 ~95K token | 读 4 个文件 ~95K token | 持平 |

**常见场景（新增 5-10 题到 1 个象限）节省显著**，这是日常使用中最频繁的操作。

**理由**：题库文件是日常编辑频率最高的文件。按象限拆分后，Claude 新增题目时只需读取目标象限文件（~1/4 大小），显著降低 Token 消耗。现有题库暂不拆分是为了控制本次变更范围。

---

## Phase 1-6 设计决策

### D1：Linux 知识点设计——面向数据库内核开发者

**决策**：56 个知识点，4 个象限，每个知识点描述中明确其与数据库的关联。

**象限划分**：

```
象限1: 资源观测与诊断 (observability) — 16 个
  basic:     /proc 文件系统 | CPU 使用率诊断 | 内存水位分析 | 磁盘 I/O 观测
  interm:    perf 基础与火焰图 | 块设备 I/O 追踪 | Page Cache 命中率分析 | 网络流量观测
  advanced:  eBPF 可观测性入门 | 数据库负载火焰图 | NUMA 感知与内存分布 | 延迟异常值分析

象限2: 内核行为理解 (os-internals) — 16 个
  basic:     系统调用全流程 | Page Cache 与 Buffer Pool | 进程调度 CFS | 虚拟内存与页表
  interm:    Direct I/O vs Buffered I/O | Huge Pages 与数据库 | fsync 语义深度解析 | I/O 调度器
  advanced:  io_uring 深度解析 | mmap 与数据库引擎 | cgroup v2 资源隔离 | 内核 RCU 与无锁读

象限3: 存储与文件系统 (storage) — 12 个
  basic:     ext4/xfs 基础 | 磁盘与 SSD 特性 | Linux 通用块层 | LVM 与设备映射器
  interm:    fallocate 与预分配 | fdatasync 与写屏障 | 文件系统日志 | fio 基准测试
  advanced:  CoW 文件系统与数据库 | SPDK/NVMe 用户态驱动 | 异步 I/O 对比 | 存储栈端到端延迟

象限4: 网络与系统编程 (networking) — 12 个
  basic:     TCP 连接状态机 | Socket 缓冲区调优 | DB 协议抓包分析 | 连接池与短连接开销
  interm:    epoll 原理与事件驱动 | 零拷贝技术栈 | CPU 亲和性与 IRQ 绑定 | Unix Domain Socket
  advanced:  TCP 拥塞控制与长肥管道 | 内核旁路网络 | RDMA 概述 | 用户态协议栈
```

**Registry 中的数据格式**（追加到 items-registry.js）：

```js
"linux-proc-fs": { stack:"linux", quadrant:"observability", ring:"basic",
  title:"/proc 文件系统全景",
  desc:"读懂 /proc/meminfo、/proc/cpuinfo、/proc/<pid>/fd 等。理解 DB 进程的内存/IO/fd 资源如何观测。",
  tags: ["proc", "观测", "诊断"] },
```

**理由**：得益于 Phase 0 P1（统一数据源），Linux 的 56 个知识点只需在 items-registry.js 中定义一次。quiz-tech.js、radar-tech.js、kanban-data.js 自动通过过滤 `stack === "linux"` 生成各自的视图。题库按 4 象限拆分为 4 个文件（Phase 0 P4 策略）。

### D2：自适应学习计划机制

**决策**：月度学习计划硬编码在 `learn-roadmap.js` 中，每月检查是否全部完成并弹出确认。

**数据格式**：

```js
const LEARNING_ROADMAP = {
  "2026": {
    theme: "筑基年 — 打好基础",
    months: {
      1: {
        label: "Linux 内核基础 + DB 存储回顾",
        items: [
          { id: "linux-proc-fs",      stack: "linux", difficulty: "easy",   estDays: 2 },
          { id: "linux-cpu-diag",     stack: "linux", difficulty: "easy",   estDays: 2 },
          { id: "db-storage-overview",stack: "db",    difficulty: "easy",   estDays: 1 },
        ]
      },
    }
  }
};
```

**自适应流程**：

```
renderCurrentMonthPlan()
      ↓
检测：当月所有 item 的 kanban 状态是否都是 "done"
      ↓ 是
renderCompletionDialog()
      ├── "🎉 本月计划提前完成！"
      ├── [提前开始下月内容] → pullForwardNextMonth()
      ├── [复习本月薄弱点]   → generateWeakPointQuiz()
      └── [不再提醒]         → dismiss()
      ↓
pullForwardNextMonth()
      ├── 将下月所有 item 的 plannedMonth 改为当前月
      ├── 后续月份依次前移
      └── 年末缓冲月（12月）收缩
```

**关键约束**：
- 状态检查复用 localStorage 中已有的 kanban 数据（`learning-kanban-<stack>` key）
- pull-forward 产生的状态变更持久化到 localStorage（`learning-roadmap-state` key），下次打开页面时优先读取
- 不修改 `learn-roadmap.js` 文件本身

**理由**：自适应不需要 AI，规则明确。复杂度集中在 UI 层的确认对话框和 localStorage 状态管理。

### D3：五年计划页面集成方式

**决策**：在 five-year-plan.html 中新增"月度学习层"，不替换现有打卡系统。

**布局变更**：在 theme-card 之后、stats-row 之前插入 `monthly-learning` 区块。

**弹窗变更**：在现有的五大工程勾选框下方，新增"📚 今日学习"区域。从当月学习清单中读取知识点，每个显示为 toggle chip。选中的知识点自动联动看板状态。

**日历格子变更**：每个 day-cell 的 dot-row 中增加一个"学习标记点"（蓝色），区别于五大工程点。

**理由**：保持现有打卡系统完整，学习层作为增量叠加。用户可以选择只用打卡、只用学习路线图、或两者都用。

### D4：页面导航更新策略

**决策**：得益于 Phase 0 P0（统一导航栏），Linux 技术栈的导航入口只需在 nav-component.js 中追加。quiz-system.html 和 learning-kanban.html 的技术栈选择按钮（CATEGORIES 遍历生成）自动适配。

**各页面具体变更**：

| 页面 | 变更 |
|------|------|
| nav-component.js | 无需改动（技术栈选择在页面内部，导航栏只负责页面间跳转） |
| index.html | header mode-bar 增加 Linux 模式按钮（页面内部逻辑） |
| dashboard.html | stacks-grid 增加 Linux 进度卡片 |
| quiz-system.html | CATEGORIES 增加 linux，自动渲染导航按钮 |
| learning-kanban.html | kanban 数据增加 LINUX_DATA，自动渲染 |

### D5：技术栈优先级映射到月度计划

**决策**：2026 年 12 个月按以下逻辑编排：

```
优先级     技术栈        学时占比    2026年覆盖范围
─────────────────────────────────────────────
P0        DB 深化        25%        事务引擎/查询引擎/分布式共识/高可用
P1        Linux 新学      35%        56 个知识点全部分配到各月
P2        C 深化          15%        advanced ring 回顾
P3        DS 补理论       15%        图论/DP/高级数据结构
P4        C++ 现代特性     5%         C++17/20 核心新特性
P5        Python 工具      5%         数据处理生态补缺
```

**月度编排**：

```
1-2月:   Linux 资源观测 + DB 存储引擎回顾
3-4月:   Linux 内核行为 + DB 索引系统深入
5-6月:   Linux 存储 + DB 事务引擎强化
7-8月:   Linux 网络 + DB 查询引擎与优化器
9-10月:  Linux 高级主题 + DS 图论/DP 理论
11月:    C++ 现代特性 + C 高级系统编程回顾
12月:    弹性缓冲月 + Python 工具链补缺
```

**节奏**：每月 7-10 个知识点。easy 每周 3-4 个，medium 每周 2 个，hard 每周 1 个。

---

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|---------|
| Phase 0 涉及 6 个 HTML + 3 个数据文件，回归测试工作量大 | 每个子阶段完成后逐页验证，Phase 0 各子阶段独立可回滚 |
| items-registry.js 迁移 240 个条目的字段完整性 | 控制台自动校验：`C_TECH_DATA.length` 等不变性检查 |
| quiz-system 拆分后全局状态访问可能断裂 | 状态挂在 `window` 下，拆分前后访问路径一致 |
| 题库按象限拆分后 quiz-system 加载逻辑需适配 | quiz-core.js 中统一处理合并逻辑，与单文件模式兼容 |
| Linux 56 个知识点元数据一次性手写工作量 | 分批追加，先用占位 desc，后续迭代完善 |
| 学习路线图的月度分配可能与实际学习速度不匹配 | 自适应机制允许 pull-forward；难度标记让用户自行判断 |
| 五年计划页面 JS 逻辑变复杂 | 新增代码控制在独立函数中，不侵入现有 renderAll |
| Linux 题库为空，quiz-system 访问可能报错 | quiz-core.js 处理 QUESTION_BANK[catId] 为空的情况 |
| learn-roadmap.js 引用 items-registry.js 的 ID | ID 格式一致（`<stack>-<name>`），HTML 中按依赖顺序加载 |

---

## 加载顺序约定

Phase 0 完成后，各 HTML 页面的 `<script>` 加载顺序应为：

```html
<!-- 1. 数据层 -->
<script src="data/items-registry.js"></script>      <!-- 唯一定义所有知识点 -->
<script src="data/quiz-static.js"></script>          <!-- 静态配置 -->
<script src="data/quiz-tech.js"></script>            <!-- 从 registry 派生 TECH_DATA -->
<script src="data/radar-tech.js"></script>           <!-- 从 registry 派生 RADAR_DATA -->
<script src="data/kanban-data.js"></script>          <!-- 从 registry 派生 KANBAN_DATA -->

<!-- 2. 题库（按象限拆分，可选择性加载） -->
<script src="data/quiz-questions-c.js"></script>     <!-- 或按象限拆分后的文件 -->
<!-- ... -->

<!-- 3. 共享组件 -->
<script src="data/nav-component.js"></script>        <!-- 统一导航栏 -->
<script src="data/kanban-render.js"></script>        <!-- 看板渲染 -->
<script src="data/quiz-core.js"></script>            <!-- 测验逻辑 -->
<script src="data/quiz-ui.js"></script>              <!-- 测验 UI -->

<!-- 4. 页面内联初始化 -->
<script>
  initGlobalNav("radar");  // 当前页面 ID
  // ... 页面特定逻辑
</script>
```

---

## Open Questions

1. **pull-forward 后，后续月份的学习内容是否允许被跳过？** 建议不允许——前移只是把整个月的学习块整体提前，不减内容。如果连续提前完成，年初内容会在暑假前学完，年底成为"项目实战期"。

2. **"本月提前完成"的判定条件是什么？** 建议：当月所有 item 的 kanban 状态为 "done"（而非 "review"）。review 意味着还需要回头巩固，不算真正完成。

3. **学习路线图是否应该支持"落后"时的提醒？** 暂不实现。第一版只做提前完成的 pull-forward，进度落后的处理留到后续迭代。

4. **Linux 题库什么时候补？** 作为独立后续变更。建议在用户使用 Linux 看板至少 2 个月后，根据实际学习反馈决定先补哪些知识点的题。

5. **现有题库（C/CPP/DS/DB/PY）什么时候按象限拆分？** 作为独立后续变更。本次只在新创建的 Linux 题库中采用象限拆分。现有题库拆分时机：当某个题库文件超过 3000 行时触发拆分。
