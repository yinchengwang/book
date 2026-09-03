# GPU-HNSW 索引规格

## 概述

GPU-HNSW 是在 HNSW (Hierarchical Navigable Small World) 图索引基础上，利用 GPU 并行计算能力加速向量搜索的索引类型。

## 功能需求

### 核心功能

| 功能 | 描述 | 优先级 |
|------|------|--------|
| 图构建 | GPU 加速的 HNSW 图构建 | P0 |
| 搜索 | GPU 并行最近邻搜索 | P0 |
| 内存管理 | GPU 内存池管理 | P0 |
| CPU 降级 | GPU 不可用时自动降级到 CPU | P1 |

### API

```c
// 创建 GPU-HNSW 索引
gpu_hnsw_index_t* gpu_hnsw_create(
    gpu_context_t* ctx,
    int max_elements,
    int m,
    int ef_construction,
    int ef_search
);

// 插入向量
int gpu_hnsw_insert(
    gpu_hnsw_index_t* index,
    const float* vectors,
    int n
);

// 搜索
int gpu_hnsw_search(
    gpu_hnsw_index_t* index,
    const float* queries,
    int nqueries,
    int k,
    int64_t* ids,
    float* distances
);

// 销毁
void gpu_hnsw_destroy(gpu_hnsw_index_t* index);
```

## 性能指标

| 指标 | 目标 |
|------|------|
| 构建吞吐量 | 500K+ 向量/秒 |
| 搜索延迟 (1M 数据) | < 2ms |
| Recall | >= 0.95 |

## 验收标准

- [x] `gpu_hnsw_create` 正确创建索引
- [x] `gpu_hnsw_insert` 批量插入向量
- [x] `gpu_hnsw_search` 返回正确结果
- [x] `gpu_hnsw_destroy` 正确释放资源
- [ ] 性能基准测试通过
