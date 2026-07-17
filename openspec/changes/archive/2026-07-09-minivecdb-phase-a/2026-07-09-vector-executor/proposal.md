# A2: 向量查询执行器

## 概述

实现统一的向量查询执行管道，整合粗排、精排、混合检索能力。

## 问题背景

当前各向量索引独立运行，缺乏统一的查询执行器来协调：
- 粗排阶段 (HNSW/DiskANN)
- 精排阶段 (ReRanker)
- 混合检索 (BM25 + Vector)

## 目标

- 统一的向量查询执行管道
- 两阶段检索 (粗排 → 精排)
- 混合检索能力
- 查询计划缓存

## 任务清单

### 接口设计

- [ ] A2.1 审视 `include/db/core/vector_exec.h` 现有接口
- [ ] A2.2 设计 VectorQueryPlan 结构
- [ ] A2.3 设计 VectorQueryContext 执行上下文

### 执行器实现

- [ ] A2.4 实现 `vector_exec_init()` 初始化
- [ ] A2.5 实现 `vector_exec_shutdown()` 关闭
- [ ] A2.6 实现 `vector_exec_create_plan()` 创建查询计划
- [ ] A2.7 实现 `vector_exec_execute()` 执行查询
- [ ] A2.8 实现 `vector_exec_destroy_plan()` 销毁计划

### 两阶段检索

- [ ] A2.9 实现粗排阶段调度 (HNSW/DiskANN)
- [ ] A2.10 实现精排阶段调度 (ReRanker)
- [ ] A2.11 实现 MMR 去重

### 混合检索

- [ ] A2.12 实现 BM25 + Vector 混合
- [ ] A2.13 实现 RRF 融合
- [ ] A2.14 实现加权融合

### 优化

- [ ] A2.15 实现查询计划缓存
- [ ] A2.16 实现执行统计
- [ ] A2.17 实现执行器钩子 (profile/debug)

### 测试

- [ ] A2.18 单元测试
- [ ] A2.19 集成测试 (与 A1 对接)

## 关键文件

### 新建

```
src/db/core/vector_exec.c     # 执行器实现
test/db/core/vector_exec_test.cpp
```

### 已有 (复用)

```
include/db/core/vector_exec.h # 已有接口
include/db/index/vector_index/reranker/reranker.h
include/db/index/vector_index/hybrid_search.h
```

## 技术方案

### 查询管道

```
┌─────────────────────────────────────────────────────────────────┐
│                    VectorQueryPipeline                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│   Query ─→ Parser ─→ Plan ─→ Dispatcher ─→ [粗排] ─→ [精排]    │
│                                      │                          │
│                                      │ 候选集                   │
│                                      ↓                          │
│                                  [MMR] ─→ [融合] ─→ 结果      │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### 查询计划结构

```c
typedef struct {
    // 粗排配置
    IndexType coarse_index_type;  // HNSW, DISKANN, IVF
    int ef_search;
    int max_candidates;

    // 精排配置
    RerankerType reranker_type;  // PRECISE, MMR, MULTI_METRIC
    RerankerConfig reranker_config;

    // 混合配置
    bool hybrid_enabled;
    float vector_weight;
    float text_weight;
    FusionType fusion_type;      // RRF, WEIGHTED
} VectorQueryPlan;
```

## 依赖关系

- 前置: A1 (向量持久化层)
- 后置: A3 (API 层)

## 评估标准

- [ ] 两阶段检索管道可工作
- [ ] 粗排 10000 候选 → 精排返回 TopK
- [ ] BM25 + Vector 混合检索可用
- [ ] 查询延迟 < 100ms (100万向量)
