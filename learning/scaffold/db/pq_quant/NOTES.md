# PQ 量化对照笔记

本文档对照 `engineering/src/algo-prod/quantization/pq.c`，描述乘积量化的实现细节。

## 1. 数据结构

### 量化器配置（quantizer_config_t）

```c
typedef struct {
    int32_t     dims;               // 向量维度 D
    int32_t     pq_subquantizers;   // 子量化器数量 M
    int32_t     pq_bits;            // 码本大小位数 (k = 2^bits)
} quantizer_config_t;
```

- `pq_bits`：控制每个子空间的码本大小，通常为 8（256 个码字）
- 本示例使用 CODEBOOK_SIZE=4（2^2），简化演示

### 量化器主体（quantizer_t）

```c
typedef struct {
    quantizer_config_t  config;     // 配置
    int32_t             subvector_dims;    // D / M
    int32_t             max_codebook_size; // 2^pq_bits
    int32_t             codebook_size;     // 实际码本大小
    float               *codebooks;         // 连续内存码本
    bool                trained;           // 训练标志
} quantizer_t;
```

**关键设计：码本采用连续内存布局**
```c
// 码本访问辅助函数
static float *pq_codebook_ptr(const quantizer_t *q, int32_t sub) {
    return &q->codebooks[sub * q->max_codebook_size * q->subvector_dims];
}
```

## 2. 训练流程

### K-Means 参数配置

pq.c 中的训练参数：
```c
params.n_clusters   = q->codebook_size;  // 码字数量
params.n_init       = 8;                  // 多次初始化取最优
params.max_iter     = 100;               // 最大迭代次数
params.tol          = 1e-4;               // 收敛阈值
params.init         = "k-means++";       // 初始化策略
params.algorithm   = "lloyd";            // Lloyd 算法
params.random_state = 42 + sub;          // 随机种子（子空间差异）
```

### 子向量提取

训练前需要将原始向量拆分为子向量：
```c
for (s = 0; s < n; ++s) {
    const float *sv = &vectors[s * q->config.dims + sub * q->subvector_dims];
    for (d = 0; d < q->subvector_dims; ++d) {
        train_data[s * q->subvector_dims + d] = (float)sv[d];
    }
}
```

## 3. 编码实现

### 单向量编码

```c
int32_t pq_encode(const quantizer_t *q, const float *vector, uint8_t *code) {
    for (sub = 0; sub < q->config.pq_subquantizers; ++sub) {
        const float *sv       = &vector[sub * q->subvector_dims];
        const float *codebook = pq_codebook_ptr(q, sub);
        float best_dist       = FLT_MAX;

        for (int c = 0; c < q->codebook_size; c++) {
            float dist = euclidean_distance(sv, &codebook[c * q->subvector_dims], q->subvector_dims);
            if (dist < best_dist) {
                best_dist = dist;
                code[sub] = c;  // 存储最近码字索引
            }
        }
    }
}
```

## 4. 距离计算优化

### 查表预计算

pq.c 支持预计算查询向量到码本的距离表：
```c
void pq_compute_lookup_table(const quantizer_t *q, const float *query, float *lookup);
```

### SIMD 加速

实际工程实现中可能使用 SIMD 指令集（AVX/SSE）加速距离计算。

## 5. 与本演示程序的区别

| 特性 | pq.c | main.c 演示 |
|------|------|-------------|
| 码本大小 | 2^pq_bits（通常 256） | 4（简化） |
| 训练算法 | K-Means（多次初始化） | 简化采样 |
| 内存布局 | 连续内存 | 结构体数组 |
| 距离计算 | 可选 SIMD | 朴素实现 |
| API 封装 | 完整生命周期管理 | 简化演示 |

## 6. 应用场景

PQ 量化的典型应用包括：
1. **向量数据库**：Faiss、Milvus 的索引加速
2. **图像检索**：大规模图像特征压缩
3. **推荐系统**：用户/物品向量压缩
4. **语义搜索**：Embedding 向量存储优化

## 7. 总结

PQ 量化的核心优势在于：
1. **高压缩率**：D 维浮点数压缩为 M 字节
2. **查表加速**：预计算距离表避免重复计算
3. **可扩展性**：子空间独立训练，易于并行化
