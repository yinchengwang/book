## Why

reading-radar 项目当前有 296 个知识点（C/CPP/DS/DB/PY/Linux/VDB），但学习页（learn.html）每个知识点只有 4 段通用导学（各 3 条 bullet），内容密度低，不适合直接用于学习。用户希望**基于雷达图的知识点体系，补充 blog 风格的深度内容**，点击知识点后直接可读可学。

同时 VDB（14 个）和 Linux（62 个）完全没有 `learn-content-*.js` 的 4 段式导学，需要补齐。DB（48 个）已有导学但深度不足。

## What Changes

### 新增文件

**learn-content 导学文件**（补齐缺失 stack）：
- `data/learn-content-vdb.js` — VDB 14 个知识点的 4 段式导学
- `data/learn-content-linux.js` — Linux 62 个知识点的 4 段式导学

**learn-deep 深度文章目录**（每个知识点一篇 .md）：
- `data/learn-deep/db-vector-basic.md` — ✅ 已写好（Flat 精确搜索样章）
- `data/learn-deep/db-ivf-variants.md` — IVF 家族
- `data/learn-deep/db-hnsw.md` — HNSW
- `data/learn-deep/db-graph-index.md` — 图索引族谱
- `data/learn-deep/db-pq-quant.md` — 乘积量化
- `data/learn-deep/db-quantization.md` — 量化技术全景
- `data/learn-deep/db-scalar-quant.md` — 标量量化
- `data/learn-deep/db-diskann.md` — DiskANN
- `data/learn-deep/db-milvus-arch.md` — Milvus 架构
- `data/learn-deep/db-milvus-segment.md` — Milvus 段管理
- `data/learn-deep/db-milvus-index.md` — Milvus 索引构建
- `data/learn-deep/db-milvus-search.md` — Milvus 混合查询
- `data/learn-deep/db-hybrid-search.md` — 混合查询
- `data/learn-deep/vdb-pinecone-serverless.md` — Pinecone Serverless
- `data/learn-deep/vdb-pinecone-storage.md` — Pinecone 存储索引

→ DB 48 个知识点 → 选取核心 ~20 篇深度文章
→ Linux 62 个知识点 → 按 4 个能力组逐步覆盖全部

### 修改文件

- `learn.html`：
  - `OPTIONAL_CONTENT_SCRIPTS` 追加 `learn-content-vdb.js`、`learn-content-linux.js`
  - `STACK_ORDER` 已含 `vdb` 和 `linux`（不改）
  - `SECTION_META` 追加第 5 项 `{ key: "deepdive", title: "📖 深度阅读", badge: "05" }`
  - 新增 `renderDeepDive()` 函数：fetch `data/learn-deep/<item-id>.md`，简易渲染为 HTML
  - 渲染器支持：标题、段落、代码块、表格、链接、列表 ~50 行

### 文件结构

```
project/reading-radar/data/
├── learn-content-vdb.js         ← 新建：VDB 4 段导学
├── learn-content-linux.js       ← 新建：Linux 4 段导学
└── learn-deep/                  ← 新建目录：深度文章
    ├── db-vector-basic.md       ← 👍 已完成
    ├── db-ivf-variants.md       ← VDB
    ├── ...
    ├── db-btree-idx.md          ← DB
    ├── db-mvcc-principle.md     ← DB
    ├── ...
    ├── linux-cpu-diag.md        ← Linux
    ├── linux-perf-basic.md      ← Linux
    └── ...
```

## Capabilities

### New Capabilities

- `vdb-core-index-algorithms`: VDB 核心索引算法（Flat / IVF / HNSW / 图索引族谱）
- `vdb-quantization`: VDB 量化压缩技术（PQ / SQ / OPQ / AQ / RVQ / 标量量化）
- `vdb-engineering`: VDB 系统工程实践（DiskANN / Milvus 架构/段管理/索引构建/混合查询 / Pinecone / 混合检索）
- `db-storage-engine`: DB 存储引擎（页/行/缓冲区/WAL/Checkpoint/Compaction/LSM/列存）
- `db-index-query`: DB 索引与查询引擎（B+树/哈希/全文/空间索引/SQL解析/优化器/执行引擎）
- `db-transaction-recovery`: DB 事务与恢复（ACID/MVCC/锁/死锁/Redo/ARIES/2PC/OCC）
- `db-redis`: DB Redis 子系统（事件模型/RESP协议/对象系统/持久化/复制/集群）
- `db-distributed-sqlite`: DB 分布式与嵌入式（共识/分片/HA/SQLite 架构）
- `linux-observability`: Linux 可观测性（procfs/CPU/内存/磁盘/网络/perf/eBPF/火焰图/NUMA）
- `linux-kernel-subsystems`: Linux 内核子系统（系统调用/PageCache/CFS/VMM/大页/fsync/io_uring/cgroup/RCU）
- `linux-storage-filesystem`: Linux 存储与文件系统（ext4/xfs/SSD/块层/LVM/fallocate/fio/Btrfs/NFS/FUSE/RAID）
- `linux-networking`: Linux 网络栈（TCP状态机/Socket缓冲区/tcpdump/epoll/零拷贝/XDP/容器网络）

### Modified Capabilities

无

## Impact

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `data/learn-deep/db-vector-basic.md` | 已新增 | 样章完成 |
| `data/learn-deep/*.md` | 批量新增 | VDB 14 篇 → DB ~20 篇 → Linux ~62 篇 |
| `data/learn-content-vdb.js` | 新增 | VDB 导学，从 items-registry 派生 |
| `data/learn-content-linux.js` | 新增 | Linux 导学，从 items-registry 派生 |
| `learn.html` | 修改 | +2 script 加载 + 1 section + 1 render 函数 |
| 上下文 | 无风险 | 每个 .md 文件独立，learn.html 只改一次定型 |

**不影响的文件**：items-registry.js / radar-tech.js / quiz-tech.js / kanban-data.js / quiz-system.html / index.html

### 写作顺序

1. VDB（14 篇 + vdb 导学）— 用户最关注，与数据库内核知识直接相连
2. DB（~20 篇核心 + 已有导学完善）— VDB 的上下游知识，自然延续
3. Linux（62 篇 + linux 导学）— 体量最大，放在最后
