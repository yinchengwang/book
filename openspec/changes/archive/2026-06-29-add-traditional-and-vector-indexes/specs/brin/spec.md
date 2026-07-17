# BRIN 块范围索引

## Purpose

实现 BRIN (Block Range Index) 索引，适合大表顺序扫描场景的低开销索引。

## Requirements

### Requirement: BRIN 索引基础操作

BRIN 索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `brin_index_t *brin_create(int page_size, int pages_per_range)` | 创建 BRIN 索引 |
| 插入 | `int brin_insert(brin_index_t *idx, int page_num, const void *key, int doc_id)` | 插入记录 |
| 批量插入 | `int brin_insert_batch(brin_index_t *idx, int *page_nums, const void **keys, int *doc_ids, int count)` | 批量插入 |
| 范围查询 | `int brin_range_query(brin_index_t *idx, const void *min_key, const void *max_key, int *results, int *count)` | 范围查询 |
| 扫描 | `int brin_scan(brin_index_t *idx, int start_page, int end_page, void **results, int *count)` | 页面扫描 |
| 更新摘要 | `int brin_update_summary(brin_index_t *idx, int page_num)` | 更新页面摘要 |
| 销毁 | `void brin_destroy(brin_index_t *idx)` | 释放资源 |

### Requirement: BRIN 核心数据结构

```c
// 页面摘要
typedef struct brin_summary {
    int page_num;                  // 起始页号
    int n_pages;                   // 页面数
    void *min_value;               // 该范围的最小值
    void *max_value;               // 该范围的最大值
    int null_count;                // NULL 值数量
    int has_null_min;              // 最小值是否为 NULL
    int has_null_max;              // 最大值是否为 NULL
    float avg_density;             // 平均密度
} brin_summary_t;

// BRIN 索引
typedef struct brin_index {
    int page_size;                 // 页面大小（字节）
    int pages_per_range;           // 每个范围的页面数
    int range_size;                // 范围大小（字节）
    void *summaries;               // brin_summary_t 数组
    int n_summaries;               // 摘要数量
    int key_size;                  // 键大小
    compare_fn_t compare;           // 比较函数
} brin_index_t;
```

### Requirement: 页面范围管理

| 功能 | 说明 |
|------|------|
| 自动范围划分 | 按 `pages_per_range` 自动划分页面范围 |
| 摘要更新 | 插入/更新时自动更新所在范围的摘要 |
| 延迟更新 | 支持批量更新后统一刷新摘要 |

### Requirement: 查询优化

BRIN 查询 SHALL 使用摘要快速跳过不相关页面：

| 场景 | 优化策略 |
|------|----------|
| 范围查询 `[min, max]` | 跳过 `max_value < min_key` 或 `min_value > max_key` 的范围 |
| 等值查询 `key = v` | 跳过 `v < min_value` 或 `v > max_value` 的范围 |
| 顺序扫描 | 按页面顺序扫描，无需随机 I/O |

### Requirement: 摘要统计

每个页面范围的摘要 SHALL 包含：

```c
// 数值类型摘要
typedef struct brin_numeric_summary {
    float min;
    float max;
    float sum;
    float avg;
    int count;
    int distinct_count;
} brin_numeric_summary_t;

// 时间类型摘要
typedef struct brin_timestamp_summary {
    int64_t min_ts;
    int64_t max_ts;
    int64_t span;                  // 时间跨度
} brin_timestamp_summary_t;
```

#### Scenario: 大表范围查询

- **WHEN** 用户在 100 万行表上执行 `SELECT * FROM t WHERE id BETWEEN 1000 AND 2000`
- **AND** BRIN 的 `pages_per_range = 128`
- **THEN** 系统 SHALL 只扫描包含该范围的页面（约 8 个页面）
- **AND** 跳过 99.99% 的页面

#### Scenario: 插入导致摘要更新

- **WHEN** 用户插入 `(page=5, key=999)` 到 BRIN 索引
- **AND** 页面 5 属于范围 [0, 127]
- **THEN** 系统 SHALL 更新范围 [0, 127] 的 `max_value = 999`
