## Context

阅读雷达项目（reading-radar）现有 6 个技术栈的题库文件，分布在 10 个 JS 文件中：

```
quiz-questions-c.js     (2354 行)    ← 按象限注释分区
quiz-questions-cpp.js   (2131 行)    ← 按象限注释分区
quiz-questions-ds.js    (2606 行)    ← 按象限注释分区
quiz-questions-db.js    (3998 行)    ← 按知识点 ID 分区（最大）
quiz-questions-py.js    (2238 行)    ← 按象限注释分区
quiz-questions-linux-*.js  (4×3 行) ← 4 个空壳文件
```

当前数据架构已实现了 `ITEMS_REGISTRY` 统一注册中心，`quiz-tech.js`/`radar-tech.js`/`kanban-data.js` 从中自动派生。但题库数据（`quiz-questions-*.js`）仍以 `QUESTION_BANK` 全局变量挂载，结构独立于注册中心。

**核心问题：**
1. 文件过大（`db.js` 接近 4000 行），难以维护和补充
2. 传统数据库（MySQL/PostgreSQL/Redis/ES/SQLite）和向量数据库（Faiss/Milvus/Pinecone/Chroma/DiskANN）混在一个 `db` 技术栈下，技术路线和用户目标不同
3. 题目对象缺少 `quadrant` 字段，象限筛选功能依赖 `!quadrant` 短路回退，实际失效
4. Linux 题库文件为空，等待填充

**主要约束：**
- 纯静态 HTML/JS，无构建工具
- 全局变量挂载模式（`window` 下共享数据）
- `ITEMS_REGISTRY` 是知识点唯一定义源
- 不增加外部依赖

## Goals / Non-Goals

**Goals:**

1. 数据库技术栈拆分为 `db`（传统）和 `vdb`（向量），各自拥有独立导航项、CATEGORIES、象限标签
2. 所有题库文件按象限维度拆分为小文件（每文件 ~300-600 行）
3. 题库文件按产品粒度组织命名（`quiz-questions-db-mysql.js`、`quiz-questions-vdb-faiss.js` 等）
4. 所有迁移题目补充 `quadrant` 字段，修复象限筛选 bug
5. 看板渲染支持按产品分组的子章节视图
6. 补充 Linux 题库内容（基于现有 56 个 Linux 知识点）
7. 新增 SQLite 知识点和题目

**Non-Goals:**

- 不改变 `quiz-core.js` 的选题/判题核心逻辑
- 不改变雷达图 (`radar-tech.js`) 和五年计划 (`five-year-plan.html`) 的行为
- 不引入构建工具或运行时依赖
- 不改变已知存在的题库数据内容（仅迁移、拆分、补充字段，不改题目本身）
- 不处理 openGauss/Qdrant/Weaviate 等无现有 items-registry 条目的产品

## Decisions

### D1: 技术栈拆分方案

items-registry.js 中 `stack:"db"` 的条目分为两类：

| 条目范围 | 迁移目标 | 说明 |
|---------|---------|------|
| `db-storage-overview` → `db-sstable`（language 象限，13 条） | `stack:"db"` 不变 | 存储引擎核心概念 |
| `db-btree-idx` → `db-executor-detail`（systems 象限，去重） | `stack:"db"` 不变 | B+树/SQL/优化器/执行引擎 |
| `db-acid` → `db-2pc`（algorithms 象限，12 条） | `stack:"db"` 不变 | 事务/并发/恢复 |
| `db-redis-*`（engineering 象限，6 条） | `stack:"db"` 不变 | Redis 归属传统 DB |
| `db-consensus/sharding/ha/perf`（engineering 象限，4 条） | `stack:"db"` 不变 | 分布式/性能 |
| `db-vector-basic` → `db-diskann`（systems 象限，~9 条） | **→ stack:"vdb"** | 向量索引算法 |
| `db-quantization/scalar-quant/pq`（systems，~3 条） | **→ stack:"vdb"** | 量化压缩 |
| `db-ann-eval/hybrid-search/index-tuning`（systems/engineering，~3 条） | **→ stack:"vdb"** | ANN 评估与混合检索 |
| `db-milvus-*`（engineering，4 条） | **→ stack:"vdb"** | Milvus 专属 |

items-registry 调整约 19 条记录的 `stack` 字段，约 40 条保持为 `db`。

**为何不新建第三个栈？** Redis/ES 等虽然在产品维度与 MySQL 不同，但它们共享相同的关系型/键值型技术范式（B+树、WAL、ACID、主从复制），与向量检索的技术范式差异远大于它们之间的差异。产品和范式两个维度正交——产品归产品（看板子章节），技术范式归技术栈。

### D2: 象限标签保持统一

`db` 和 `vdb` 共享同一套象限语义（`language/systems/algorithms/engineering`），但标签文案不同：

```
QUADRANT_LABELS = {
  db:   { language:'存储引擎', systems:'索引与查询', algorithms:'事务与并发', engineering:'分布式与系统' },
  vdb:  { language:'向量基础', systems:'索引算法',   algorithms:'量化与压缩', engineering:'系统与工程' },
}
```

`vdb` 的象限标签侧重向量 DB 领域自身知识体系。

### D3: 题目文件按产品粒度组织

每个产品的知识点条目按象限分组，辅以难度分层。题目对象显式携带 `quadrant` 和 `ring` 字段。

```
// 文件命名规则：
//   quiz-questions-<stack>-<product>.js
//
// 传统 DB（db）产品文件：
┌─────────────────────────────────────────────────────────────────────┐
│ quiz-questions-db-mysql.js     ← MySQL/InnoDB 专属概念              │
│                                   (存储/buffer-pool/WAL/B+树/       │
│                                    MVCC/锁/Redo/事务/优化器/...)    │
│                                                                     │
│ quiz-questions-db-postgres.js  ← PostgreSQL 专属                    │
│                                   (SSI/2PC/空间索引...)             │
│                                                                     │
│ quiz-questions-db-redis.js     ← Redis（事件/RESP/对象/持久化/复制）│
│                                                                     │
│ quiz-questions-db-rocksdb.js   ← LSM-Tree 系（LSM/SSTable/          │
│                                    Compaction）                      │
│                                                                     │
│ quiz-questions-db-elasticsearch.js ← ES（倒排索引/全文检索）        │
│                                                                     │
│ quiz-questions-db-sqlite.js    ← SQLite（新增）                      │
│                                                                     │
│ quiz-questions-db-general.js   ← 跨产品通用概念                      │
│                                    (列存储/ACID/分片/共识/高可用/   │
│                                     性能调优/SQL DDL/DML/DCL)       │
└─────────────────────────────────────────────────────────────────────┘

// 向量 DB（vdb）产品文件：
┌─────────────────────────────────────────────────────────────────────┐
│ quiz-questions-vdb-faiss.js    ← Faiss 系                           │
│                                   (IVF/HNSW/Flat/PQ/SQ/...)        │
│                                                                     │
│ quiz-questions-vdb-milvus.js   ← Milvus 专属                        │
│                                   (架构/段管理/索引构建/混合查询)   │
│                                                                     │
│ quiz-questions-vdb-chroma.js   ← Chroma 专属（单一文件）            │
│                                                                     │
│ quiz-questions-vdb-diskann.js  ← DiskANN（Vamana/磁盘索引）          │
│                                                                     │
│ quiz-questions-vdb-pinecone.js ← Pinecone（先建框架，后续填充）     │
│                                                                     │
│ quiz-questions-vdb-general.js  ← 跨产品通用概念                      │
│                                    (ANN评测/图索引/混合检索)        │
└─────────────────────────────────────────────────────────────────────┘
```

**产品归属的判断原则：**
- 某产品独占的概念（如 Milvus 架构、Redis RESP 协议）→ 归该产品文件
- 某产品是该概念的代表性实现（如 MySQL 的 InnoDB 与 B+ 树）→ 归该产品文件，但题目描述中不限于该产品
- 跨产品通用概念（如 ACID、共识算法、分片）→ 归 `general.js`
- 多产品共有但某产品为发源地的概念（如 ESLint→倒排索引）→ 归该产品文件

### D4: 象限字段补充策略

每道题需要 `quadrant` 字段。对于按象限注释分区的文件（c/cpp/ds/py），从注释分组名推导；对于按知识点 ID 分区的文件（db），从 items-registry 中查询该知识点的 `quadrant`。

题目对象模型规范化：

```js
{
  id: "c-syntax-q1",
  type: "choice",
  difficulty: "basic",      // 已存在 → 导出为 ring
  quadrant: "language",     // 新增（从注释分区推导）
  ring: "basic",            // 新增（等于 difficulty）
  scenario: "...",          // 已存在
  stem: "...",              // 已存在
  code: "...",              // 可选的
  options: [...],           // 已存在
  answer: "...",            // 已存在
  explanation: "..."        // 已存在
}
```

迁移后的 `difficulty` 字段**保留**，同时新增 `ring` 字段与之相等，保障 `selectQuestions()` 中 `q.ring` 和 `q.difficulty` 两种访问路径都兼容。

### D5: 看板产品分组

在 `kanban-render.js` 中新增子章节分组模式。知识点先按 `product` 字段（从 tags 或新字段推断）分组，组内再按象限排列。

实现方式：在 `ITEMS_REGISTRY` 条目中新增可选的 `product` 字段，标记该知识点归属的产品。看板渲染检测到 `groupBy: "product"` 时使用双层分组（产品→象限）。

```
看板双层视图结构：
┌─ MySQL（7 个知识点）──────────────┐
│ 📗 存储引擎      进度 ████░░ 50% │
│   ├ db-storage-overview          │
│   ├ db-page-block                │
│   └ ...                          │
│ 📘 索引与查询    进度 ██░░░░ 25% │
│   ├ db-btree-idx                 │
│   └ ...                          │
├─ Redis（6 个知识点）───────────────┤
│ 📕 分布式与系统  进度 ████░░ 50% │
│   ├ db-redis-event               │
│   └ ...                          │
└──────────────────────────────────┘
```

看板默认仍按象限分组，产品分组作为可选模式。

### D6: 题库加载机制

现有 `QUESTION_BANK` 使用直接赋值模式：
```js
// 当前单个大文件：
QUESTION_BANK.db = { db: { "topic-id": [ ... ] } }

// 小文件使用 Object.assign 合并：
QUESTION_BANK.db = Object.assign(QUESTION_BANK.db || {}, {
  "topic-id": [ // 只包含该产品知识点 + 象限的题目 ]
})
```

拆分后每个产品文件只导出该产品的知识点集合，多个文件通过 `Object.assign` 合并到同一个 `QUESTION_BANK.db`（传统 DB）或 `QUESTION_BANK.vdb`（向量 DB）下。

### D7: 导航栏

`nav-component.js` 新增 vdb 的导航项：
```js
items: [
  { id: "vdb", label: "向量DB", href: "quiz-system.html?stack=vdb", icon: "🔮" },
]
```
同时调整 `CATEGORIES` 注册，使 `quiz-system.html` 的 URL 参数 `?stack=db` 和 `?stack=vdb` 分别指向不同的技术栈。

`quiz-system.html` 的技术栈选择器增加「向量DB」入口。

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| `vdb` 新栈的某些象限初始题量不足（如 `vdb.language` 象限只有基础概念） | 用户选中该象限时题目不足 | 在 `quiz-core.js` 中补充不足时的回退逻辑：如果某个象限/难度组合不足 10 题，从相邻难度自动补题 |
| 产品归属判断主观，部分知识点边界模糊（如 `db-spatial` 归 MySQL 还是 PostgreSQL） | 题目在不同产品文件间错放 | 不以产品作为唯一分类——边界模糊的知识点同时在两个产品文件中保留引用（但数据只定义一次，通过 import/merge 共享），或归入 general.js |
| `quiz-system.html` 的 `<script>` 标签从 5 个增加到约 20 个 | 页面加载时间略微增加 | 纯静态 `<script>` 加载无依赖问题；按需加载推迟到用户选择技术栈时动态注入更可取，但本次不做（超出非目标范围） |
| `product` 字段新增到 ITEMS_REGISTRY 会增加维护成本 | 每个条目多一个字段 | 大多数条目可自动推导（从 id 前缀），仅边界条目需要手动标注 |
| 现有 `quiz-questions-db.js` 中按知识点 ID 组织的题目与按产品组织不完全对齐（某些 ID 包含多种产品共有的概念） | 题目重复或遗漏 | 交叉概念归入 general.js；确实专属某个产品的条目才放入该产品文件 |

## Migration Plan

### Phase 1: 基础设施（items-registry + 技术栈元数据）

1. items-registry.js 中将 ~19 个向量 DB 条目的 `stack:"db"` 改为 `stack:"vdb"`
2. 新增 `db-sqlite` 条目
3. quiz-tech.js 自动生成 `VDB_TECH_DATA`（基于 `stack:"vdb"` 过滤）
4. quiz-static.js 新增 `CATEGORIES.vdb` 和 `QUADRANT_LABELS.vdb`
5. nav-component.js 新增 vdb 导航项
6. 验证：`VDB_TECH_DATA.length === 19`，导航栏正常显示 vdb 标签

### Phase 2: 题库文件拆分（按栈 + 按产品）

1. 从 `quiz-questions-db.js` 按知识点 ID 提取传统 DB 题目到各产品文件
2. 从 `quiz-questions-db.js` 按知识点 ID 提取向量 DB 题目到各产品文件
3. 删除 `quiz-questions-db.js`
4. 其他文件（c/cpp/ds/py）按象限注释分区拆为 4 个文件各
5. 给所有迁移题目标注 `quadrant` 和 `ring` 字段
6. quiz-system.html 更新 `<script>` 加载标签
7. 验证：`selectQuestions()` 象限筛选返回正确结果

### Phase 3: 看板产品分组

1. items-registry.js 中新增 `product` 字段
2. kanban-render.js 新增 `groupBy: "product"` 双层渲染模式
3. index.html / learning-kanban.html 中调用时传入产品分组选项
4. 验证：看板按产品分组展示，组内按象限排列

### Phase 4: Linux 题库 + SQLite 新题

1. 填充 `quiz-questions-linux-observability.js` 等 4 个文件
2. 新建 `quiz-questions-db-sqlite.js`
3. 可能补充其他产品文件的题目

**回滚策略：** 每个 Phase 独立可回滚。保留原 `quiz-questions-*.js` 打包备份。提交时文件级别的 git 历史可追溯。

## Open Questions

1. **`product` 字段是否需要直接写在 items-registry 中，还是通过 tags 推断？** 前者明确但增加维护成本，后者依赖约定。倾向直接加字段，清晰优先于自动化。
2. **传统 DB 的 `general.js` 会不会成为第二个大文件？** 预计 ~8 个知识点（列存储/ACID/分片/共识/HA/性能/SQL DDL/DML/DCL），约 500-800 行，可控。如果后续膨胀可进一步拆分。
3. **产品文件命名中，MySQL/InnoDB 混用怎么处理？** 文件名为 `mysql`，内容涵盖 InnoDB 引擎层面知识。PostgreSQL 同理。
4. **看板产品分组是替换象限分组还是并存？** 并存。看板默认象限分组，通过下拉切换产品分组。
