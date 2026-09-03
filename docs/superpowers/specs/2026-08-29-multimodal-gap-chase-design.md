# 多模态数据库追赶计划设计

> **目标:** 22 领域全量覆盖，能力持平 90% 以上（API + 性能双达标）
> **技术路线:** 全自研
> **交付节奏:** 按阶段分批交付

## 一、背景与目标

### 1.1 分析结果摘要

`docs/multimodal-gap-analysis-2026/` 的 22 领域差距分析已完成，核心发现：

**三类系统性缺口：**
1. 并发原语缺失 — 5 个模态复刻同一份有缺陷的 rwlock
2. WAL 覆盖不对称 — 4 个模态无 redo log
3. 执行器与存储层契约断裂 — Relational TID 伪造 + MVCC 孤岛

**P0 正确性风险 Top 3：**
| 排名 | 缺陷 | 严重性 |
|------|------|--------|
| 1 | UPDATE/DELETE 构造 TID 硬编码块 0 偏移 24 | 静默数据损坏 |
| 2 | faiss_hnsw add 触发 realloc 与 search 并发 → UAF | 内存安全崩溃 |
| 3 | WAL 写入返回值被忽略 + 无 fsync | 崩溃后数据丢失 |

### 1.2 追赶目标

- **功能 API 覆盖率:** 各模态核心 API 与标杆产品对比达到 90%
- **性能指标:** 召回率、QPS、延迟等达到标杆的 90%
- **正确性:** 消灭所有 P0/P1 正确性风险

### 1.3 技术约束

- **全自研路线** — 不引入外部依赖，全部自主实现
- **串行推进** — 按依赖顺序执行，共性 Gap 先于模态追赶
- **阶段交付** — 4 个 Phase，每 Phase 有明确验收标准

---

## 二、整体架构

```
Phase 1: 共性基础设施（6 个 Gap）
    ├── G1: 并发原语库（rwlock 竞态修复）
    ├── G2: WAL 统一接入 + fsync
    ├── G3: TID 管道修复
    ├── G4: 锁默认开关修正
    ├── G5: 错误路径统一化
    └── G6: 跨模态集成测试基线
    ↓
Phase 2: 8 个模态追赶（按代码质量排序）
    ├── 2.1 Tree/Yang (4.2 → 7+)
    ├── 2.2 KV (4.2 → 7+)
    ├── 2.3 Spatial (4.2 → 7+)
    ├── 2.4 Timeseries (4.2 → 7+)
    ├── 2.5 Document (4.3 → 7+)
    ├── 2.6 Vector (4.3 → 7+)
    ├── 2.7 Graph (4.5 → 7+)
    └── 2.8 Relational (3.8 → 7+)
    ↓
Phase 3: 5 个缺失模态全自研落地（按契合度排序）
    ├── 3.1 多模态 AI 原生（契合度 9）
    ├── 3.2 对象 Blob（契合度 8）
    ├── 3.3 全文搜索（契合度 7）
    ├── 3.4 可观测日志（契合度 6）
    └── 3.5 宽表（契合度 5）
    ↓
Phase 4: 集成测试 + 性能优化
```

---

## 三、Phase 1: 共性基础设施

### 3.1 执行顺序

| 步骤 | 任务 | 影响范围 | 工作量 | 目标 |
|------|------|---------|--------|------|
| 1.1 | 抽取公共并发原语库 | 5 模态 | M | 修复 rwlock 竞态 + 写者饥饿 |
| 1.2 | 关系模态 TID 管道修复 | Relational | M | 消除静默数据损坏 |
| 1.3 | WAL 统一接入 + fsync | 6 模态 | L | 6 模态崩溃安全 |
| 1.4 | 锁默认开关修正 | 5 模态 | S | use_lock=true 默认 |
| 1.5 | 错误路径统一化 | 4 模态 | M | 统一错误码 + 资源回收 |
| 1.6 | 跨模态集成测试基线 | 全部 | M | 回归保护 |

### 3.2 G1: 并发原语库设计

**问题现状:**
- `vector_engine.c:1557`、`ts_engine.c:745-770`、`doc_engine.c:750-770` 复刻同一份有缺陷的 `simple_rwlock_t`
- 缺陷：读者与写者同时持有的竞态窗口、`writers_waiting` 永不检查、`timeout_ms` 被忽略
- `graph_csr.c`、`rtree.c` 直接零锁，更糟

**设计方案:**
```c
// engineering/include/db/common_rwlock.h
typedef struct {
    pthread_rwlock_t rwlock;
    bool            use_lock;
    const char*     name;  // 用于调试
} common_rwlock_t;

// API 设计
common_rwlock_t* common_rwlock_create(const char* name);
void             common_rwlock_destroy(common_rwlock_t* lock);
void             common_rwlock_read_lock(common_rwlock_t* lock);
void             common_rwlock_read_unlock(common_rwlock_t* lock);
void             common_rwlock_write_lock(common_rwlock_t* lock);
void             common_rwlock_write_unlock(common_rwlock_t* lock);
bool             common_rwlock_try_write_lock(common_rwlock_t* lock, int timeout_ms);
```

**替换策略:**
1. 创建 `common_rwlock.h/c`，使用 pthread_rwlock 封装
2. 5 个模态的 `simple_rwlock_t` 引用点逐一替换
3. `use_lock=true` 设为默认，无锁模式通过 `use_lock=false` opt-in

### 3.3 G2: WAL 统一接入设计

**问题现状:**
- 只有 KV 接入共享 WAL
- Vector 有独立 WAL 但缺 fsync
- Relational/Timeseries/Spatial/Tree 完全无 redo log

**设计方案:**
```c
// engineering/include/db/mm_wal.h
typedef enum {
    WAL_SYNC_FULL,     // write + fsync（防系统崩溃）
    WAL_SYNC_BUFFERED, // write + fflush（仅防进程崩溃）
    WAL_SYNC_NONE      // 异步写入
} WalSyncMode;

typedef struct {
    uint64_t    lsn;
    WalSyncMode mode;
    // ...
} WalContext;

// 统一 WAL API
int  wal_append(WalContext* ctx, ModelType model, const void* record, size_t len);
int  wal_flush(WalContext* ctx);
int  wal_replay(WalContext* ctx, void (*callback)(ModelType, const void*, size_t));
```

**接入点:**
1. `mm_storage.c` 作为 WAL 入口，各模态 engine 通过 mm_storage 写 WAL
2. Vector WAL 改造：复用 `mm_storage` 而非独立 `vector_wal.c`
3. 其他模态：在 `insert/update/delete` 路径统一调用 `wal_append`

### 3.4 G3: TID 管道修复

**问题现状:**
- `nodeModifyTable.c` 硬编码 TID（块 0、偏移 24）
- UPDATE/DELETE 会改错行

**设计方案:**
```c
// engineering/src/db/relational/tid_resolver.c
typedef struct {
    BlockNumber block_num;
    OffsetNumber offset;
    ItemPointerData tid;  // 真实 TID
} TIDResolver;

// 正确获取 TID：从 heap tuple 获取真实物理位置
TIDResolver* tid_resolver_from_tuple(HeapTuple tuple);
int          tid_resolver_update(TIDResolver* resolver, const void* new_data);
int          tid_resolver_delete(TIDResolver* resolver);
```

### 3.5 G4-G5: 锁默认 + 错误路径

**G4:** `use_lock=true` 设为各 engine 创建时的默认值

**G5:**
- 统一错误码体系（参考 PostgreSQL `ERRCODE_*`）
- 使用 MemoryContext 统一管理错误路径资源回收

---

## 四、Phase 2: 8 个模态追赶

### 4.1 追赶顺序（按代码质量）

| 顺序 | 模态 | 当前分 | 目标分 | 核心工作 |
|------|------|--------|--------|---------|
| 1 | Tree/Yang | 4.2 | 7+ | XML parser 升级、NETCONF 完整实现、持久化 |
| 2 | KV | 4.2 | 7+ | 并发安全完善、CAS/Watch/Multi、fsync |
| 3 | Spatial | 4.2 | 7+ | R-Tree 加锁、ST_* PostGIS 兼容子集 |
| 4 | Timeseries | 4.2 | 7+ | 锁修复、热路径增量压缩、连续聚合完善 |
| 5 | Document | 4.3 | 7+ | 锁修复、聚合管道完善、同义词增强 |
| 6 | Vector | 4.3 | 7+ | UAF 修复、WAL fsync、HNSW 并发安全 |
| 7 | Graph | 4.5 | 7+ | CSR 加锁、算法库扩充（Dijkstra/PageRank 等完善）、Cypher 测试覆盖 |
| 8 | Relational | 3.8 | 7+ | TID + WAL + MVCC 集成、优化器完善、B+Tree 页式化 |

### 4.2 各模态详细目标

#### Tree/Yang (2.1)
- XML parser 升级：支持属性、命名空间
- datastore 持久化
- YANG/NETCONF 1.1 + SSH transport

#### KV (2.2)
- 完善并发 put 安全（已有共性原语库）
- CAS/Watch/Multi 命令实现
- KV_FULL 错误码 + B+Tree 分裂
- WAL 失败正确处理 + fsync

#### Spatial (2.3)
- R-Tree 读写锁或 RCU
- ST_* PostGIS 兼容函数子集
- 时空索引（支持移动对象）

#### Timeseries (2.4)
- 锁修复（复用共性原语库）
- 热路径增量压缩
- 连续聚合完善
- 压缩算法优化

#### Document (2.5)
- 锁修复（复用共性原语库）
- 聚合管道完善（match/group/sort/limit/skip/project）
- 同义词系统增强
- JSONPath 完整支持

#### Vector (2.6)
- faiss_hnsw UAF 修复（RCU/快照 + search 读锁）
- WAL fsync
- 21+ 向量索引完善
- GPU 加速集成完善

#### Graph (2.7)
- CSR 读写锁或 RCU
- 图算法库扩充（betweenness/closeness/Louvain）
- Cypher/openCypher 核心测试套件
- PageRank 悬挂节点处理

#### Relational (2.8)
- TID 管道修复
- DML 接入共享 WAL
- MVCC 集成到执行路径（SeqScan 可见性 + heap_insert 戳 xmin）
- 优化器 join 顺序动态规划
- B+Tree 页式化 + FSM

---

## 五、Phase 3: 5 个缺失模态

### 5.1 落地顺序（按契合度）

| 顺序 | 模态 | 契合度 | 核心功能 |
|------|------|--------|---------|
| 1 | 多模态 AI 原生 | 9 | NamedVector + 对象内嵌 Blob + 跨模态检索 |
| 2 | 对象 Blob | 8 | Chunk + Manifest + GC + SHA-256 |
| 3 | 全文搜索 | 7 | 自研倒排索引 + TF-IDF/BM25 |
| 4 | 可观测日志 | 6 | 标签倒排 + LogQL + TTL |
| 5 | 宽表 | 5 | Column Family + LSM 树 |

### 5.2 各模态设计概要

#### 多模态 AI 原生 (3.1)
```c
// engineering/include/db/multimodal_ai.h
typedef struct {
    char*       object_id;      // Blob 引用
    float*      embedding;      // 向量
    char*       text;           // 文本
    Metadata    metadata;       // 元数据
    ModelType   primary_type;   // 主模态
} MultimodalRecord;

// 跨模态检索
SearchResult* multimodal_search(
    MultimodalIndex* idx,
    const Query* query,           // 支持多模态混合查询
    SearchOptions* opts
);
```

#### 对象 Blob (3.2)
```c
// engineering/include/db/blob_engine.h
typedef struct {
    uint64_t    chunk_size;     // 固定 4MB
    uint64_t    total_size;
    char        manifest_hash[64];
} BlobObject;

// API
int  blob_upload(BlobEngine* engine, const void* data, size_t len, char** object_id);
int  blob_download(BlobEngine* engine, const char* object_id, void** data, size_t* len);
int  blob_delete(BlobEngine* engine, const char* object_id);
int  blob_gc(BlobEngine* engine);  // 垃圾回收
```

#### 全文搜索 (3.3)
```c
// engineering/include/db/fts_engine.h
typedef struct {
    InvertedIndex* index;
    Analyzer*      analyzer;     // 分词器
    Scorer*        scorer;       // BM25/TF-IDF
} FtsEngine;

// API
int  fts_index_document(FtsEngine* engine, const char* doc_id, const char* content);
SearchResult* fts_search(FtsEngine* engine, const char* query);
```

#### 可观测日志 (3.4)
```c
// engineering/include/db/log_engine.h
typedef struct {
    TagInvertedIndex* tag_index;  // 标签倒排
    LogStore*         store;      // 日志存储
    LogQLParser*      parser;     // LogQL 解析
} LogEngine;

// API
int  log_append(LogEngine* engine, const LogEntry* entry);
SearchResult* log_query(LogEngine* engine, const char* logql);
int  log_delete_old(LogEngine* engine, uint64_t ttl_seconds);
```

#### 宽表 (3.5)
```c
// engineering/include/db/wide_column.h
typedef struct {
    char*       row_key;
    Columns*    columns;         // 列族
    uint64_t    timestamp;
} WideColumnRow;

// API
int  wc_put(WideColumnEngine* engine, const char* row_key, const Columns* cols);
Row* wc_get(WideColumnEngine* engine, const char* row_key);
int  wc_scan(WideColumnEngine* engine, const char* start_key, const char* end_key);
```

---

## 六、Phase 4: 集成与优化

### 6.1 集成测试基线

| 测试场景 | 覆盖模态 | 验证点 |
|---------|---------|--------|
| Vector + Graph + RAG | Vector, Graph, RAG | 跨模态检索 |
| Relational + MVCC + WAL | Relational | 事务一致性 |
| Vector + faiss_hnsw 并发 | Vector | 并发安全 |
| Blob + NamedVector | Blob, Vector | 多模态 AI 原生 |
| 全模态 CRUD | 全部 13 模态 | 基础功能回归 |

### 6.2 性能基准

| 指标 | Vector | Relational | Graph | KV |
|------|--------|-----------|-------|-----|
| 召回率 | vs Milvus | N/A | N/A | N/A |
| QPS | vs Qdrant | vs PostgreSQL | vs Neo4j | vs Redis |
| 延迟 P99 | 目标 90% | 目标 90% | 目标 90% | 目标 90% |

---

## 七、验收标准

### Phase 1 验收
- [ ] 5 个模态的 rwlock 替换为公共库，无竞态窗口
- [ ] Relational UPDATE/DELETE 不再改错行
- [ ] 6 个模态都有 WAL 保护且 fsync 正确
- [ ] 锁默认开启
- [ ] 错误路径统一，无资源泄漏
- [ ] 跨模态集成测试基线可运行

### Phase 2 验收
- [ ] 8 个模态总分 ≥ 7（分析报告评分标准）
- [ ] 核心 API 覆盖率 ≥ 90%
- [ ] 无 P0/P1 正确性风险

### Phase 3 验收
- [ ] 5 个缺失模态全部可跑
- [ ] 与 mm_storage 抽象正确集成
- [ ] 单元测试覆盖 ≥ 80%

### Phase 4 验收
- [ ] 跨模态集成测试全部通过
- [ ] 性能指标达到标杆 90%

---

## 八、错误处理

- 子代理之间结论冲突 → 读代码仲裁
- 代码量超出预期 → 拆分任务
- 性能不达标 → 分析瓶颈，优化或调整目标

---

## 九、非目标

- 不做分布式分片（Phase 4 之后）
- 不做 Raft 一致性（Phase 4 之后）
- 不引入外部依赖（MinIO/Lucene/ClickHouse 等）

---

**设计文档版本:** 1.0
**创建日期:** 2026-08-29
**基于分析报告:** `docs/multimodal-gap-analysis-2026/`
