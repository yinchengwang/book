# Proposal: 向量流式插入

## 1. Summary / 摘要

为自研 HNSW/DiskANN/IVF-PQ 索引实现增量流式插入能力，支持：
- 批量插入聚合 + 后台异步合并
- 增量量化器更新（避免全量重训）
- 无锁并发查询（查询覆盖缓冲区 + 主索引）

## 2. Motivation / 动机

当前自研向量索引的 `index_add()` 是同步阻塞的：
- 每次插入都触发图结构更新
- 量化索引（启用 PQ 时）可能触发全量重训
- 高频小批量插入性能差
- 无法支持实时增量更新场景

## 3. What Changes / 变更内容

### 3.1 核心架构

```
┌─────────────────────────────────────────────────────────────┐
│  插入流程优化                                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  批量插入 ──→ 增量缓冲(环形队列) ──→ 后台合并线程          │
│      │              │                     │                  │
│      │              │                     ▼                  │
│      │              │              增量图构建              │
│      │              │                     │                  │
│      │              ▼                     ▼                  │
│      └───────→ 可并发查询的"主索引+缓冲区"                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 新增组件

| 组件 | 文件 | 说明 |
|------|------|------|
| 向量写入缓冲区 | `vector_write_buffer.h/c` | 环形队列聚合批量插入 |
| 增量量化器 | `incremental_quant_train.h/c` | 避免全量重训 |
| 合并调度器 | `merge_scheduler.h/c` | 后台异步合并任务 |
| 并发查询适配器 | `concurrent_search.h/c` | 查询主索引+缓冲区 |

### 3.3 API 变更

```c
// 新增流式插入 API
int32_t vector_index_streaming_add(vector_index_t *index, 
                                    const float *vectors, 
                                    int32_t n);

// 新增缓冲区管理 API
int32_t vector_buffer_flush(vector_write_buffer_t *buf);
int32_t vector_buffer_set_flush_threshold(vector_write_buffer_t *buf, int32_t threshold);

// 新增合并控制 API
int32_t merge_scheduler_trigger(vector_merge_scheduler_t *sched);
int32_t merge_scheduler_wait(vector_merge_scheduler_t *sched);
```

## 4. Backward Compatibility / 向后兼容性

- 现有 `vector_index_add()` API 保持不变
- 新增 `vector_index_streaming_add()` 作为高级 API
- 持久化格式保持兼容

## 5. Out of Scope / 范围外

- 分布式流式插入（由 Phase 1 任务 5 覆盖）
- 图结构全局优化（长期规划）

## 6. Alternatives / 替代方案

| 方案 | 优点 | 缺点 |
|------|------|------|
| 方案A：标记删除+重建 | 实现简单 | 重建开销大 |
| **方案B：增量合并（推荐）** | 实时性好，无锁查询 | 实现复杂度中等 |
| 方案C：完全锁同步 | 实现最简单 | 并发性能差 |

## 7. Success Metrics / 成功指标

| 指标 | 目标 |
|------|------|
| 插入 QPS | 提升 5x（相比同步插入） |
| 查询延迟 | P99 < 50ms（无阻塞） |
| 内存开销 | 缓冲区 < 10% 索引内存 |
| 召回率 | 合并后 ≥ 99.5% 基线 |

## 8. Status / 状态

- [x] proposal.md - 本文档
- [x] design.md - 已创建 (`src/db/index/vector_index/streaming/DESIGN.md`)
- [x] tasks.md - 已完成 (28 个子任务)
