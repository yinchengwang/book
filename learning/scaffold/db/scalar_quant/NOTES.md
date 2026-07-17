# 标量量化工程参考

本文档对照 `engineering/src/algo-prod/quantization/sq.c`，描述标量量化（Scalar Quantization）的实现细节。

## 1. 量化器结构

工程实现中，SQ 量化器的核心参数存储在 `quantizer_t` 结构体中：

```c
/* ── SQ (Scalar Quantization) ── */
float global_min;  /* 全局最小值 */
float global_max;  /* 全局最大值 */
float scale;       /* 量化步长 = (max - min) / (2^sq_bits - 1) */
```

关键设计：
- `global_min` 和 `global_max` 记录训练数据的全局统计值
- `scale` 是量化步长，决定了量化精度
- 码字格式包含 8 字节头部 + 实际维度数据

## 2. 训练流程

SQ 训练阶段遍历所有训练向量，统计全局 min/max：

```c
int32_t sq_train(quantizer_t *q, int32_t n, const float *vectors) {
    // 1. 遍历所有向量，统计 min/max
    float min_val = vectors[0];
    float max_val = vectors[0];
    for (int i = 1; i < n * q->config.dims; i++) {
        if (vectors[i] < min_val) min_val = vectors[i];
        if (vectors[i] > max_val) max_val = vectors[i];
    }

    // 2. 计算缩放因子
    q->global_min = min_val;
    q->global_max = max_val;
    q->scale = (max_val - min_val) / ((1 << q->config.sq_bits) - 1);

    q->trained = true;
    return 0;
}
```

## 3. 编码实现

编码时对每个维度独立量化：

```c
int32_t sq_encode(const quantizer_t *q, const float *vector, uint8_t *code) {
    // 计算码字：每个维度量化后紧凑存储
    int32_t bits = q->config.sq_bits;  // 通常为 8
    int32_t dims = q->config.dims;

    for (int i = 0; i < dims; i++) {
        // 量化公式
        float qval = (vector[i] - q->global_min) / q->scale;
        uint8_t qcode = (uint8_t)saturate(qval, 0, (1 << bits) - 1);

        // 存储到码字
        // ... 紧凑存储逻辑
    }
    return 0;
}
```

## 4. 距离计算优化

SQ 的特点是不需要码本，距离表直接存储查询向量：

```c
int32_t sq_compute_distance_table(const quantizer_t *q,
                                   distance_metric_t metric,
                                   const float *query,
                                   float *table) {
    // SQ 的距离表就是查询向量本身（用于内积/欧氏距离计算）
    for (int i = 0; i < q->config.dims; i++) {
        table[i] = query[i];
    }
    return 0;
}
```

## 5. 模型持久化

SQ 模型仅需存储 2 个浮点数：`global_min` 和 `scale`：

```c
// 导出模型
int32_t sq_export_model(const quantizer_t *q, float *params) {
    params[0] = q->global_min;
    params[1] = q->scale;
    return 0;
}

// 导入模型
int32_t sq_load_model(quantizer_t *q, const float *params) {
    q->global_min = params[0];
    q->scale = params[1];
    q->trained = true;
    return 0;
}
```

## 6. 与其他量化方法对比

| 特性 | SQ（标量） | PQ（乘积） | LVQ（学习向量） |
|------|-----------|-----------|----------------|
| 量化粒度 | 标量 | 向量段 | 标量 |
| 码本需求 | 无 | 需要 | 可选 |
| 压缩率 | 4x (int8) | 可调 | 可调 |
| 精度 | 中等 | 较高 | 较高 |
| 训练复杂度 | 低 | 中 | 中 |

## 7. 溢出饱和处理

实际工程中，饱和截断函数确保量化值在有效范围内：

```c
static inline uint8_t saturate(float val, float min, float max) {
    if (val < min) return (uint8_t)min;
    if (val > max) return (uint8_t)max;
    return (uint8_t)roundf(val);
}
```

该函数用于处理数据分布超出训练范围的情况，避免环形溢出（wrap-around）。

## 8. 总结

标量量化是向量数据库中常用的轻量级量化方法，通过简单的缩放和截断实现 4 倍存储压缩。
