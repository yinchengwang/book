## Why

reading-radar 是一个纯静态的交互式技术学习可视化工具，目前已具备雷达图、看板、测验、仪表盘四层联动框架，但题库覆盖深度不足（每个知识点仅 5 道题）、缺少配套学习内容、DB 技术栈缺失分布式与向量数据库专题、知识提示每次不刷新、五年计划与主流程割裂，且 README 已严重落后于实际设计。本次升级系统补齐上述短板，使其成为完整可用的技术成长看板。

---

## 现有系统设计（基线）

### 文件结构

```
project/reading-radar/
├── index.html              # 雷达图 + 书籍看板（Layer 1 / 3 联动入口）
├── learning-kanban.html    # 知识点学习看板（Layer 1）
├── quiz-system.html        # 知识测验系统（Layer 2 / 3 联动）
├── dashboard.html          # 聚合仪表盘（Layer 4）
├── five-year-plan.html     # 五年计划页（孤立）
├── server.js               # 本地 HTTP 服务（可选，支持多设备）
├── data/
│   ├── quiz-tech.js        # 所有知识点元数据（C/C++/DS/DB/PY）
│   ├── quiz-static.js      # 静态配置（CATEGORIES / DAILY_TIPS 等）
│   ├── quiz-questions-c.js     # C 题库（~43 知识点，各 5 题）
│   ├── quiz-questions-cpp.js   # C++ 题库（~43 知识点，各 5 题）
│   ├── quiz-questions-ds.js    # 数据结构题库（各 5 题）
│   ├── quiz-questions-db.js    # DB 题库（29 知识点已有，30 待补）
│   ├── quiz-questions-py.js    # Python 题库（43 知识点，部分已扩至 10 题）
│   ├── radar-tech.js           # 书籍雷达元数据
│   └── kanban-data.js          # 看板卡片数据
└── user-data/              # 用户状态（localStorage 或本地 JSON）
```

### 四层联动架构

| 层级 | 页面 | 作用 |
|------|------|------|
| Layer 1 | learning-kanban.html | 卡片右上角显示评测分 badge；"去测试"链接跳转 |
| Layer 2 | quiz-system.html | 评测完成后驱动看板状态（建议下一步）|
| Layer 3 | index.html（雷达） | 节点内圈颜色反映评测掌握度 |
| Layer 4 | dashboard.html | 聚合所有技术栈的历史、进度、盲点、热力图 |

### 数据结构规范

```js
// 题库双层嵌套结构
QUESTION_BANK[catId] = { [catId]: { [itemId]: [...题目数组] } }

// 题目格式
{ id, type, difficulty, scenario, stem, options, answer, explanation, code? }
// type: choice | predict_output | true_false | fill_blank | multi_choice
```

### 知识点技术栈

| 技术栈 | 知识点数 | 代码 | 状态 |
|--------|---------|------|------|
| C 语言 | 43 | c | 各 5 题，待扩充 |
| C++ | 43 | cpp | 各 5 题，待扩充 |
| 数据结构 | 若干 | ds | 各 5 题，待扩充 |
| 数据库 | 59（定义），29（已有题库） | db | 部分 5 题，待扩充 + 补充新知识点 |
| Python | 43 | py | 前 16 个已扩至 10 题，剩余 27 个待扩充 |

### 现有 DB 知识点分类（59 个已定义，29 个有题库）

已有：基础 SQL、事务、索引、存储引擎、查询优化器、锁与并发、主从复制、分区表等。
缺失：Redis 事件驱动、Redis 数据结构底层、分布式事务、Raft/Paxos、向量数据库（HNSW/IVF/DiskANN/PQ 量化）及其索引优化。

---

## What Changes

- **题库扩充**：Python（43 知识点）、C、C++、DS 各知识点从 5 题扩到 10 题（分批执行 A4-A10、B 系列等）
- **DB 题库补充**：新增分布式数据库（Raft、分布式事务、ShardingSphere）+ 向量数据库（Milvus、HNSW、IVF、PQ 量化、索引调优）共约 30 个新知识点及其题库
- **随机知识提示**：quiz-system.html 每次切换页面自动随机换一条 tip（已实现，纳入规范）
- **五年计划融合**：在 dashboard.html 嵌入五年计划摘要卡，或将 five-year-plan.html 增加返回仪表盘链接，实现导航闭环
- **学习内容页**：为每个知识点生成 `learn.html`（通用）+ `data/learn-content-*.js`（各技术栈内容数据），内容从入门到实战，含调优专题
- **README 更新**：补全四层联动设计、文件路径说明、数据结构规范、各技术栈知识点清单
- **实战/面试专题**：在 quiz-static.js 的 DAILY_TIPS 中补充实战/面试向提示；知识点 desc 补充面试常考点标注

---

## Capabilities

### New Capabilities

- `quiz-bank-expansion`：题库扩充引擎——Python/C/C++/DS 各知识点从 5 题扩充至 10 题，含批次执行规范与验证流程
- `db-distributed-vector`：DB 技术栈新增分布式（Raft/分布式事务/ShardingSphere）+ 向量数据库（Milvus/HNSW/IVF/PQ/索引调优）约 30 个知识点及题库
- `learn-content-page`：知识点学习内容页——通用 learn.html + 各技术栈 learn-content-*.js 数据文件，内容从入门到实战调优
- `five-year-plan-integration`：五年计划与 dashboard.html 仪表盘融合，导航闭环
- `readme-update`：README 全面更新，覆盖现有设计、联动架构、文件结构、数据规范

### Modified Capabilities

- `quiz-tip-rotation`：quiz-system.html 随机 tip 切换（已实现），补充规范文档说明交互行为

---

## Impact

- **修改文件**：`quiz-questions-py.js`、`quiz-questions-c.js`、`quiz-questions-cpp.js`、`quiz-questions-ds.js`、`quiz-questions-db.js`、`quiz-tech.js`（新增 DB 知识点元数据）、`quiz-system.html`、`dashboard.html`、`five-year-plan.html`、`README.md`
- **新增文件**：`learn.html`、`data/learn-content-c.js`、`data/learn-content-cpp.js`、`data/learn-content-ds.js`、`data/learn-content-db.js`、`data/learn-content-py.js`
- **无破坏性变更**：所有修改向下兼容，localStorage 键名不变，现有用户数据不受影响
- **无外部依赖**：纯静态站，不引入新的第三方库
