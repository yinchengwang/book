# 向量流式插入设计文档

> 本文档详细描述向量索引流式插入的实现设计。

## 1. 概述

### 1.1 目标

为自研 HNSW/DiskANN/IVF-PQ 索引实现增量流式插入能力，支持：
- 批量插入聚合 + 后台异步合并
- 增量量化器更新（避免全量重训）
- 无锁并发查询（查询覆盖缓冲区 + 主索引）

### 1.2 架构图

```
┌─────────────────────────────────────────────────────────────┐
│  流式索引架构                                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  插入请求 ──→ ┌─────────────────┐ ──→ 后台合并线程          │
│              │  写入缓冲区      │          │                  │
│              │  (环形队列)      │          ▼                  │
│              └─────────────────┘   ┌─────────────────┐      │
│                      ▲             │  增量量化器      │      │
│                      │             │  (在线 k-means) │      │
│                      │             └─────────────────┘      │
│                      │                     │                  │
│                      └─────────────────────┘                  │
│                                     │                         │
│                                     ▼                         │
│                              ┌─────────────────┐             │
│                              │  主索引         │             │
│                              │  (HNSW/DiskANN) │             │
│                              └─────────────────┘             │
│                                     ▲                         │
│                                     │                         │
│                              ┌──────┴──────┐                 │
│                              │ 并发查询适配器 │                 │
│                              │ (结果去重)   │                 │
│                              └─────────────┘                 │
│                                     │                         │
│                                     ▼                         │
│                              ┌─────────────────┐             │
│                              │  查询结果       │             │
│                              │  (Top-K)        │             │
│                              └─────────────────┘             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 2. 核心组件

### 2.1 向量写入缓冲区 (vector_write_buffer)

**设计决策**：
- 使用环形队列实现，FIFO 语义
- 支持无锁 push/pop（需要外部同步）
- 自动扩容（倍增策略）

**关键接口**：
```c
vector_write_buffer_t *vector_buffer_create(const vector_buffer_config_t *config);
int32_t vector_buffer_push_batch(vector_write_buffer_t *buf, const float *vectors, int32_t n);
int32_t vector_buffer_flush(vector_write_buffer_t *buf, float *vectors, int32_t max_n);
bool vector_buffer_need_flush(const vector_write_buffer_t *buf);
```

**数据布局**：
```
┌─────────────────────────────────────────────────────────────┐
│  环形队列布局                                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  head ──→ [vec0][vec1][vec2]...[vecN][    ][    ] ──→ tail │
│           ▲ 已使用                    ▲ 未使用               │
│                                                             │
│  当 head == tail 时，表示队列为空或队满                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 增量量化器 (incremental_quant_train)

**设计决策**：
- 使用在线 k-means 增量更新码书
- 检测码书漂移，漂移过大时触发全量重建
- 累计更新次数达到阈值时强制全量重建

**在线 k-means 算法**：
```
centroid = centroid * (1 - learning_rate) + sample * learning_rate
```

**触发条件**：
| 条件 | 阈值 |
|------|------|
| 增量更新次数 | 默认 10 次 |
| 码书漂移距离 | 默认 0.1 |

**关键接口**：
```c
incremental_quant_trainer_t *incremental_quant_trainer_create(
    quantizer_t *base_quantizer,
    const incremental_quant_config_t *config);
int32_t incremental_quant_update(
    incremental_quant_trainer_t *trainer,
    int32_t n,
    const float *vectors);
bool incremental_quant_need_full_retrain(const incremental_quant_trainer_t *trainer);
```

### 2.3 后台合并调度器 (merge_scheduler)

**设计决策**：
- 后台线程独立执行合并任务
- 任务队列 FIFO 调度
- 支持定时触发和阈值触发

**任务类型**：
- `MERGE_TASK_FLUSH`：刷新缓冲区到主索引
- `MERGE_TASK_REBUILD`：重建索引
- `MERGE_TASK_OPTIMIZE`：图优化

**关键接口**：
```c
merge_scheduler_t *merge_scheduler_create(
    vector_index_t *index,
    const merge_scheduler_config_t *config);
int32_t merge_scheduler_trigger(merge_scheduler_t *sched, const merge_task_t *task);
int32_t merge_scheduler_wait(merge_scheduler_t *sched);
```

### 2.4 并发查询适配器 (concurrent_search)

**设计决策**：
- 同时搜索主索引和缓冲区
- 合并结果时应用权重调整
- 支持结果去重

**查询流程**：
```
1. 搜索主索引 → main_results[]
2. 搜索缓冲区 → buffer_results[]
3. 合并结果（应用 buffer_weight）
4. 去重
5. 返回 Top-K
```

**关键接口**：
```c
concurrent_search_adapter_t *concurrent_search_create(
    vector_index_t *index,
    vector_write_buffer_t *buffer,
    const concurrent_search_config_t *config);
int32_t concurrent_search(
    concurrent_search_adapter_t *adapter,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);
```

### 2.5 流式索引 (streaming_index)

**统一入口**，包装上述所有组件。

**关键接口**：
```c
streaming_index_t *streaming_index_create(const streaming_index_config_t *config);
int32_t streaming_index_streaming_add(streaming_index_t *index, const float *vectors, int32_t n);
int32_t streaming_index_search(
    streaming_index_t *index,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);
```

## 3. 使用示例

### 3.1 基本使用

```c
#include "streaming/streaming_index.h"

// 创建配置
streaming_index_config_t config = streaming_index_config_default(
    STREAMING_INDEX_HNSW, 128);
config.buffer_capacity = 10000;
config.buffer_flush_threshold = 1000;

// 创建流式索引
streaming_index_t *index = streaming_index_create(&config);

// 流式插入（非阻塞）
float vectors[100 * 128];
generate_random_vectors(vectors, 100, 128);
streaming_index_streaming_add(index, vectors, 100);

// 查询（自动包含缓冲区）
float query[128];
float distances[10];
int32_t ids[10];
streaming_index_search(index, query, 10, distances, ids);

// 等待所有插入完成
streaming_index_wait(index);

// 销毁
streaming_index_destroy(index);
```

### 3.2 包装现有索引

```c
#include "streaming/streaming_index.h"
#include "hnsw/faiss_hnsw.h"

// 创建 HNSW 索引
faiss_hnsw_t *hnsw = faiss_hnsw_index_create(16, 128, 200, ...);

// 包装为流式索引
streaming_index_config_t config = streaming_index_config_default(
    STREAMING_INDEX_HNSW, 128);
streaming_index_t *stream = streaming_index_wrap(hnsw, STREAMING_INDEX_HNSW, &config);

// 后续使用同上
```

## 4. 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `buffer_capacity` | 10000 | 缓冲区容量（向量数） |
| `buffer_flush_threshold` | 1000 | 自动刷写阈值 |
| `merge_interval_ms` | 1000 | 合并间隔（毫秒） |
| `enable_incremental_quant` | true | 启用增量量化 |
| `max_incremental_updates` | 10 | 最大增量更新次数 |

## 5. 性能指标

| 指标 | 目标 | 说明 |
|------|------|------|
| 插入 QPS | 提升 5x | 相比同步插入 |
| 查询延迟 | P99 < 50ms | 无阻塞 |
| 内存开销 | 缓冲区 < 10% 索引内存 | |
| 召回率 | 合并后 ≥ 99.5% 基线 | |

## 6. 已知限制

1. **线程安全**：当前缓冲区 push/pop 需要外部同步
2. **底层索引集成**：需要与 HNSW/DiskANN 实际集成（当前为占位实现）
3. **分布式支持**：暂不支持分布式流式插入

## 7. 未来规划

- [ ] 与 HNSW/DiskANN 实际集成
- [ ] 添加线程安全支持
- [ ] 支持分布式流式插入
- [ ] 添加图结构全局优化
