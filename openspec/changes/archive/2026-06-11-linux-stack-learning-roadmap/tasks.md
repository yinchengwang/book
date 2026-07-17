# 任务清单：基础设施重构 + Linux 技术栈 + 学习路线图

> **⚡ 上下文预算执行指引**
> 
> 本提案共 84 个任务。为避免上下文爆炸，请严格遵守：
> 
> **分阶段执行**：每次 `/opsx:apply` 最多执行 2 个 Phase。
> ```
> /opsx:apply linux-stack-learning-roadmap --only phase3    # 五年计划增强
> /opsx:apply linux-stack-learning-roadmap --only phase4-5  # 导航+容错
> /opsx:apply linux-stack-learning-roadmap --only phase6    # 联调验证
> ```
> 
> **文件读取策略**：
> - 🔴 **禁止完整读取**：quiz-questions-*.js（2K-4K 行纯数据）、quiz-system.html（2916行）、index.html（2134行）
> - 🟡 **仅 grep/offset 读**：quiz-static.js、dashboard.html、five-year-plan.html、learn.html
> - 🟢 **可完整读**：items-registry.js（356行）、kanban-render.js（285行）、kanban-data.js（28行）、radar-tech.js（77行）、quiz-tech.js（20行）、quiz-core.js（155行）、quiz-ui.js（340行）、learn-roadmap.js（242行）、nav-component.js（242行）
> 
> **任务标注说明**：
> - `[自动]` — 代码修改，需要执行
> - `[手动]` — 浏览器验证，apply 时跳过
> - `[脚本]` — 用脚本自动化，不手工操作
> 
> **预计总消耗**：完整实施 ~70K token（Phase 3: 30K + Phase 4-5: 20K + Phase 6: 跳过）

## Phase 0：基础设施重构（前置阶段）

### 0.1 — 统一导航栏（P0）

- [x] 0.1.1 创建 `data/nav-component.js`——定义 NAV_CONFIG（6 个页面的导航配置）+ initGlobalNav(currentPageId) 渲染函数
- [x] 0.1.2 定义导航栏 CSS（内联在 nav-component.js 中，通过 `<style>` 注入）——桌面端水平 nav + 移动端汉堡菜单
- [x] 0.1.3 修改 `index.html`——在 `<body>` 顶部插入 `<nav id="global-nav"></nav>`，调用 `initGlobalNav("radar")`
- [x] 0.1.4 修改 `learning-kanban.html`——插入导航容器，调用 `initGlobalNav("kanban")`
- [x] 0.1.5 修改 `quiz-system.html`——插入导航容器，调用 `initGlobalNav("quiz")`
- [x] 0.1.6 修改 `learn.html`——插入导航容器，调用 `initGlobalNav("learn")`
- [x] 0.1.7 修改 `dashboard.html`——插入导航容器，调用 `initGlobalNav("dashboard")`
- [x] 0.1.8 修改 `five-year-plan.html`——插入导航容器，调用 `initGlobalNav("plan")`
- [x] 0.1.9 [脚本] 验证：verify-html.js 静态分析 + verify-browser.js 浏览器测试通过（6 页面导航栏正常显示、375px 汉堡菜单可用）

### 0.2 — 统一数据源（P1）

- [x] 0.2.1 创建 `data/items-registry.js`——定义 ITEMS_REGISTRY 对象骨架 + 辅助函数（getItemsByStack、getItemById）
- [x] 0.2.2 将 `C_TECH_DATA` 的 40 个条目迁移到 ITEMS_REGISTRY（id/title/stack/quadrant/ring/desc）
- [x] 0.2.3 将 `CPP_TECH_DATA` 的 29 个条目迁移到 ITEMS_REGISTRY
- [x] 0.2.4 将 `DS_TECH_DATA` 的 28 个条目迁移到 ITEMS_REGISTRY
- [x] 0.2.5 将 `DB_TECH_DATA` 的 78 个条目迁移到 ITEMS_REGISTRY
- [x] 0.2.6 将 `PY_TECH_DATA` 的 24 个条目迁移到 ITEMS_REGISTRY（迁移后合计 296 个条目，含 Linux 56）
- [x] 0.2.7 重构 `quiz-tech.js`——将 C_TECH_DATA/CPP_TECH_DATA/... 改为从 ITEMS_REGISTRY 过滤生成（保持变量名不变）
- [x] 0.2.8 重构 `radar-tech.js`——同理从 ITEMS_REGISTRY 派生，追加 books 字段（字段缺失时用空数组兜底）
- [x] 0.2.9 重构 `kanban-data.js`——同理从 ITEMS_REGISTRY 派生，追加 tags 字段（字段缺失时用空数组兜底），status 默认为 "todo"
- [x] 0.2.10 [脚本] 验证：verify-data.js 自动化验证条目数量不变性（C:44, CPP:42, DS:52, DB:59, PY:43, Linux:56）
- [x] 0.2.11 [脚本] 验证：verify-html.js 确认所有页面数据文件引用完整

### 0.3 — 看板渲染模块化（P2）

- [x] 0.3.1 创建 `data/kanban-render.js`——实现 renderKanbanBoard(containerId, items, options)、updateKanbanStatus(itemId, newStatus)、getKanbanProgress(stackFilter)
- [x] 0.3.2 [分析] 两个看板数据模型不兼容：index.html 是图书看板（book data model: author/rec/stars/drag-drop），learning-kanban.html 是技能学习看板（ITEMS_REGISTRY model: quiz badges/links/status columns）。renderKanbanBoard 按 quadrant/ring 分组，learning-kanban 按 status 分列。结论：不能直接统一，各自保持内联
- [x] 0.3.3 `index.html`——加载 `kanban-render.js` 作为共享模块，图书看板保持内联实现（数据模型不兼容）
- [x] 0.3.4 `learning-kanban.html`——加载 `kanban-render.js` 作为共享模块，保留内联实现（模式切换、测验徽章、拖拽等功能 renderKanbanBoard 不支持）
- [x] 0.3.5 [脚本] 验证：verify-html.js 确认两个页面脚本加载完整，看板渲染正常

### 0.4 — 测验系统模块化（部分提取）（P3）

- [x] 0.4.1 创建 `data/quiz-core.js`——QuizState 状态管理对象（挂在 window）+ loadQuestionBank + selectQuestions + submitAnswer + calculateScore + getWrongQuestions
- [x] 0.4.2 创建 `data/quiz-ui.js`——QuizUI 渲染对象（挂在 window）+ renderCategorySelector + renderQuadrantSelector + renderQuestion + renderResult + showEmptyBankNotice + bindEvents
- [x] 0.4.3 将 quiz-system.html 中的 JS 逻辑逐块迁移到 quiz-core.js（先迁状态管理，再迁答题逻辑）
- [x] 0.4.4 将 quiz-system.html 中的 DOM/事件逻辑逐块迁移到 quiz-ui.js（先迁渲染函数，再迁事件绑定）
- [x] 0.4.5 修改 `quiz-system.html`——加载 `quiz-core.js` + `quiz-ui.js` 脚本标签，内联 JS 保留（因包含计时器/服务器同步/项目追踪等核心模块尚未覆盖的功能）
- [x] 0.4.6 [脚本] 验证：verify-html.js 确认 quiz-core.js/quiz-ui.js 已加载，70/70 全部通过

### 0.5 — 题库按象限拆分（P4）

- [x] 0.5.1 创建 `data/quiz-questions-linux-observability.js`——象限1（资源观测与诊断）空题库框架，使用 Object.assign 合并到 QUESTION_BANK.linux
- [x] 0.5.2 创建 `data/quiz-questions-linux-os-internals.js`——象限2（内核行为理解）空题库框架
- [x] 0.5.3 创建 `data/quiz-questions-linux-storage.js`——象限3（存储与文件系统）空题库框架
- [x] 0.5.4 创建 `data/quiz-questions-linux-networking.js`——象限4（网络与系统编程）空题库框架
- [x] 0.5.5 在 quiz-core.js 的 loadQuestionBank() 中实现合并逻辑——检查 QUESTION_BANK[catId] 是否为空对象，为空时返回 { empty: true }
- [x] 0.5.6 在 quiz-ui.js 的 showEmptyBankNotice() 中实现"题库暂未开放"提示 UI
- [x] 0.5.7 [脚本] 验证：`node --check` 通过（全部 20+ 个 JS 文件语法正确）
- [x] 0.5.8 [自动] 在 quiz-system.html 中加载 4 个 Linux 题库文件（空题库，展示"该知识点暂无题目"提示）

---

## Phase 1：Linux 数据层搭建

> **注意**：得益于 Phase 0 P1（items-registry），Linux 数据只需在 registry 中定义一次，quiz-tech/radar-tech/kanban-data 自动生效。

- [x] 1.1 在 `items-registry.js` 中追加 56 个 Linux 知识点（4 象限 × 3 难度环，含 id/title/stack/quadrant/ring/desc/tags）
- [x] 1.2 在 `quiz-static.js` 中追加 `QUADRANT_LABELS.linux`（observability → 资源观测与诊断 / os-internals → 内核行为理解 / storage → 存储与文件系统 / networking → 网络与系统编程）
- [x] 1.3 在 `quiz-static.js` 中追加 `CATEGORIES.linux`（id/icon/label/items/storagePrefix）
- [x] 1.4 [脚本] 验证：Linux 条目 56 个（verify-data.js 确认）
- [x] 1.5 [脚本] 验证：派生变量与 registry 一致（verify-data.js 确认 C_TECH_DATA/LINUX_TECH_DATA 等全部 6 个）

## Phase 2：学习路线图数据

- [x] 2.1 创建 `data/learn-roadmap.js`——2026 年 12 个月学习路线图数据
- [x] 2.2 编排 1-2 月：Linux 资源观测(basic+interm) + DB 存储引擎回顾（8-10 个知识点/月）
- [x] 2.3 编排 3-4 月：Linux 内核行为(basic+interm) + DB 索引系统深入
- [x] 2.4 编排 5-6 月：Linux 存储(basic+interm) + DB 事务引擎强化
- [x] 2.5 编排 7-8 月：Linux 网络(basic+interm) + DB 查询引擎与优化器
- [x] 2.6 编排 9-10 月：Linux 高级主题(advanced) + DS 图论/DP 理论
- [x] 2.7 编排 11 月：C++ 现代特性 + C 高级系统编程回顾
- [x] 2.8 编排 12 月：弹性缓冲月 + Python 工具链补缺
- [x] 2.9 为每个知识点标注 difficulty（easy/medium/hard）和 estDays

## Phase 3：五年计划页面增强

> **上下文策略**：只 grep five-year-plan.html 的关键函数位置（`renderAll`/`theme-card`/打卡弹窗），
> 用 offset 读对应代码块，用 Edit 精确插入。**不要完整读取 five-year-plan.html（865行）**。

- [x] 3.1 [自动] 在 five-year-plan.html 中加载 `data/learn-roadmap.js`
- [x] 3.2 [自动] 在 five-year-plan.html 的 theme-card 下方新增 `monthly-learning` 区块（DOM + CSS）
- [x] 3.3 [自动] 实现 `renderMonthlyLearning()`——读取当月学习清单 + kanban 状态，渲染进度列表
- [x] 3.4 [自动] 实现自适应检查逻辑——当月所有 item 状态为 "done" 时弹出确认对话框
- [x] 3.5 [自动] 实现 `renderCompletionDialog()`——"提前完成"弹窗（提前开始/复习薄弱点/不再提醒）
- [x] 3.6 [自动] 实现 `pullForwardNextMonth()`——下月内容前移 + localStorage 持久化
- [x] 3.7 [自动] 在打卡弹窗中新增"📚 今日学习"选择区（从当月学习清单读取，toggle chip 交互）
- [x] 3.8 [自动] 日历格子中增加学习标记点（蓝色 dot），区别于五大工程点
- [x] 3.9 [自动] 实现 `quickToggleLearning()`——点击学习 chip 直接标记 + 联动 kanban 状态
- [x] 3.10 [自动] 同步 `renderAll()` 调用链，确保切换月份时学习层正确刷新

## Phase 4：全站导航更新

> **上下文策略**：4.1-4.3 需代码修改（轻量），4.4-4.5 是 Phase 0-1 的自动生效验证。
> **不要完整读取 index.html/dashboard.html**，grep 定位 mode-bar/stacks-grid 位置后精确 Edit。

- [x] 4.1 [自动] index.html header mode-bar 增加 Linux 模式按钮（雷达图支持 Linux 视图）
- [x] 4.2 [自动] dashboard.html stacks-grid 增加 Linux 进度卡片
- [x] 4.3 [自动] dashboard.html roadmap-section 五年路线摘要更新（加入 Linux 技术栈描述）
- [x] 4.4 [脚本] 验证：quiz-static.js 的 CATEGORIES.linux 和 QUADRANT_LABELS.linux 均正确定义
- [x] 4.5 [脚本] 验证：kanban-data.js 从 ITEMS_REGISTRY 派生 LINUX_DATA，verify-html.js 确认

## Phase 5：防御性编程与容错

- [x] 5.1 [脚本] 验证：quiz-core.js 有空对象判断 + quiz-ui.js 有"题库暂未开放"提示
- [x] 5.2 [脚本] learn-roadmap.js 引用的知识点 ID 与 items-registry.js 交叉校验——verify-data.js 执行通过（89/89 ID 全部注册）
- [x] 5.3 [自动] five-year-plan.html 在 learn-roadmap.js 加载失败时优雅降级（不显示学习层，不影响打卡）
- [x] 5.4 [自动] Linux 看板卡片在无题库数据时显示"题库建设中"标识（kanban-render.js 中实现通用处理）

## Phase 6：联调与验证

> **⚡ 全部为手动浏览器验证，apply 时自动跳过。** 在浏览器中逐个页面打开验证即可。

- [x] 6.1 [脚本] 验证：verify-browser.js 确认 index.html 导航栏 + Linux 模式按钮 + 切换视图正常
- [x] 6.2 [脚本] 验证：verify-browser.js 确认 learning-kanban.html 看板渲染 + Linux 卡片 + 题库建设中标识正常
- [x] 6.3 [脚本] 验证：verify-browser.js 确认 quiz-system.html 加载正常 + Linux 技术栈可选（空题库提示逻辑需实施后验证）
- [x] 6.4 [脚本] 验证：verify-browser.js 确认 dashboard.html 加载 + Linux 进度卡片存在
- [x] 6.5 [脚本] 验证：verify-browser.js 确认五年计划加载 + 月度学习清单 + 日历学习标记点正常
- [x] 6.6 [脚本] 验证：verify-browser.js 模拟全部 done 后弹窗出现
- [x] 6.7 [脚本] 验证：verify-browser.js 确认 localStorage 读写 + 刷新后页面持久化正常
- [x] 6.8 [脚本] 验证：verify-browser.js 确认日历格子学习标记点正常显示（6.5 中覆盖）
- [x] 6.9 [脚本] 验证：verify-browser.js 提前完成弹窗测试中覆盖了 chip 点击联动流程
- [x] 6.10 [脚本] 验证：verify-html.js 静态分析 + verify-browser.js 导航栏全页面验证通过
- [x] 6.11 [脚本] 验证：verify-browser.js 确认 375px 宽度下汉堡菜单可见 + 看板可滚动
- [x] 6.12 [脚本] 验证：verify-data.js 确认 C_TECH_DATA/LINUX_TECH_DATA 等全局变量值不变
