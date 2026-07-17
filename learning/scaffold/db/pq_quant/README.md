# pq_quant - PQ（乘积量化）原理

## 概述

PQ（Product Quantization，乘积量化）是一种高效的向量量化技术，广泛应用于近似最近邻搜索（ANN）系统，如向量数据库和图像检索。

## 核心思想

将高维向量划分为多个子空间，每个子空间独立进行量化（K-Means），从而实现高压缩率。

```
原始向量 (D=8):  [v0, v1, v2, v3, v4, v5, v6, v7]
                        ↓
    ┌──────────────┬──────────────┐
    │  子空间 0    │  子空间 1    │
    │  [v0,v1,v2,v3]│ [v4,v5,v6,v7]│
    └──────┬───────┴───────┬──────┘
           │               │
    ┌──────▼───────┐ ┌─────▼──────┐
    │   码本 0     │ │   码本 1   │
    │  4 个码字    │ │  4 个码字  │
    └──────────────┘ └────────────┘
           │               │
           └───────┬───────┘
                   ↓
          编码: [code0, code1]
```

## 核心概念

### 1. 子空间划分（Sub-space Partitioning）

将 D 维向量划分为 M 个子向量，每个子向量维度为 D/M：

- 子量化器数量：M = D / sub_dim
- 本示例：D=8, M=2, sub_dim=4

### 2. 码本训练（Codebook Training）

对每个子空间独立运行 K-Means 聚类：

```c
// 对每个子空间训练独立的码本
for (sub = 0; sub < num_subquantizers; sub++) {
    // 提取子向量
    // 运行 K-Means (pq.c: n_init=8, max_iter=100)
    // 聚类中心即码字
}
```

### 3. 编码（Encoding）

将每个子向量映射到最近码字的索引：

- 每个子空间输出 1 个码字索引
- 原始向量编码为 M 个字节（假设码本大小 ≤ 256）
- 压缩率：D float32 → M bytes

### 4. 查表距离计算（Lookup Table Distance）

预先计算查询向量与所有码字的距离，然后通过查表计算近似距离：

```c
// 构建查表：query 与码本中所有码字的距离
lookup[sub][code] = dist(query_sub, centroid[sub][code])

// 计算两个编码向量的近似距离
approx_dist = sum(lookup[sub][code1] + lookup[sub][code2])
```

## 存储优势

| 指标 | 原始向量 | PQ 编码 |
|------|----------|---------|
| 存储大小 | D × 4 bytes | M × 1 byte |
| 压缩率 | 1x | 32x (D=128, M=4) |
| 距离计算 | O(D) | O(M) |

## 编译运行

```bash
make && ./test
```

## 参考实现

参考 `engineering/src/algo-prod/quantization/pq.c`
