# MiniVecDB 技术文档

## 系统概述

MiniVecDB 是一个从零构建的向量数据库系统，实现了向量存储、检索、持久化的完整能力。

## 架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      MiniVecDB 架构                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│   ┌──────────────────────────────────────────────────────────┐   │
│   │              API 层 (A3)                                 │   │
│   │   vector_api.c - REST/CLI 接口                         │   │
│   └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│   ┌──────────────────────────────────────────────────────────┐   │
│   │              执行器层 (A2)                                │   │
│   │   vector_query.c - 两阶段检索管道                      │   │
│   │   粗排(HNSW/DiskANN/IVF) → 精排 → 融合               │   │
│   └──────────────────────────────────────────────────────────┘   │
│                              │                                   │
│   ┌──────────────────────────────────────────────────────────┐   │
│   │              持久化层 (A1)                               │   │
│   │   vector_index_persist.c - 索引序列化                 │   │
│   └──────────────────────────────────────────────────────────┘   │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

## 核心模块

### 1. 向量 API (src/db/api/vector_api.c)

集合管理：
```c
VectorAPI *api = vector_api_create("./data");
vector_api_create_collection(api, &params);
vector_api_drop_collection(api, "my_collection");
```

向量操作：
```c
// 插入
vector_api_insert(api, &insert_params, NULL);

// 搜索
VectorSearchResults *results = vector_api_search(api, &search_params);
for (int i = 0; i < results->count; i++) {
    printf("id=%lld, dist=%.4f\n",
           results->results[i].id,
           results->results[i].distance);
}
vector_api_results_free(results);
```

### 2. 向量查询执行器 (src/db/core/vector_query.c)

两阶段检索：
```
Query → 粗排(HNSW) → 精排(MMR) → 结果
         ↓
      100个候选 → 10个最终结果
```

配置示例：
```c
VectorQueryPlan *plan = vector_query_plan_create();
vector_query_plan_set_coarse(plan, VECTOR_INDEX_HNSW, 100, 500);
vector_query_plan_set_rerank(plan, RERANKER_MMR, 50);
```

### 3. 查询优化器 (src/db/optimizer/optimizer.c)

代价模型：
```c
double hnsw_cost = cost_vector_hnsw(100, 1000000, 10);
double ivf_cost = cost_vector_ivf(10, 1000000, 10);

const char *best = select_best_vector_index(1000000, 10, 100, 10);
// 返回最优索引类型
```

### 4. 分片支持 (src/db/sharding/sharding.c)

一致性哈希：
```c
uint64_t hash = vector_consistent_hash(vector, 128);
int shard_id = shard_route(router, &hash, sizeof(hash));
```

跨分片搜索：
```c
int *shards = vector_shard_route_search(router, query, 128, 10);
// 搜索所有相关分片并合并结果
```

## 测试

### 单元测试
```bash
./build/test/db/api/test_vector_api.exe
# 11/11 测试通过 ✓
```

### 基准测试
```bash
./build/test/db/benchmark/vector_benchmark.exe --gtest_filter="OptimizerBenchmark.*"
# 代价模型验证 ✓
```

## 编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
```

## 性能指标

| 指标 | 数值 |
|------|------|
| 向量插入 | < 1ms (1K 向量) |
| 向量搜索 | < 100ms (100K 向量) |
| 索引类型 | HNSW / DiskANN / IVF |

## 未来工作

- [ ] MVCC 完整实现
- [ ] 分布式事务
- [ ] 自动故障转移
- [ ] 监控指标

---

*Generated: 2026-07-09*
