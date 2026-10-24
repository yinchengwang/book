# GPU-IVF 索引规格

## 概述

GPU-IVF 是基于 IVF (Inverted File) 的向量索引，利用 GPU 并行计算加速聚类和搜索过程。

## 功能需求

### 核心功能

| 功能 | 描述 | 优先级 |
|------|------|--------|
| K-Means 训练 | GPU 加速的聚类中心计算 | P0 |
| 倒排索引构建 | GPU 构建倒排文件 | P0 |
| 搜索 | GPU 并行距离计算和排序 | P0 |
| PQ 编码 | GPU 加速的乘积量化 | P1 |

### API

```c
// 创建 GPU-IVF 索引
gpu_ivf_index_t* gpu_ivf_create(
    gpu_context_t* ctx,
    int nlist,
    int nprobe
);

// 训练
int gpu_ivf_train(
    gpu_ivf_index_t* index,
    const float* vectors,
    int n
);

// 添加向量
int gpu_ivf_add(
    gpu_ivf_index_t* index,
    const float* vectors,
    int n,
    const int64_t* ids
);

// 搜索
int gpu_ivf_search(
    gpu_ivf_index_t* index,
    const float* queries,
    int nqueries,
    int k,
    int64_t* ids,
    float* distances
);

// 销毁
void gpu_ivf_destroy(gpu_ivf_index_t* index);
```

## 性能指标

| 指标 | 目标 |
|------|------|
| 训练吞吐量 | 1M+ 向量/秒 |
| 搜索延迟 (1M 数据) | < 1ms |
| Recall | >= 0.90 |

## 验收标准

- [x] `gpu_ivf_create` 正确创建索引
- [x] `gpu_ivf_train` GPU 聚类训练
- [x] `gpu_ivf_add` 添加向量到倒排列表
- [x] `gpu_ivf_search` 返回正确结果
- [ ] 性能基准测试通过
