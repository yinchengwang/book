## Context

reading-radar 是纯静态 HTML/JS/CSS 站点，直接用浏览器打开，无构建工具。所有数据文件（`data/*.js`）通过 `<script>` 标签全局挂载，页面间通过 `localStorage` 共享状态。当前已完成四层联动框架（雷达图→看板→测验→仪表盘），但内容深度和覆盖面不足。本次升级在不引入任何构建工具和外部依赖的前提下进行。

## Goals / Non-Goals

**Goals:**
- Python/C/C++/DS 各知识点题目从 5 道扩充到 10 道
- DB 题库新增分布式（Raft/分布式事务/ShardingSphere）+ 向量数据库（Milvus/HNSW/IVF/PQ/调优）约 30 个知识点及题库
- 每个知识点提供配套学习内容页（入门→原理→实战→调优），通过统一的 `learn.html` + 各栈数据文件呈现
- dashboard.html 嵌入五年计划摘要，five-year-plan.html 增加返回仪表盘链接，导航闭环
- README 全面更新，覆盖联动架构、文件结构、数据规范

**Non-Goals:**
- 不引入 React/Vue/打包工具，保持纯静态
- 不修改 localStorage 键名（防止现有用户数据丢失）
- 不重构现有四层联动代码（只扩展，不重写）
- 不增加服务端功能（server.js 保持不变）

## Decisions

### D1：题库扩充策略——`replace_string_in_file` 精确插入

**决策**：使用编辑工具定位每个知识点的最后一道题（q5），在其末尾 `},` 后精确插入新题，而非重写整个文件。

**理由**：题库文件体积大（quiz-questions-py.js 约 1800+ 行），全文替换风险高；PowerShell heredoc 含中文时有编码问题；精确插入只需找到唯一锚点，操作最小、最安全。

**替代方案**：全文 Set-Content 重写——风险高，一次性内容过多容易截断；已验证 PowerShell -replace 跨行失效。

### D2：学习内容页——单 HTML 页 + 多数据文件

**决策**：创建一个通用的 `learn.html`，通过 URL hash（`learn.html#py/py-basic-types`）确定技术栈和知识点，动态加载对应数据文件中的内容并渲染。数据文件按技术栈分割：`data/learn-content-py.js`、`data/learn-content-c.js` 等。

**数据结构**：
```js
const LEARN_CONTENT = {
  "py-basic-types": {
    title: "Python 基本类型",
    sections: [
      { level: "入门", content: "..." },
      { level: "原理", content: "..." },
      { level: "实战", content: "..." },
      { level: "调优/面试", content: "..." }
    ]
  }
}
```

**理由**：单 HTML 可复用所有技术栈；按栈分文件避免单文件过大（每个数据文件约 200-500KB）；hash 路由无需服务端支持，兼容 `file://` 协议。

**替代方案**：每个知识点一个 HTML 文件——文件数量爆炸（43×5=215 个），维护成本极高。

### D3：DB 新知识点元数据——追加到 quiz-tech.js

**决策**：向量数据库和分布式新知识点追加到 `quiz-tech.js` 的 `DB_TECH_DATA` 数组末尾，题库内容追加到 `quiz-questions-db.js` 的 `db.db` 嵌套对象中。

**知识点分类规划**（新增约 30 个）：

| 分类 | 知识点 ID | 数量 |
|------|-----------|------|
| 分布式事务 | db-dist-tx, db-saga, db-tcc | 3 |
| 共识算法 | db-raft, db-paxos | 2 |
| 分布式数据库 | db-sharding, db-newSQL, db-tidb | 3 |
| 向量数据库基础 | db-vector-intro, db-milvus, db-chroma | 3 |
| 向量索引算法 | db-hnsw, db-ivf, db-pq, db-diskann, db-scann | 5 |
| 索引调优 | db-ann-eval, db-index-tuning, db-hybrid-search | 3 |
| Redis 底层 | db-redis-object, db-redis-event, db-redis-aof | 3 |
| Redis 分布式 | db-redis-cluster, db-redis-sentinel | 2 |
| 其余缺失项 | db-mvcc, db-lsm, db-wal, db-buffer-pool, db-columnstore, db-bloom | 6 |

### D4：五年计划融合——dashboard.html 嵌入摘要卡

**决策**：在 dashboard.html 底部新增"五年成长路线"摘要区块，从 five-year-plan.html 提取核心阶段数据（硬编码到 dashboard.html，不新增数据文件），five-year-plan.html 顶部增加返回仪表盘的链接。

**理由**：五年计划数据量小且相对静态，硬编码成本低；避免引入新的跨页面数据依赖。

### D5：README 更新——全量重写

**决策**：全量重写 README.md，结构为：快速开始 → 系统架构（四层联动图）→ 文件结构（带说明）→ 数据规范 → 各技术栈知识点清单 → 学习内容页使用说明 → 贡献指南。

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|---------|
| 题库文件过大，浏览器加载变慢 | 每个文件单独 `<script>` 加载，仅在对应技术栈激活时才解析；learn-content 文件按需用动态 `<script>` 插入 |
| 精确插入锚点定位失败（知识点名拼写不一致） | 每次插入前 Select-String 确认行号，插入后 node --check 验证语法 |
| learn.html hash 路由在 file:// 下兼容性 | 用 `window.location.hash` 解析，不依赖 History API，兼容所有浏览器的 file:// 模式 |
| DB 新知识点元数据与题库 ID 不匹配 | quiz-tech.js 和 quiz-questions-db.js 同步提交，ID 命名严格遵循 `db-<name>` 规范 |
| learn-content 数据文件单文件过大 | Python 43 个知识点内容可能达 1-2MB；可按 ring（basic/intermediate/advanced）拆分，但当前阶段先合并，视实际大小再拆 |

## Open Questions

1. learn.html 的内容是否需要支持 Markdown 渲染？建议优先用纯 HTML 字符串（section.content 直接赋给 innerHTML），避免引入 marked.js 等依赖。
2. 五年计划的阶段数据是否需要可编辑？当前方案硬编码，后续可考虑 localStorage 存储用户自定义节点。
3. C/C++ 题库扩充（A 系列之后）优先级：建议与 Python 并行，Python 先完成（A4-A10），C/C++ 随后启动。
