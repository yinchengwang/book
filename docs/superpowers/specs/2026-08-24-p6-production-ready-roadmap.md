# P6 生产就绪追赶计划

> **目标**：在 6-12 个月内将自研多模态数据库从"功能演示可用"提升到"生产就绪"水平，对标 Milvus 2.4 / Qdrant 1.12 / Weaviate 1.22。
>
> **日期**：2026-08-24
> **状态**：规格草案，待评审

---

## 1. 背景与目标

### 1.1 现状差距

| 维度 | 自研现状 | 目标（Milvus/Qdrant） | 严重程度 |
|------|----------|----------------------|----------|
| 1M+ 规模性能 | 未验证 | Recall@10 ≥ 0.85, QPS ≥ 1000 | 🔴 P0 |
| 分页 API | 无 | offset/skip/cursor 分页 | 🔴 P0 |
| 监控指标 | 无 | Prometheus 端点 | 🟡 P1 |
| 备份/恢复 | 无 | 在线快照 + 恢复 | 🟡 P1 |
| ACID 事务 | 无 | begin/commit/rollback | 🟡 P1 |
| 复制/分片 | 无 | Raft 复制 + 一致性哈希 | 🟡 P1 |
| 多租户 | 无 | namespace 隔离 | 🟢 P2 |
| 通用聚合 | 仅时序 | group by / count / sum / avg | 🟢 P2 |

### 1.2 追赶目标

- **功能完整**：补全生产必需的工程特性（监控、备份、事务、复制）
- **性能达标**：1M 规模 Recall@10 ≥ 0.85, QPS ≥ 1000
- **稳定性保障**：7×24h 压力测试通过
- **文档就绪**：API 文档 + 运维手册 + 性能调优指南

---

## 2. 里程碑规划

| 阶段 | 时间 | 核心目标 | 关键产出 |
|------|------|----------|----------|
| **P6-M1** | 第 1-2 月 | 工程基础补强 | 分页 API + 监控指标 + 1M 性能验证 |
| **P6-M2** | 第 3-4 月 | 数据安全 | 备份/恢复 + ACID 事务 |
| **P6-M3** | 第 5-7 月 | 可扩展性 | 复制/分片 + 多租户 |
| **P6-M4** | 第 8-10 月 | 聚合能力 | 通用聚合框架 + 增强时序聚合 |
| **P6-Final** | 第 11-12 月 | 集成验证 | 全量基准 + 压力测试 + 文档 |

---

## 3. 详细任务规格

### 3.1 P6-M1：工程基础补强

#### M1.1 分页 API（P0）

**问题**：当前 `mmdb_vectors_search()` 无分页支持，客户端无法实现"下一页"。

**API 设计**：

```c
/* mmdb_query_t 新增字段 */
typedef struct {
    // ... 现有字段 ...
    uint32_t    offset;       /* 结果偏移（从 0 开始） */
    uint32_t    limit;        /* 返回最大数量（默认 0 = 无限制） */
    void*       cursor;       /* cursor 分页游标（备选，暂不实现） */
} mmdb_query_t;

/* mmdb_result_t 新增字段 */
typedef struct {
    uint32_t    total_count;  /* 满足条件的总结果数 */
    bool        has_more;     /* 是否还有更多结果 */
    uint32_t    returned;     /* 本次返回的结果数 */
} mmdb_result_t;
```

**实现要求**：
- 向量搜索路径（HNSW/Flat）：skip 已扫描的 top-k 结果
- 过滤搜索路径：在 filter 阶段后应用 offset/limit
- 聚合查询：在 group by 后应用分页
- **向后兼容**：`offset=0, limit=0` 行为与现有完全一致

**验收标准**：
- [ ] `offset=0, limit=10` 返回前 10 条
- [ ] `offset=10, limit=10` 返回第 11-20 条
- [ ] `total_count` 与实际匹配
- [ ] `has_more` 正确指示是否有更多
- [ ] 既有测试无回归（`mmdb_vectors_test` 等）

---

#### M1.2 监控指标（P1）

**问题**：无可观测性，生产环境无法告警和排查问题。

**API 设计**：

```c
/* mmdb_metrics.h */
typedef struct {
    /* 运行时指标 */
    uint64_t    vectors_total;        /* 累计插入向量数 */
    uint64_t    queries_total;        /* 累计查询次数 */
    uint64_t    queries_success;      /* 成功查询数 */
    uint64_t    queries_failed;       /* 失败查询数 */
    double      query_latency_avg_ms; /* 平均延迟 ms */
    double      query_latency_p50_ms; /* P50 延迟 ms */
    double      query_latency_p99_ms; /* P99 延迟 ms */
    uint64_t    cache_hits;           /* 缓存命中数 */
    uint64_t    cache_misses;         /* 缓存未命中数 */
    double      cache_hit_rate;       /* 缓存命中率 */

    /* 资源指标 */
    size_t      memory_used_bytes;    /* 已用内存字节 */
    size_t      memory_total_bytes;   /* 总内存字节 */
    size_t      disk_used_bytes;      /* 已用磁盘字节 */
    size_t      disk_total_bytes;     /* 总磁盘字节 */

    /* HNSW 指标 */
    uint64_t    hnsw_build_total;     /* 累计 HNSW 构建次数 */
    double      hnsw_build_time_ms;   /* 最后一次 HNSW 构建耗时 ms */
} mmdb_metrics_t;

/**
 * @brief 获取全局指标快照
 * @return 指标结构体（内部缓冲区，每次调用更新）
 */
const mmdb_metrics_t* mmdb_metrics_get(void);

/**
 * @brief 重置所有计数器
 */
void mmdb_metrics_reset(void);

/**
 * @brief 暴露 Prometheus 格式指标字符串
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 写入的字节数；buf 不够时返回需要的总大小
 */
size_t mmdb_metrics_prometheus_format(char* buf, size_t buf_size);
```

**实现要求**：
- 指标存储在全局 `mmdb_metrics_t` 结构体中（原子操作保护）
- `mmdb_metrics_prometheus_format()` 输出 Prometheus 标准格式（`# HELP` / `# TYPE` / `mmsdk_vectors_total`）
- 对于 SDK（嵌入式），提供 `mmdb_metrics_prometheus_format()` 供应用层暴露
- 对于服务化场景（未来），提供内置 HTTP `/metrics` 端点（暂不实现，接口预留）

**验收标准**：
- [ ] `mmdb_vectors_search()` 调用后 `queries_total` 递增
- [ ] `mmdb_metrics_get()` 返回的 `query_latency_p99_ms` 与实测 P99 匹配
- [ ] `mmdb_metrics_prometheus_format()` 输出符合 Prometheus 格式
- [ ] `mmdb_metrics_reset()` 后计数器归零

---

#### M1.3 1M 性能验证（P0）

**问题**：1M 规模 Recall@10 和 QPS 未验证，无法评估生产可用性。

**验证目标**：
- Recall@10 ≥ 0.85（1K 子集采样 GT）
- search QPS ≥ 1000（20 queries 平均）
- P99 latency ≤ 20ms

**调优参数**：
| 参数 | 当前值 | 建议范围 | 目标值 |
|------|--------|----------|--------|
| `ef_search` | ? | 100-500 | 200 |
| `ef_construction` | ? | 100-400 | 200 |
| `M` | ? | 16-64 | 32 |
| `n_threads` | 1 | 1-16 | 8 |

**执行计划**：
1. 跑 `staircase_benchmark.exe --gtest_filter=Staircase.VectorKNN1M`
2. 记录 baseline 数据（Recall@10 / QPS / P99）
3. 按上表调参，重跑，验证提升
4. 若 Recall@10 < 0.85，增加 `ef_search`（代价是 QPS 下降）
5. 若 QPS < 1000，增加 `n_threads` + SIMD 覆盖更多距离函数

**验收标准**：
- [ ] `Staircase.VectorKNN1M` Recall@10 ≥ 0.85
- [ ] `Staircase.VectorKNN1M` search QPS ≥ 1000
- [ ] `Staircase.VectorKNN1M` P99 latency ≤ 20ms
- [ ] 性能数据填入 `docs/performance-scale-report.md`

---

### 3.2 P6-M2：数据安全

#### M2.1 备份/恢复（P1）

**问题**：无数据备份，磁盘故障无法恢复。

**API 设计**：

```c
/* mmdb_backup.h */
typedef struct mmdb_backup_s mmdb_backup_t;

typedef enum {
    MMDB_BACKUP_ONLINE,    /* 在线快照，不阻塞读写 */
    MMDB_BACKUP_FULL,      /* 全量备份，阻塞写 */
} mmdb_backup_mode_t;

/**
 * @brief 创建备份
 * @param db    数据库句柄
 * @param path  备份目标路径（目录，会自动创建）
 * @param mode  备份模式
 * @return 备份句柄；失败返回 NULL
 */
mmdb_backup_t* mmdb_backup_create(mmdb_t* db, const char* path, mmdb_backup_mode_t mode);

/**
 * @brief 获取备份进度（0.0 - 1.0）
 */
double mmdb_backup_progress(mmdb_backup_t* backup);

/**
 * @brief 等待备份完成
 */
int mmdb_backup_wait(mmdb_backup_t* backup);

/**
 * @brief 释放备份句柄
 */
void mmdb_backup_free(mmdb_backup_t* backup);

/**
 * @brief 从备份恢复
 * @param db        数据库句柄（必须已关闭）
 * @param backup_path 备份路径
 * @return MMDB_OK 成功
 */
int mmdb_backup_restore(mmdb_t* db, const char* backup_path);

/**
 * @brief 列出可用备份
 * @param db        数据库句柄
 * @param backups   输出数组（调用方分配，容量为 n）
 * @param n         输入：数组容量；输出：实际备份数
 * @return MMDB_OK 成功
 */
int mmdb_backup_list(mmdb_t* db, const char** backups, size_t* n);
```

**实现方案**：
- **在线快照**：基于 SQLite WAL 的 checkpoint + 文件系统快照（`CopyFile` 或 `sendfile`）
- **恢复**：关闭数据库，替换数据文件，重新打开
- **备份元数据**：`backup_path/metadata.json` 记录备份时间、版本、集合列表

**验收标准**：
- [ ] 在线备份不阻塞读写（QPS 衰减 < 10%）
- [ ] 备份可恢复，数据完整性 100%
- [ ] `mmdb_backup_list()` 返回所有可用备份
- [ ] 备份文件大小合理（压缩）

---

#### M2.2 ACID 事务（P1）

**问题**：无显式事务，并发写可能导致数据不一致。

**API 设计**：

```c
/* mmdb_transaction.h */
typedef struct mmdb_txn_s mmdb_txn_t;

typedef enum {
    MMDB_TXN_READONLY,   /* 只读事务 */
    MMDB_TXN_READWRITE,  /* 读写事务 */
} mmdb_txn_mode_t;

typedef enum {
    MMDB_TXN_OK,         /* 成功提交 */
    MMDB_TXN_ABORTED,    /* 被回滚 */
    MMDB_TXN_COMMITTED,  /* 已提交 */
    MMDB_TXN_INVALID,    /* 事务无效 */
} mmdb_txn_status_t;

/**
 * @brief 开始事务
 * @param db    数据库句柄
 * @param mode  事务模式
 * @return 事务句柄；失败返回 NULL
 */
mmdb_txn_t* mmdb_txn_begin(mmdb_t* db, mmdb_txn_mode_t mode);

/**
 * @brief 提交事务
 * @param txn   事务句柄
 * @return 提交状态
 */
mmdb_txn_status_t mmdb_txn_commit(mmdb_txn_t* txn);

/**
 * @brief 回滚事务
 */
void mmdb_txn_abort(mmdb_txn_t* txn);

/**
 * @brief 获取事务状态
 */
mmdb_txn_status_t mmdb_txn_status(mmdb_txn_t* txn);

/**
 * @brief 释放事务句柄（若未提交则自动回滚）
 */
void mmdb_txn_free(mmdb_txn_t* txn);
```

**实现方案**：
- 基于 SQLite 的 `BEGIN TRANSACTION` / `COMMIT` / `ROLLBACK`
- `mmdb_collection_t` 关联 `mmdb_txn_t*`（当前事务）
- 读写事务获取写锁（`mmdb_collection_s.lock` 升级为写锁）
- 只读事务获取读锁（可并发）
- MVCC 预留接口（暂不实现，基于版本链的快照读）

**验收标准**：
- [ ] 并发读写不产生死锁
- [ ] 事务回滚后数据恢复原状
- [ ] 只读事务可并发执行
- [ ] 嵌套事务返回 `MMDB_ERR_INVALID`

---

### 3.3 P6-M3：可扩展性

#### M3.1 复制（P1）

**问题**：单机部署，无高可用。

**架构设计**：

```
┌─────────────┐       Raft       ┌─────────────┐
│   Leader    │ ←──────────────→ │  Follower1  │
│  (读写)     │                  │  (只读/备)  │
└─────────────┘                  └─────────────┘
       ↑
       │ 异步复制
       ↓
┌─────────────┐
│  Follower2  │
│  (只读/备)  │
└─────────────┘
```

**API 设计**：

```c
/* mmdb_replication.h */
typedef struct mmdb_replica_s mmdb_replica_t;

typedef enum {
    MMDB_REPLICA_LEADER,    /* 主节点 */
    MMDB_REPLICA_FOLLOWER,  /* 从节点 */
    MMDB_REPLICA_CANDIDATE, /* 候选节点 */
} mmdb_replica_role_t;

typedef struct {
    mmdb_replica_role_t  role;           /* 当前角色 */
    const char*          leader_addr;    /* 主节点地址（仅 follower） */
    uint64_t             commit_index;   /* 已提交日志索引 */
    uint64_t             applied_index;  /* 已应用日志索引 */
    bool                 is_synced;      /* 是否已同步 */
} mmdb_replica_info_t;

/**
 * @brief 初始化复制模式
 * @param db    数据库句柄
 * @param role  节点角色
 * @param peers JSON 格式的集群节点列表
 * @return MMDB_OK 成功
 */
int mmdb_replication_init(mmdb_t* db, mmdb_replica_role_t role, const char* peers);

/**
 * @brief 获取复制状态
 */
int mmdb_replication_info(mmdb_t* db, mmdb_replica_info_t* info);

/**
 * @brief 触发故障转移（仅 follower 调用）
 */
int mmdb_replication_failover(mmdb_t* db);
```

**实现方案**：
- 参考 `engineering/src/db/raft/` 已有占位实现
- Raft 日志：WAL 条目作为日志条目，checkpoint 作为快照
- 复制协议：gRPC / libuv TCP
- 读一致性：follower 可提供只读查询（可能返回过期数据）

**验收标准**：
- [ ] Leader 故障后 30s 内自动选出新 Leader
- [ ] 写操作在 Follower 上最终一致（延迟 < 1s）
- [ ] 网络分区恢复后自动合并
- [ ] `mmdb_replication_info()` 显示正确的角色和状态

---

#### M3.2 分片（P1）

**问题**：单机容量有上限，无法水平扩展。

**架构设计**：

```
Client
   │
   ▼
┌──────────────────────┐
│   Router (路由层)     │
│  一致性哈希分片路由   │
└──────────────────────┘
   │              │
   ▼              ▼
┌────────┐    ┌────────┐
│Shard 1 │    │Shard 2 │
│(节点A) │    │(节点B) │
└────────┘    └────────┘
```

**API 设计**：

```c
/* mmdb_sharding.h */
typedef struct mmdb_shard_s mmdb_shard_t;

/**
 * @brief 初始化分片集群
 * @param shards JSON 格式的分片节点列表
 * @return MMDB_OK 成功
 */
int mmdb_sharding_init(mmdb_t* db, const char* shards);

/**
 * @brief 手动迁移分片（负载均衡用）
 */
int mmdb_sharding_move(mmdb_t* db, const char* from_shard, const char* to_shard);

/**
 * @brief 获取分片分布信息
 */
int mmdb_sharding_stats(mmdb_t* db, char* json_out, size_t json_size);
```

**实现方案**：
- 一致性哈希：基于 `std::hash<string>` 环
- 分片键：`collection_name` 或用户指定字段
- 路由：客户端 SDK 或服务端 Router 层
- 跨分片查询：广播到所有分片，RRF 融合结果

**验收标准**：
- [ ] 数据均匀分布在各分片（偏差 < 20%）
- [ ] 单分片故障不影响其他分片
- [ ] 跨分片查询返回正确结果
- [ ] 分片迁移期间服务不中断

---

#### M3.3 多租户（P1）

**问题**：无租户隔离，资源共享。

**API 设计**：

```c/* mmdb_namespace.h */
typedef struct mmdb_namespace_s mmdb_namespace_t;

/**
 * @brief 创建命名空间
 * @param name     命名空间名称
 * @param quota    资源配额 JSON（如 {"vectors_max": 10000000, "disk_max_gb": 100}）
 * @return 命名空间句柄
 */
mmdb_namespace_t* mmdb_namespace_create(mmdb_t* db, const char* name, const char* quota);

/**
 * @brief 获取命名空间
 */
mmdb_namespace_t* mmdb_namespace_get(mmdb_t* db, const char* name);

/**
 * @brief 设置命名空间资源配额
 */
int mmdb_namespace_set_quota(mmdb_namespace_t* ns, const char* quota);

/**
 * @brief 获取命名空间使用量
 */
int mmdb_namespace_usage(mmdb_namespace_t* ns, char* json_out, size_t json_size);

/**
 * @brief 删除命名空间（级联删除所有 collection）
 */
int mmdb_namespace_drop(mmdb_namespace_t* ns);
```

**实现方案**：
- 命名空间隔离：每个 namespace 有独立的 collection 集合
- 资源配额：在 `mmdb_namespace_s` 中存储限制值，insert/search 时检查
- 配额检查：向量数量、磁盘使用量、内存使用量

**验收标准**：
- [ ] 命名空间 A 无法访问命名空间 B 的数据
- [ ] 配额超限后 insert 返回 `MMDB_ERR_QUOTA_EXCEEDED`
- [ ] 删除命名空间后所有关联数据被清理

---

### 3.4 P6-M4：聚合能力

#### M4.1 通用聚合框架（P2）

**问题**：仅有时序聚合，通用场景缺失。

**API 设计**：

```c
/* mmdb_aggregate.h */
typedef enum {
    MMDB_AGG_COUNT,
    MMDB_AGG_SUM,
    MMDB_AGG_AVG,
    MMDB_AGG_MIN,
    MMDB_AGG_MAX,
    MMDB_AGG_HISTOGRAM,    /* 直方图（需指定 bucket 数） */
} mmdb_agg_type_t;

typedef struct {
    const char*     field;          /* 聚合字段（metadata 字段名） */
    mmdb_agg_type_t type;           /* 聚合类型 */
    const char*     alias;          /* 输出别名 */
    /* 仅 HISTOGRAM */
    uint32_t        bucket_count;   /* bucket 数量 */
    double          bucket_min;     /* bucket 下界 */
    double          bucket_max;     /* bucket 上界 */
} mmdb_agg_expr_t;

typedef struct {
    const char*     group_by;       /* 分组字段（metadata 字段名，为 NULL 则全局聚合） */
    mmdb_agg_expr_t aggs[8];        /* 聚合表达式（最多 8 个） */
    size_t          agg_count;      /* 实际聚合表达式数 */
    /* 分页 */
    uint32_t        offset;
    uint32_t        limit;
} mmdb_aggregate_query_t;

typedef struct {
    char            key[256];       /* group_by 值（全局聚合时为空字符串） */
    uint64_t        count;          /* 该组数量 */
    double          sum;            /* SUM 结果 */
    double          avg;            /* AVG 结果 */
    double          min;            /* MIN 结果 */
    double          max;            /* MAX 结果 */
    /* HISTOGRAM */
    uint32_t*       histogram_buckets;
    uint32_t        histogram_bucket_count;
} mmdb_aggregate_result_t;

typedef struct {
    mmdb_aggregate_result_t* groups;    /* 分组结果数组 */
    size_t                  group_count;/* 分组数 */
    uint64_t                total_count;/* 总匹配数 */
    bool                    has_more;    /* 是否有更多分组 */
} mmdb_aggregate_result_set_t;

/**
 * @brief 执行聚合查询
 * @param c         collection 句柄
 * @param query     聚合查询
 * @param filter    过滤条件（JSON，NULL 表示不过滤）
 * @param result    输出结果
 * @return MMDB_OK 成功
 */
int mmdb_aggregate(mmdb_collection_t* c, const mmdb_aggregate_query_t* query,
                   const char* filter, mmdb_aggregate_result_set_t** result);

/**
 * @brief 释放聚合结果
 */
void mmdb_aggregate_result_free(mmdb_aggregate_result_set_t* result);
```

**实现方案**：
- 基于 SQLite 的 `GROUP BY` + 聚合函数（`COUNT` / `SUM` / `AVG` / `MIN` / `MAX`）
- HISTOGRAM：手动计算 bucket 边界
- 过滤：通过 `filter_parser` 构建 WHERE 子句

**验收标准**：
- [ ] `COUNT(*)` 返回正确数量
- [ ] `GROUP BY category, COUNT(*)` 按分组正确计数
- [ ] `AVG(price), MIN(price), MAX(price)` 计算正确
- [ ] `HISTOGRAM(price, 10)` 返回 10 个 bucket 的计数

---

#### M4.2 增强时序聚合（P2）

**问题**：当前时序仅支持简单聚合，缺少滑动窗口等高级特性。

**API 扩展**：

```c
/* mmdb_timeseries.h 扩展 */
typedef struct {
    const char*     field;          /* 聚合字段 */
    mmdb_agg_type_t type;           /* 聚合类型 */
    uint64_t        window_ms;      /* 窗口大小（毫秒） */
    uint64_t        slide_ms;       /* 滑动步长（毫秒，0 = 不滑动） */
} mmdb_ts_agg_expr_t;

typedef struct {
    uint64_t        start_time;     /* 查询起始时间 */
    uint64_t        end_time;       /* 查询结束时间 */
    mmdb_ts_agg_expr_t aggs[4];     /* 聚合表达式（最多 4 个） */
    size_t          agg_count;
    bool            fill_empty;     /* 空窗口是否补零 */
} mmdb_ts_aggregate_query_t;

/**
 * @brief 时序聚合查询（滑动窗口）
 */
int mmdb_ts_aggregate(mmdb_collection_t* c, const mmdb_ts_aggregate_query_t* query,
                      mmdb_aggregate_result_set_t** result);
```

**验收标准**：
- [ ] `window_ms=60000, slide_ms=10000` 生成每 10s 一个数据点
- [ ] 空窗口按 `fill_empty` 决定是否补零
- [ ] 滑动窗口不丢数据

---

## 4. 依赖关系

```
M1.1 分页 API
    ├─→ M1.3 性能验证（分页影响 QPS 测量）
    └─→ M4.1 通用聚合（依赖 offset 语义）
            │
            └─→ M2.2 ACID 事务（聚合需要事务隔离）
                    │
                    └─→ M3.1 复制（事务后才能做 Raft）
                            │
                            └─→ M3.2 分片
                                    │
                                    └─→ M3.3 多租户
```

---

## 5. 验收标准总表

| 任务 | 验收条件 | 优先级 |
|------|----------|--------|
| M1.1 分页 | offset/limit/total_count/has_more 正确 | P0 |
| M1.2 监控 | Prometheus 格式输出 + 指标准确 | P1 |
| M1.3 1M 性能 | Recall@10 ≥ 0.85, QPS ≥ 1000 | P0 |
| M2.1 备份 | 在线备份不阻塞 + 可恢复 | P1 |
| M2.2 事务 | 并发安全 + 回滚正确 | P1 |
| M3.1 复制 | 30s 故障转移 + 最终一致 | P1 |
| M3.2 分片 | 均匀分布 + 跨分片查询正确 | P1 |
| M3.3 多租户 | 隔离 + 配额限制 | P2 |
| M4.1 聚合 | GROUP BY / COUNT / AVG 等正确 | P2 |
| M4.2 时序增强 | 滑动窗口 + fill_empty 正确 | P2 |

---

## 6. 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 工期超期（6-12 月） | 中 | 高 | 季度 Checkpoint Review，动态调整 |
| 1M 性能不达标 | 中 | 中 | M1.3 先跑基准，根据结果决策 |
| 复制/分片复杂度高 | 高 | 高 | 参考 `docs/architecture/db/distributed/` 设计 |
| MVCC 实现困难 | 低 | 高 | M2.2 暂不实现 MVCC，只做基础事务 |

---

## 7. 后续计划

P6 完成后，评估是否进入：

- **P7 SQL 兼容性深度**：完整 SQL 92 支持、JDBC/ODBC 驱动
- **P8 云原生**：Kubernetes Operator、Helm Chart、云端托管服务
- **P9 生态集成**：LangChain/LlamaIndex 插件、OpenAI 插件市场
