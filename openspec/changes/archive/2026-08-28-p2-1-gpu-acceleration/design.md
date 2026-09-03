# P2-1 GPU 向量加速 设计文档

## 架构概述

GPU 向量加速模块提供高性能的向量索引计算能力，支持 CUDA 和 OpenCL 双后端。

```
┌─────────────────────────────────────────────────────────────┐
│                    Vector Index API                          │
├─────────────────────────────────────────────────────────────┤
│  GPU-HNSW  │  GPU-IVF  │  GPU-IVF-PQ  │  SIMD Optimizer  │
├─────────────────────────────────────────────────────────────┤
│                    GPU Backend Abstraction                    │
├──────────────────────────┬──────────────────────────────────┤
│     CUDA Backend        │       OpenCL Backend              │
└──────────────────────────┴──────────────────────────────────┘
```

## 核心组件

### 1. GPU 设备管理 (`gpu_device.c`)

- 设备发现和选择
- 上下文管理
- 流管理
- 错误处理

### 2. GPU 内存管理 (`gpu_memory.c`)

- GPU 内存分配/释放
- CPU-GPU 数据传输
- 内存池优化
- 统一内存支持

### 3. GPU-HNSW (`gpu_hnsw.c`)

- GPU 加速的图构建
- GPU 并行搜索
- 层次遍历优化

### 4. GPU-IVF (`gpu_ivf.c`, `gpu_ivf_pq.c`)

- GPU K-Means 聚类
- GPU 倒排索引构建
- GPU 距离计算
- PQ 编码/解码加速

### 5. SIMD 优化 (`simd.c`, `gpu_simd.h`)

- AVX-512 向量量化
- 批量距离计算
- 内存预取优化

## API 设计

### 初始化

```c
// 初始化 GPU 设备
int gpu_init(void);

// 创建设备上下文
gpu_context_t* gpu_context_create(int device_id);

// 销毁上下文
void gpu_context_destroy(gpu_context_t* ctx);
```

### HNSW

```c
// 创建 GPU-HNSW 索引
gpu_hnsw_index_t* gpu_hnsw_create(gpu_context_t* ctx, hnsw_config_t* config);

// 插入向量
int gpu_hnsw_insert(gpu_hnsw_index_t* index, float* vectors, int n);

// 搜索
int gpu_hnsw_search(gpu_hnsw_index_t* index, float* query, int k, 
                    int64_t* ids, float* distances);
```

### IVF

```c
// 创建 GPU-IVF 索引
gpu_ivf_index_t* gpu_ivf_create(gpu_context_t* ctx, ivf_config_t* config);

// 训练
int gpu_ivf_train(gpu_ivf_index_t* index, float* vectors, int n);

// 搜索
int gpu_ivf_search(gpu_ivf_index_t* index, float* query, int nprobe, int k,
                   int64_t* ids, float* distances);
```

## 性能优化策略

1. **批量处理**：一次性传输大量向量，减少通信开销
2. **流水线**：CPU-GPU 并行执行
3. **内存预取**：异步数据传输
4. **共享内存**：利用 GPU 共享内存加速
5. **SIMD**：AVX-512 加速 CPU 端计算

## 错误处理

- 所有 GPU 函数返回错误码
- 支持 CUDA/OpenCL 错误转换
- 自动降级到 CPU 实现（当 GPU 不可用时）
