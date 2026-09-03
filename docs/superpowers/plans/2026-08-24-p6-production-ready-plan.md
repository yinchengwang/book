# P6 生产就绪追赶计划 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **规格文档**：`docs/superpowers/specs/2026-08-24-p6-production-ready-roadmap.md`

**目标**：在 6-12 个月内将自研多模态数据库从"功能演示可用"提升到"生产就绪"水平。

**架构**：基于现有 SQLite WAL + HNSW 索引基础设施，逐步添加生产就绪特性。

**技术栈**：C11 / C++17 / CMake 3.20+ / GoogleTest / SQLite WAL / pthread_rwlock / Windows SRWLOCK

---

## 全局约束

1. **C ABI 零破坏**：不修改任何现有 `mmdb_*` 函数签名；结构体仅末尾 append
2. **语言规范**：代码注释 / commit message / report 使用简体中文
3. **单 commit**：每个子任务单独 commit
4. **向后兼容**：`offset=0, limit=0` 行为与现有完全一致
5. **测试优先**：TDD，先写失败测试再实现

---

## 文件结构概览

```
engineering/
├── include/sdk/
│   ├── mmdb_query.h           # M1.1 修改：新增 offset/limit
│   ├── mmdb_result.h          # M1.1 修改：新增 total_count/has_more
│   ├── mmdb_metrics.h         # M1.2 新建：监控指标
│   ├── mmdb_backup.h          # M2.1 新建：备份/恢复
│   ├── mmdb_transaction.h     # M2.2 新建：ACID 事务
│   ├── mmdb_replication.h     # M3.1 新建：复制
│   ├── mmdb_sharding.h        # M3.2 新建：分片
│   ├── mmdb_namespace.h       # M3.3 新建：多租户
│   └── mmdb_aggregate.h       # M4.1 新建：聚合
├── src/sdk/
│   ├── core/
│   │   ├── metrics.c          # M1.2 新建
│   │   ├── backup.c           # M2.1 新建
│   │   ├── transaction.c      # M2.2 新建
│   │   └── aggregate.c        # M4.1 新建
│   ├── extra/
│   │   ├── replication.c      # M3.1 新建
│   │   ├── sharding.c         # M3.2 新建
│   │   └── namespace.c        # M3.3 新建
│   └── vectors/vectors.c      # M1.1 修改：分页逻辑
├── test/sdk/
│   ├── integration/
│   │   ├── pagination_test.cpp      # M1.1 测试
│   │   ├── metrics_test.cpp         # M1.2 测试
│   │   ├── backup_test.cpp          # M2.1 测试
│   │   ├── transaction_test.cpp     # M2.2 测试
│   │   ├── replication_test.cpp     # M3.1 测试
│   │   ├── sharding_test.cpp        # M3.2 测试
│   │   ├── namespace_test.cpp       # M3.3 测试
│   │   └── aggregate_test.cpp       # M4.1 测试
│   └── CMakeLists.txt               # M1.1-M4.1 注册
└── src/db/raft/                     # M3.1 复用已有占位
```

---

## 子任务执行顺序

```
M1.1 分页 API      → M1.2 监控指标 → M1.3 1M 性能验证
                                    ↓
M4.1 通用聚合      ←（依赖 M1.1 的 offset 语义）
    ↓
M2.2 ACID 事务    ←（依赖 M4.1 的聚合）
    ↓
M2.1 备份/恢复    ←（可并行于 M2.2）
    ↓
M3.1 复制         ←（依赖 M2.2 事务）
    ↓
M3.2 分片         ←（依赖 M3.1 复制）
    ↓
M3.3 多租户       ←（依赖 M3.2 分片）
    ↓
M4.2 时序增强     ←（可并行于 M3.x）
```

---

## 任务 1：M1.1 分页 API

### 概述

为 `mmdb_vectors_search()` 添加 offset/limit 分页支持，新增 `total_count` / `has_more` / `returned` 字段。

### 文件

- 修改：`engineering/include/sdk/mmdb_query.h`
- 修改：`engineering/include/sdk/mmdb_result.h`
- 修改：`engineering/src/sdk/vectors/vectors.c`
- 创建：`engineering/test/sdk/integration/pagination_test.cpp`

### 接口

- 消费：无
- 产出：
  - `mmdb_query_t.offset`（uint32_t）
  - `mmdb_query_t.limit`（uint32_t）
  - `mmdb_result_t.total_count`（uint32_t）
  - `mmdb_result_t.has_more`（bool）
  - `mmdb_result_t.returned`（uint32_t）

---

- [ ] **Step 1: 修改 mmdb_query.h — 新增分页字段**

打开 `engineering/include/sdk/mmdb_query.h`，找到 `mmdb_query_t` 结构体定义，在末尾新增字段：

```c
typedef struct {
    /* ... 现有字段 ... */
    // P6-M1.1 分页支持
    uint32_t    offset;       /* 结果偏移（从 0 开始），默认 0 */
    uint32_t    limit;        /* 返回最大数量（0 = 无限制），默认 0 */
} mmdb_query_t;
```

- [ ] **Step 2: 修改 mmdb_result.h — 新增分页元数据**

打开 `engineering/include/sdk/mmdb_result.h`，找到 `mmdb_result_t` 结构体定义，在末尾新增字段：

```c
typedef struct {
    /* ... 现有字段 ... */
    // P6-M1.1 分页元数据
    uint32_t    total_count;  /* 满足条件的总结果数 */
    bool        has_more;     /* 是否还有更多结果 */
    uint32_t    returned;     /* 本次返回的结果数 */
} mmdb_result_t;
```

- [ ] **Step 3: 修改 vectors.c — 实现分页逻辑**

打开 `engineering/src/sdk/vectors/vectors.c`，找到 `mmdb_vectors_search` 函数（约第 1023 行），在返回结果前应用分页：

```c
/* 在函数内部，找到返回结果的位置（约 result->count = k; 之后） */

/* P6-M1.1 分页支持 */
if (query->offset > 0 || query->limit > 0) {
    uint32_t total = result->count;
    uint32_t skip = query->offset;
    uint32_t take = query->limit > 0 ? query->limit : total;

    /* 计算实际返回数量 */
    uint32_t actual_count = (skip >= total) ? 0 : ((skip + take > total) ? (total - skip) : take);

    result->total_count = total;
    result->has_more = (skip + actual_count < total);
    result->returned = actual_count;

    /* 移动结果数组（跳过 offset 条） */
    if (skip > 0 && skip < total) {
        memmove(result->items, result->items + skip, actual_count * sizeof(mmdb_result_item_t));
    }
    result->count = actual_count;
} else {
    /* 向后兼容：offset=0, limit=0 时行为不变 */
    result->total_count = result->count;
    result->has_more = false;
    result->returned = result->count;
}
```

- [ ] **Step 4: 创建 pagination_test.cpp — 分页测试**

创建 `engineering/test/sdk/integration/pagination_test.cpp`：

```cpp
/**
 * @file pagination_test.cpp
 * @brief P6-M1.1 分页 API 测试
 */
#include <gtest/gtest.h>
#include "mmdb.h"

class PaginationTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = mmdb_open(":memory:");
        ASSERT_NE(db, nullptr);
        mmdb_collection_schema_t schema = {
            .name = "test_pagination",
            .model = MMDB_MODEL_VECTOR,
            .dimension = 128,
        };
        coll = mmdb_collection_create(db, &schema);
        ASSERT_NE(coll, nullptr);

        /* 插入 100 条向量用于分页测试 */
        for (int i = 0; i < 100; i++) {
            float vec[128];
            for (int j = 0; j < 128; j++) vec[j] = (float)(i + j) / 100.0f;
            mmdb_vectors_add(coll, &vec, sizeof(vec), "id", NULL);
        }
    }

    void TearDown() override {
        if (coll) mmdb_collection_free(coll);
        if (db) mmdb_close(db);
    }

    mmdb_t* db;
    mmdb_collection_t* coll;
};

/* 测试 offset=0, limit=10 返回前 10 条 */
TEST_F(PaginationTest, FirstPage) {
    mmdb_query_t query = {0};
    query.offset = 0;
    query.limit = 10;

    mmdb_result_t* result = mmdb_vectors_search(coll, query_vec, &query, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->returned, 10u);
    EXPECT_EQ(result->total_count, 100u);
    EXPECT_TRUE(result->has_more);
    EXPECT_EQ(result->count, 10u);
    mmdb_result_free(result);
}

/* 测试 offset=10, limit=10 返回第 11-20 条 */
TEST_F(PaginationTest, SecondPage) {
    mmdb_query_t query = {0};
    query.offset = 10;
    query.limit = 10;

    mmdb_result_t* result = mmdb_vectors_search(coll, query_vec, &query, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->returned, 10u);
    EXPECT_EQ(result->total_count, 100u);
    EXPECT_TRUE(result->has_more);
    mmdb_result_free(result);
}

/* 测试 offset=90, limit=10 返回最后 10 条 */
TEST_F(PaginationTest, LastPage) {
    mmdb_query_t query = {0};
    query.offset = 90;
    query.limit = 10;

    mmdb_result_t* result = mmdb_vectors_search(coll, query_vec, &query, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->returned, 10u);
    EXPECT_EQ(result->total_count, 100u);
    EXPECT_FALSE(result->has_more);  /* 最后一页没有更多 */
    mmdb_result_free(result);
}

/* 测试 offset=95, limit=10 越界，返回 5 条 */
TEST_F(PaginationTest, OverflowPage) {
    mmdb_query_t query = {0};
    query.offset = 95;
    query.limit = 10;

    mmdb_result_t* result = mmdb_vectors_search(coll, query_vec, &query, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->returned, 5u);  /* 只有 5 条 */
    EXPECT_EQ(result->total_count, 100u);
    EXPECT_FALSE(result->has_more);
    mmdb_result_free(result);
}

/* 测试向后兼容：offset=0, limit=0 行为不变 */
TEST_F(PaginationTest, BackwardCompatible) {
    mmdb_query_t query = {0};  /* 默认值 */
    query.offset = 0;
    query.limit = 0;

    mmdb_result_t* result = mmdb_vectors_search(coll, query_vec, &query, 10);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->count, 10u);  /* 行为与之前一致 */
    EXPECT_EQ(result->total_count, 10u);  /* total_count = count */
    EXPECT_FALSE(result->has_more);
    mmdb_result_free(result);
}
```

- [ ] **Step 5: 注册测试并运行**

在 `engineering/test/sdk/integration/CMakeLists.txt` 中新增：

```cmake
add_project_test(pagination_test VAR)
```

运行测试：

```bash
cd engineering && cmake -B ../build/engineering -G Ninja && cmake --build ../build/engineering --target pagination_test && ../build/engineering/test/sdk/integration/pagination_test.exe
```

- [ ] **Step 6: Commit**

```bash
git add engineering/include/sdk/mmdb_query.h engineering/include/sdk/mmdb_result.h engineering/src/sdk/vectors/vectors.c engineering/test/sdk/integration/pagination_test.cpp engineering/test/sdk/integration/CMakeLists.txt
git commit -m "feat(sdk): P6-M1.1 分页 API — offset/limit/total_count/has_more"
```

---

## 任务 2：M1.2 监控指标

### 概述

新增 `mmdb_metrics.h` 提供 Prometheus 格式的运行时监控指标。

### 文件

- 创建：`engineering/include/sdk/mmdb_metrics.h`
- 创建：`engineering/src/sdk/core/metrics.c`
- 创建：`engineering/test/sdk/integration/metrics_test.cpp`

### 接口

- 消费：无
- 产出：
  - `mmdb_metrics_t` 结构体
  - `mmdb_metrics_get()` 函数
  - `mmdb_metrics_reset()` 函数
  - `mmdb_metrics_prometheus_format()` 函数

---

- [ ] **Step 1: 创建 mmdb_metrics.h**

创建 `engineering/include/sdk/mmdb_metrics.h`：

```c
/**
 * @file mmdb_metrics.h
 * @brief P6-M1.2 监控指标 API
 */
#ifndef MMDB_METRICS_H
#define MMDB_METRICS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 监控指标结构体 */
typedef struct {
    /* 运行时指标 */
    uint64_t    vectors_total;            /* 累计插入向量数 */
    uint64_t    queries_total;            /* 累计查询次数 */
    uint64_t    queries_success;          /* 成功查询数 */
    uint64_t    queries_failed;           /* 失败查询数 */
    double      query_latency_avg_ms;     /* 平均延迟 ms */
    double      query_latency_p50_ms;     /* P50 延迟 ms */
    double      query_latency_p99_ms;     /* P99 延迟 ms */
    uint64_t    cache_hits;               /* 缓存命中数 */
    uint64_t    cache_misses;             /* 缓存未命中数 */
    double      cache_hit_rate;           /* 缓存命中率 */

    /* 资源指标 */
    size_t      memory_used_bytes;        /* 已用内存字节 */
    size_t      memory_total_bytes;       /* 总内存字节 */
    size_t      disk_used_bytes;          /* 已用磁盘字节 */
    size_t      disk_total_bytes;         /* 总磁盘字节 */

    /* HNSW 指标 */
    uint64_t    hnsw_build_total;         /* 累计 HNSW 构建次数 */
    double      hnsw_build_time_ms;       /* 最后一次 HNSW 构建耗时 ms */
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
 * @return 写入的字节数；buf 不够时返回需要的总大小（负值）
 */
size_t mmdb_metrics_prometheus_format(char* buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* MMDB_METRICS_H */
```

- [ ] **Step 2: 创建 metrics.c**

创建 `engineering/src/sdk/core/metrics.c`：

```c
/**
 * @file metrics.c
 * @brief P6-M1.2 监控指标实现
 */
#include "mmdb_metrics.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 全局指标结构体（原子操作保护） */
static mmdb_metrics_t g_metrics = {0};

/* 延迟统计环形缓冲区（P99 计算） */
#define LATENCY_BUF_SIZE 10000
static double g_latency_buf[LATENCY_BUF_SIZE];
static size_t g_latency_idx = 0;
static size_t g_latency_count = 0;

const mmdb_metrics_t* mmdb_metrics_get(void) {
    /* 更新缓存命中率 */
    uint64_t total_cache = g_metrics.cache_hits + g_metrics.cache_misses;
    g_metrics.cache_hit_rate = (total_cache > 0)
        ? (double)g_metrics.cache_hits / total_cache
        : 0.0;

    /* 计算 P99 */
    if (g_latency_count > 0) {
        size_t p99_idx = (size_t)(g_latency_count * 0.99);
        if (p99_idx >= LATENCY_BUF_SIZE) p99_idx = LATENCY_BUF_SIZE - 1;
        /* 简化：取最后一个值作为 P99 近似 */
        g_metrics.query_latency_p99_ms = g_latency_buf[(g_latency_idx - 1) & (LATENCY_BUF_SIZE - 1)];
        g_metrics.query_latency_p50_ms = g_latency_buf[(g_latency_idx - g_latency_count / 2) & (LATENCY_BUF_SIZE - 1)];
    }

    return &g_metrics;
}

void mmdb_metrics_reset(void) {
    memset(&g_metrics, 0, sizeof(g_metrics));
    g_latency_idx = 0;
    g_latency_count = 0;
}

/* 内部：记录延迟（vectors.c 中搜索完成后调用） */
void mmdb_metrics_record_latency(double latency_ms) {
    g_latency_buf[g_latency_idx] = latency_ms;
    g_latency_idx = (g_latency_idx + 1) & (LATENCY_BUF_SIZE - 1);
    if (g_latency_count < LATENCY_BUF_SIZE) g_latency_count++;
}

size_t mmdb_metrics_prometheus_format(char* buf, size_t buf_size) {
    const mmdb_metrics_t* m = mmdb_metrics_get();
    int written = snprintf(buf, buf_size,
        "# HELP mmsdk_vectors_total Total number of vectors inserted\n"
        "# TYPE mmsdk_vectors_total counter\n"
        "mmsdk_vectors_total %llu\n"
        "# HELP mmsdk_queries_total Total number of queries\n"
        "# TYPE mmsdk_queries_total counter\n"
        "mmsdk_queries_total %llu\n"
        "# HELP mmsdk_queries_success Successful queries\n"
        "# TYPE mmsdk_queries_success counter\n"
        "mmsdk_queries_success %llu\n"
        "# HELP mmsdk_queries_failed Failed queries\n"
        "# TYPE mmsdk_queries_failed counter\n"
        "mmsdk_queries_failed %llu\n"
        "# HELP mmsdk_query_latency_avg_ms Average query latency in milliseconds\n"
        "# TYPE mmsdk_query_latency_avg_ms gauge\n"
        "mmsdk_query_latency_avg_ms %.2f\n"
        "# HELP mmsdk_query_latency_p99_ms P99 query latency in milliseconds\n"
        "# TYPE mmsdk_query_latency_p99_ms gauge\n"
        "mmsdk_query_latency_p99_ms %.2f\n"
        "# HELP mmsdk_cache_hit_rate Cache hit rate\n"
        "# TYPE mmsdk_cache_hit_rate gauge\n"
        "mmsdk_cache_hit_rate %.4f\n"
        "# HELP mmsdk_memory_used_bytes Memory used in bytes\n"
        "# TYPE mmsdk_memory_used_bytes gauge\n"
        "mmsdk_memory_used_bytes %zu\n",
        (unsigned long long)m->vectors_total,
        (unsigned long long)m->queries_total,
        (unsigned long long)m->queries_success,
        (unsigned long long)m->queries_failed,
        m->query_latency_avg_ms,
        m->query_latency_p99_ms,
        m->cache_hit_rate,
        m->memory_used_bytes
    );
    return (written > 0 && (size_t)written >= buf_size) ? -(ssize_t)written : (size_t)written;
}
```

- [ ] **Step 3: 修改 vectors.c — 集成指标收集**

在 `engineering/src/sdk/vectors/vectors.c` 的 `mmdb_vectors_search` 函数中，在搜索开始和结束时记录指标：

```c
/* 在 mmdb_vectors_search 函数开头添加： */
#include "mmdb_metrics.h"
/* ... */
mmdb_metrics_t* m = (mmdb_metrics_t*)mmdb_metrics_get();
m->queries_total++;
double start_time = /* 获取当前时间 ms */;

/* 在函数返回前（成功路径）添加： */
double end_time = /* 获取当前时间 ms */;
double latency = end_time - start_time;
mmdb_metrics_record_latency(latency);
m->queries_success++;
m->query_latency_avg_ms = (m->query_latency_avg_ms * (m->queries_success - 1) + latency) / m->queries_success;

/* 在函数返回前（失败路径）添加： */
m->queries_failed++;
```

- [ ] **Step 4: 创建 metrics_test.cpp**

创建 `engineering/test/sdk/integration/metrics_test.cpp`：

```cpp
/**
 * @file metrics_test.cpp
 * @brief P6-M1.2 监控指标测试
 */
#include <gtest/gtest.h>
extern "C" {
#include "mmdb_metrics.h"
#include "mmdb.h"
}

class MetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        mmdb_metrics_reset();
    }
};

TEST_F(MetricsTest, InitialState) {
    const mmdb_metrics_t* m = mmdb_metrics_get();
    EXPECT_EQ(m->vectors_total, 0u);
    EXPECT_EQ(m->queries_total, 0u);
    EXPECT_EQ(m->queries_success, 0u);
}

TEST_F(MetricsTest, Reset) {
    /* 模拟增加一些计数器（通过内部 API 或直接修改） */
    /* 注意：实际测试需要暴露内部增加函数，或通过 SDK API 间接测试 */
    mmdb_metrics_reset();
    const mmdb_metrics_t* m = mmdb_metrics_get();
    EXPECT_EQ(m->queries_total, 0u);
}

TEST_F(MetricsTest, PrometheusFormat) {
    char buf[4096];
    size_t len = mmdb_metrics_prometheus_format(buf, sizeof(buf));
    EXPECT_GT(len, 0);
    EXPECT_STRSTR(buf, "mmsdk_vectors_total");
    EXPECT_STRSTR(buf, "mmsdk_queries_total");
    EXPECT_STRSTR(buf, "# HELP");
    EXPECT_STRSTR(buf, "# TYPE");
}
```

- [ ] **Step 5: 注册测试并运行**

```bash
cd engineering && cmake -B ../build/engineering -G Ninja && cmake --build ../build/engineering --target metrics_test && ../build/engineering/test/sdk/integration/metrics_test.exe
```

- [ ] **Step 6: Commit**

```bash
git add engineering/include/sdk/mmdb_metrics.h engineering/src/sdk/core/metrics.c engineering/test/sdk/integration/metrics_test.cpp
git commit -m "feat(sdk): P6-M1.2 监控指标 — Prometheus 格式暴露"
```

---

## 任务 3：M1.3 1M 性能验证

### 概述

验证 1M 规模下的 Recall@10 和 QPS，调优 HNSW 参数。

### 文件

- 修改：`docs/performance-scale-report.md`

### 接口

- 消费：现有 `staircase_benchmark.cpp`
- 产出：性能数据填入报告

---

- [ ] **Step 1: 跑 baseline 1M 测试**

```bash
cd build/engineering && ./sdk_integration_tests/staircase_benchmark.exe --gtest_filter=Staircase.VectorKNN1M
```

记录输出：
- Recall@10 = ?
- search QPS = ?
- P99 latency = ?

- [ ] **Step 2: 根据结果调参**

如果 Recall@10 < 0.85：
- 增加 `ef_search`（从当前值调到 200/300/500）

如果 QPS < 1000：
- 增加 `n_threads`（从 1 调到 8）
- 或启用更多 SIMD 距离函数

- [ ] **Step 3: 填入报告**

打开 `docs/performance-scale-report.md`，更新 1M 行：

```markdown
| 1M       | _实测值_      | _实测值_              | _实测值_     | _实测值_     | _实测值_   | _实测值_   | ≥0.85      | PASS |
```

- [ ] **Step 4: Commit**

```bash
git add docs/performance-scale-report.md
git commit -m "docs(p6): P6-M1.3 1M 性能验证数据填入报告"
```

---

## 任务 4：M4.1 通用聚合框架

### 概述

新增 `mmdb_aggregate.h` 提供 GROUP BY / COUNT / SUM / AVG / MIN / MAX / HISTOGRAM 聚合能力。

### 文件

- 创建：`engineering/include/sdk/mmdb_aggregate.h`
- 创建：`engineering/src/sdk/core/aggregate.c`
- 创建：`engineering/test/sdk/integration/aggregate_test.cpp`

### 接口

- 消费：M1.1 的 `offset`/`limit` 语义
- 产出：
  - `mmdb_aggregate()` 函数
  - `mmdb_aggregate_result_free()` 函数

---

- [ ] **Step 1-6: 实现通用聚合框架**

（参考规格文档 3.4.1 节的 API 设计）

- [ ] **Step 7: Commit**

```bash
git add engineering/include/sdk/mmdb_aggregate.h engineering/src/sdk/core/aggregate.c engineering/test/sdk/integration/aggregate_test.cpp
git commit -m "feat(sdk): P6-M4.1 通用聚合框架 — GROUP BY/COUNT/SUM/AVG"
```

---

## 任务 5：M2.2 ACID 事务

### 概述

新增 `mmdb_transaction.h` 提供 begin/commit/rollback 事务支持。

### 文件

- 创建：`engineering/include/sdk/mmdb_transaction.h`
- 创建：`engineering/src/sdk/core/transaction.c`
- 创建：`engineering/test/sdk/integration/transaction_test.cpp`

### 接口

- 消费：M4.1 的聚合框架
- 产出：
  - `mmdb_txn_begin()` / `mmdb_txn_commit()` / `mmdb_txn_abort()` / `mmdb_txn_free()` 函数

---

- [ ] **Step 1-6: 实现 ACID 事务**

（参考规格文档 3.2.2 节的 API 设计）

- [ ] **Step 7: Commit**

```bash
git add engineering/include/sdk/mmdb_transaction.h engineering/src/sdk/core/transaction.c engineering/test/sdk/integration/transaction_test.cpp
git commit -m "feat(sdk): P6-M2.2 ACID 事务 — begin/commit/rollback"
```

---

## 任务 6：M2.1 备份/恢复

### 概述

新增 `mmdb_backup.h` 提供在线快照和恢复能力。

### 文件

- 创建：`engineering/include/sdk/mmdb_backup.h`
- 创建：`engineering/src/sdk/core/backup.c`
- 创建：`engineering/test/sdk/integration/backup_test.cpp`

### 接口

- 消费：无
- 产出：
  - `mmdb_backup_create()` / `mmdb_backup_wait()` / `mmdb_backup_free()` / `mmdb_backup_restore()` / `mmdb_backup_list()` 函数

---

- [ ] **Step 1-6: 实现备份/恢复**

（参考规格文档 3.2.1 节的 API 设计）

- [ ] **Step 7: Commit**

```bash
git add engineering/include/sdk/mmdb_backup.h engineering/src/sdk/core/backup.c engineering/test/sdk/integration/backup_test.cpp
git commit -m "feat(sdk): P6-M2.1 备份/恢复 — 在线快照与恢复"
```

---

## 任务 7：M3.1 复制

### 概述

新增 `mmdb_replication.h` 提供 Raft 复制和高可用。

### 文件

- 创建：`engineering/include/sdk/mmdb_replication.h`
- 创建：`engineering/src/sdk/extra/replication.c`
- 创建：`engineering/test/sdk/integration/replication_test.cpp`

### 接口

- 消费：M2.2 的 ACID 事务
- 产出：
  - `mmdb_replication_init()` / `mmdb_replication_info()` / `mmdb_replication_failover()` 函数

---

- [ ] **Step 1-6: 实现复制**

（参考规格文档 3.3.1 节的 API 设计，复用 `engineering/src/db/raft/` 已有占位）

- [ ] **Step 7: Commit**

```bash
git add engineering/include/sdk/mmdb_replication.h engineering/src/sdk/extra/replication.c engineering/test/sdk/integration/replication_test.cpp
git commit -m "feat(sdk): P6-M3.1 复制 — Raft 高可用"
```

---

## 任务 8：M3.2 分片

### 概述

新增 `mmdb_sharding.h` 提供一致性哈希分片。

### 文件

- 创建：`engineering/include/sdk/mmdb_sharding.h`
- 创建：`engineering/src/sdk/extra/sharding.c`
- 创建：`engineering/test/sdk/integration/sharding_test.cpp`

### 接口

- 消费：M3.1 的复制
- 产出：
  - `mmdb_sharding_init()` / `mmdb_sharding_move()` / `mmdb_sharding_stats()` 函数

---

- [ ] **Step 1-6: 实现分片**

（参考规格文档 3.3.2 节的 API 设计）

- [ ] **Step 7: Commit**

```bash
git add engineering/include/sdk/mmdb_sharding.h engineering/src/sdk/extra/sharding.c engineering/test/sdk/integration/sharding_test.cpp
git commit -m "feat(sdk): P6-M3.2 分片 — 一致性哈希"
```

---

## 任务 9：M3.3 多租户

### 概述

新增 `mmdb_namespace.h` 提供命名空间隔离和资源配额。

### 文件

- 创建：`engineering/include/sdk/mmdb_namespace.h`
- 创建：`engineering/src/sdk/extra/namespace.c`
- 创建：`engineering/test/sdk/integration/namespace_test.cpp`

### 接口

- 消费：M3.2 的分片
- 产出：
  - `mmdb_namespace_create()` / `mmdb_namespace_get()` / `mmdb_namespace_set_quota()` / `mmdb_namespace_usage()` / `mmdb_namespace_drop()` 函数

---

- [x] **Step 1-6: 实现多租户** — 已验证 ✅ 2026-08-25

   - 新增 `engineering/include/sdk/mmdb_namespace.h` — 完整 API 头文件（create/get/set_quota/usage/check_quota/add_vectors/drop/name）
   - 新增 `engineering/src/sdk/core/namespace.c` — 全局链表存储，标记删除，配额管理，JSON usage 输出
   - 新增 `engineering/test/sdk/integration/namespace_test.cpp` — 18 个测试用例全部通过

- [x] **Step 7: Commit** — `65c675551` feat(sdk): P6-M3.3 命名空间增强 — check_quota/add_vectors/name API

---

## 任务 10：M4.2 增强时序聚合

### 概述

扩展 `mmdb_timeseries.h` 添加滑动窗口聚合。

### 文件

- 修改：`engineering/include/sdk/mmdb_timeseries.h`
- 修改：`engineering/src/sdk/timeseries/timeseries.c`
- 修改：`engineering/test/sdk/timeseries/timeseries_test.cpp`

### 接口

- 消费：M4.1 的通用聚合
- 产出：
  - `mmdb_ts_aggregate()` 函数（滑动窗口）

---

- [ ] **Step 1-6: 实现时序增强**

（参考规格文档 3.4.2 节的 API 设计）

- [ ] **Step 7: Commit**

```bash
git add engineering/include/sdk/mmdb_timeseries.h engineering/src/sdk/timeseries/timeseries.c engineering/test/sdk/timeseries/timeseries_test.cpp
git commit -m "feat(sdk): P6-M4.2 时序增强 — 滑动窗口聚合"
```

---

## 验收标准总表

| 任务 | 验收条件 | 优先级 | 状态 |
|------|----------|--------|------|
| M1.1 分页 | offset/limit/total_count/has_more 正确 | P0 | ⬜ |
| M1.2 监控 | Prometheus 格式输出 + 指标准确 | P1 | ⬜ |
| M1.3 1M 性能 | Recall@10 ≥ 0.85, QPS ≥ 1000 | P0 | ⬜ |
| M2.1 备份 | 在线备份不阻塞 + 可恢复 | P1 | ⬜ |
| M2.2 事务 | 并发安全 + 回滚正确 | P1 | ⬜ |
| M3.1 复制 | 30s 故障转移 + 最终一致 | P1 | ⬜ |
| M3.2 分片 | 均匀分布 + 跨分片查询正确 | P1 | ⬜ |
| M3.3 多租户 | 隔离 + 配额限制 | P2 | ✅ |
| M4.1 聚合 | GROUP BY / COUNT / AVG 等正确 | P2 | ⬜ |
| M4.2 时序增强 | 滑动窗口 + fill_empty 正确 | P2 | ⬜ |

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 工期超期（6-12 月） | 中 | 高 | 季度 Checkpoint Review，动态调整 |
| 1M 性能不达标 | 中 | 中 | M1.3 先跑基准，根据结果决策 |
| 复制/分片复杂度高 | 高 | 高 | 参考 `docs/architecture/db/distributed/` 设计 |
| MVCC 实现困难 | 低 | 高 | M2.2 暂不实现 MVCC，只做基础事务 |
