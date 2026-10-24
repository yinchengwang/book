# SIMD 优化规格

## 概述

SIMD (Single Instruction Multiple Data) 优化利用 CPU 向量指令加速向量计算，包括 AVX-512、AVX2 和 SSE。

## 功能需求

### 核心功能

| 功能 | 描述 | 优先级 |
|------|------|--------|
| 距离计算 | L2、内积、余弦距离的 SIMD 加速 | P0 |
| 向量量化 | Product Quantization 的 SIMD 加速 | P0 |
| 批量计算 | 批量向量距离计算 | P0 |
| 内存预取 | 数据预取优化 | P1 |

### API

```c
// 初始化 SIMD
void simd_init(void);

// L2 距离 (SIMD)
float simd_l2_distance(
    const float* a,
    const float* b,
    int dim
);

// 内积 (SIMD)
float simd_inner_product(
    const float* a,
    const float* b,
    int dim
);

// 余弦距离 (SIMD)
float simd_cosine_distance(
    const float* a,
    const float* b,
    int dim
);

// 批量 L2 距离
void simd_batch_l2_distance(
    const float* query,
    const float* database,
    int n,
    int dim,
    float* distances
);

// 批量内积
void simd_batch_inner_product(
    const float* query,
    const float* database,
    int n,
    int dim,
    float* distances
);
```

## 支持的指令集

| 指令集 | 说明 | 优先级 |
|--------|------|--------|
| AVX-512 | 512 位向量，Intel Skylake+ | P0 |
| AVX2 | 256 位向量，Intel Haswell+ | P0 |
| SSE4.2 | 128 位向量，Intel Nehalem+ | P1 |
| NEON | ARM 向量，ARM64 | P2 |

## 性能指标

| 指标 | 目标 (AVX-512) |
|------|----------------|
| 单向量 L2 (128维) | < 100ns |
| 批量 L2 (1K 向量) | < 50μs |
| 内存带宽利用率 | >= 80% |

## 验收标准

- [x] `simd_init` 自动检测最佳指令集
- [x] `simd_l2_distance` 返回正确结果
- [x] `simd_inner_product` 返回正确结果
- [x] `simd_batch_l2_distance` 批量计算正确
- [ ] 性能基准测试达标
