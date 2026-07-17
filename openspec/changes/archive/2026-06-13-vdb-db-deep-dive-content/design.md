## Context

reading-radar 的学习页（learn.html）当前为每个知识点提供了 4 段通用导学（入门路径/核心原理/实战与工作/调优与面试），内容密度低，且 VDB（14个）和 Linux（62 个）完全没有 learn-content 文件，走 fallback 生成更单薄的内容。

用户希望基于雷达图的 296 个知识点体系，在 learn.html 中补充 blog 风格的深度文章（.md 文件），使得**点击知识点后可以直接阅读有营养的讲解**，类似于技术博客的体验。

当前已有一个样章 `db-vector-basic.md`（Flat 精确搜索），写作模版和渲染方案需要统一确定。

### 约束条件

- 项目为纯静态 HTML/JS，零外部依赖（无构建工具、无 npm 包）
- learn.html 约 1200 行，已有完善的 hash 路由机制（`#stack/itemId`）
- 知识点 ID 在 items-registry.js 中唯一定义，.md 文件命名与之对齐
- CLAUDE.md 有严格的先 grep 后读的纪律，每篇 .md 独立文件不撑爆上下文
- 用户使用 VSCode 编辑，习惯 Markdown 语法

## Goals / Non-Goals

**Goals:**

- 每篇深度文章用独立 `.md` 文件存储，用户可在 VSCode/Obsidian 中自由编辑
- learn.html 在现有 4 段导学后新增第 5 段「📖 深度阅读」，懒加载对应 .md 文件渲染
- 渲染器支持 Markdown 子集：标题（##）、段落、代码块（```）、表格、链接、无序列表
- 渲染器纯 innerHTML，不引入任何第三方库
- 每篇文章按 8 段模版写作：引子 → 核心原理 → 复杂度 → 代码 → 工程优化 → 生产实践 → 论文溯源 → 面试考点 + 参考
- 覆盖范围按顺序：VDB（14 篇）→ DB（~20 篇）→ Linux（62 篇）
- 每篇 .md 写完后无需改 learn.html，只需新建文件

**Non-Goals:**

- 不改 items-registry.js / radar-tech.js / quiz-tech.js / kanban-data.js
- 不改 quiz-system.html / index.html / dashboard.html / 其他页面
- 不引入 marked.js / highlight.js 等第三方库
- 不修改现有的 4 段导学内容（learn-content-*.js）
- 不写 C/CPP/DS/PY 的深度文章（当前阶段只做 VDB/DB/Linux）

## Decisions

### 决策 1：文件格式 → 纯 Markdown（.md）

| 方案 | 评价 |
|------|------|
| ✅ **纯 .md 文件** | 用户可在 VSCode 随意编辑，和 notes/ 风格一致，零语法噪音 |
| ❌ JSON | 编辑体验差（逗号、引号、不能多行字符串），不适合长篇内容 |
| ❌ JS 模板字面量 | 不如 .md 自由，且需包裹在 JS 函数作用域中 |

**结论**：每个知识点一个 `.md` 文件，存在 `data/learn-deep/<item-id>.md`。

### 决策 2：渲染方案 → 简易 Markdown 渲染器

| 方案 | 评价 |
|------|------|
| ✅ **自写 ~50 行渲染器** | 零依赖，只处理需要的语法（标题/代码/表格/链接/列表），项目一致 |
| ❌ marked.js | 引入 30KB 外部依赖，和项目"零依赖"原则冲突 |
| ❌ 直接 innerHTML 全文 | 不安全（可能执行脚本），且无法给代码块加样式 |

**结论**：实现 `function renderMarkdown(md)`，通过正则逐行解析，只支持：

```
语法               → 输出
## 标题            → <h3>
段落               → <p>
```code```          → <pre><code>
| table |          → <table><tr><td>
[text](url)        → <a href>
- 列表              → <ul><li>
> 引用              → <blockquote>
```

### 决策 3：加载策略 → 懒加载（按需 fetch）

- 用户点击知识点 → hash 变化 → HTML 渲染完成后 → renderDeepDive() 触发
- fetch `data/learn-deep/${item.id}.md`
- 200 → 渲染 markdown
- 404 → 显示「📝 深度文章待补充」（不报错，不影响其他 section）
- 浏览器缓存 `.md` 文件，切换回同一知识点时不重复请求

### 决策 4：文章模版 → 8 段式

每篇 .md 按此模版写作：

```markdown
# 标题

## 一、引子
<!-- 为什么学、解决什么问题、直觉比喻 -->

## 二、核心原理
<!-- 算法流程、图解/手算、数学直觉 -->

## 三、复杂度分析
<!-- 时间/空间/精度三角 -->

## 四、代码说话
<!-- 多种语言：Python Faiss / C++ Faiss / C++ 手写 / Python 手写 -->

## 五、工程优化技术
<!-- SIMD / 内存布局 / 多线程 / 编译优化 / trade-off -->

## 六、生产实践 + 决策指南
<!-- 什么时候用、什么时候换、反直觉场景 -->

## 七、论文溯源
<!-- 原始论文信息（标题/作者/会议/引用量）、核心贡献、算法演化谱系 -->

## 八、面试必考点 + 参考
<!-- 高频问题速查 + 扩展链接 -->
```

### 决策 5：learn-content 补齐 → 与 learn-content-*.js 风格一致

补齐 VDB 和 Linux 的 `learn-content-*.js`，结构与现有 learn-content-db.js 完全一致：

```javascript
// learn-content-vdb.js
window.LEARN_CONTENT = window.LEARN_CONTENT || {};
const FOCUS = { "db-vector-basic": "..." };
window.LEARN_CONTENT.vdb = Object.fromEntries(
  VDB_TECH_DATA.map(item => [item.id, buildContent(item)])
);
```

VDB_TECH_DATA 和 LINUX_TECH_DATA 已分别在 radar-tech.js 和 items-registry.js 中可用。

### 决策 6：写作顺序

```
Phase 1: VDB（14 篇）
  子阶段 1.1: db-ivf-variants, db-hnsw, db-graph-index  ← 核心索引算法
  子阶段 1.2: db-pq-quant, db-quantization, db-scalar-quant  ← 量化技术
  子阶段 1.3: db-diskann, db-milvus-arch, db-milvus-segment  ← 系统工程
  子阶段 1.4: db-milvus-index, db-milvus-search, db-hybrid-search  ← 系统工程续
  子阶段 1.5: vdb-pinecone-serverless, vdb-pinecone-storage  ← 产品对比

Phase 2: DB（~20 篇，选取核心知识点）
  子阶段 2.1: 存储引擎（db-page-block, db-buffer-pool, db-wal, db-lsm, db-compaction）
  子阶段 2.2: 索引与查询（db-btree-idx, db-btree-impl, db-optimizer, db-executor）
  子阶段 2.3: 事务与恢复（db-mvcc-principle, db-locking, db-redo-log-detail, db-aries）
  子阶段 2.4: Redis（db-redis-event, db-redis-object, db-redis-persist, db-redis-cluster）
  子阶段 2.5: 分布式与嵌入式（db-consensus, db-sharding, db-ha, db-sqlite-arch）

Phase 3: Linux（62 篇，按能力组递进）
  子阶段 3.1: Linux 可观测性（~12 篇）
  子阶段 3.2: Linux 内核子系统（~14 篇）
  子阶段 3.3: Linux 存储与文件系统（~16 篇）
  子阶段 3.4: Linux 网络栈（~20 篇）

Phase 4: learn.html 渲染适配 + 导学补齐
  4.1: 新增 learn-content-vdb.js / learn-content-linux.js
  4.2: learn.html SECTION_META 追加 + renderDeepDive()
```

**不要求所有 .md 写完再改 learn.html。先改 learn.html，让已存在的 .md 能显示，尚未写的显示「待补充」。**

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|---------|
| 自写渲染器功能不足（如不支持嵌套列表、表格对齐） | 按需迭代，目前只覆盖样章中出现的语法；必要时降级为展示纯文本 |
| 每个 .md 页面内容量大（样章约 400 行），后续扩展到 124 篇需大量写作 | VDB→DB→Linux 依次推进，每个 Phase 独立可交付 |
| learn.html 一次定型后不再改，但 renderDeepDive() 初次实现可能有 bug | 用样章 `db-vector-basic.md` 作为测试用例，确认渲染效果后再批量写入 |
| 纯内部渲染不考虑 XSS（团队项目，无外部内容注入） | 可接受。渲染器仍然对 HTML 特殊字符做转义（`escapeHtml()` 复用现有函数） |
| 每篇文章都需要 Google/论文搜索查资料 | 已规划在写作流程中 |
