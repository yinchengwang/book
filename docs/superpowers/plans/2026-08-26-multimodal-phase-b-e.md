# Multimodal Expansion Phase B-E: Implementation Plan

> Phase B: New Models | Phase C: Composite Capabilities | Phase D: SQL Dialects | Phase E: Distributed

## 任务清单

### Phase B: 新模型落地
- [ ] Task B1: MODEL_STREAM 流式存储引擎接口定义
- [ ] Task B2: MODEL_STREAM 核心实现（写入/消费/窗口）
- [ ] Task B3: MODEL_COLUMNAR 列存引擎接口定义
- [ ] Task B4: MODEL_COLUMNAR 核心实现（列式存储/向量化）

### Phase C: 组合能力补齐
- [ ] Task C1: 时空组合能力（ST_* 函数 + 时序索引）
- [ ] Task C2: 知识图谱/RDF（Triple Store + SPARQL 子集）
- [ ] Task C3: 稀疏向量支持（SparseVector + BM25混合检索）

### Phase D: SQL 方言补齐
- [ ] Task D1: PostgreSQL 兼容层
- [ ] Task D2: MySQL 兼容层
- [ ] Task D3: 分析 SQL 方言（DuckDB/ClickHouse 风格）

### Phase E: 分布式能力
- [ ] Task E1: Raft 共识协议
- [ ] Task E2: 分片路由层
- [ ] Task E3: 分布式查询执行

---

## Phase B 详细设计

### MODEL_STREAM 接口

```c
// 头文件: engineering/include/db/stream_engine.h

typedef struct stream_ops_s {
    const char *name;
    DataModel model;  // MODEL_STREAM

    /* 生命周期 */
    int (*init)(const char *data_dir);
    int (*shutdown)(void);

    /* Stream 操作 */
    int (*stream_create)(const char *name, const stream_config_t *config);
    void *(*stream_open)(const char *name);
    int (*stream_close)(void *stream);
    int (*stream_drop)(const char *name);

    /* 生产者操作 */
    int (*produce)(void *stream, const void *data, size_t len);
    int64_t (*get_offset)(void *stream);

    /* 消费者操作 */
    int (*subscribe)(void *stream, int64_t offset, stream_consumer_t *consumer);
    int (*consume)(stream_consumer_t *consumer, void *out_data, size_t *out_len);
    int (*commit_offset)(stream_consumer_t *consumer, int64_t offset);

    /* 窗口操作 */
    int (*window_agg)(void *stream, const char *window_def, void *out);
} stream_ops_t;

const storage_ops_t *stream_engine_get_ops(void);
```

### MODEL_COLUMNAR 接口

```c
// 头文件: engineering/include/db/columnar_engine.h

typedef struct columnar_ops_s {
    const char *name;
    DataModel model;  // MODEL_COLUMNAR

    /* 生命周期 */
    int (*init)(const char *data_dir);
    int (*shutdown)(void);

    /* 表操作 */
    int (*table_create)(const char *name, const columnar_schema_t *schema);
    void *(*table_open)(const char *name, AccessMode mode);
    int (*table_close)(void *table);
    int (*table_drop)(const char *name);

    /* 列操作 */
    int (*column_append)(void *table, const char *col_name, const void *data, size_t len);
    int (*column_get)(void *table, const char *col_name, int64_t row_id, void *out);

    /* 向量化聚合 */
    int (*agg_sum)(void *table, const char *col_name, int64_t *result);
    int (*agg_avg)(void *table, const char *col_name, double *result);
    int (*agg_count)(void *table, int64_t *result);

    /* 压缩 */
    int (*compress)(void *table, compression_type_t type);
    int (*get_stats)(const char *name, storage_stats_t *stats);
} columnar_ops_t;

const storage_ops_t *columnar_engine_get_ops(void);
```

---

## Phase C 详细设计

### 时空组合

- 扩展 Spatial 引擎支持时间维度
- 新增 `ST_WithTime` 函数
- 时空联合索引

### RDF 知识图谱

- Triple Store 实现（Subject-Predicate-Object）
- RDF 索引
- SPARQL 子集解析

### 稀疏向量

- SparseVector 类型定义
- 与 Dense 向量混合检索
- BM25 + 向量混合排序

---

## Phase D 详细设计

### SQL 方言兼容

- `func_registry` 按方言注册函数
- 方言模式开关（PG/MySQL/Analytics）
- 函数别名映射

---

## Phase E 详细设计

### 分布式架构

- Raft Leader 选举
- Hash/Range 分片
- Scatter-Gather 查询执行

