# 向量基础对照笔记

> 对照 `engineering/src/algo-prod/distance/distance.c` 分析向量距离计算实现细节

## 1. 距离度量类型

### L2 平方距离（推荐使用）

```c
// distance.c 中的实现
static float distance_l2sqr_scalar(const float *lhs, const float *rhs, int32_t dims) {
    float result = 0.0f;
    for (int i = 0; i < dims; ++i) {
        float diff = lhs[i] - rhs[i];
        result += diff * diff;  // 累加平方差
    }
    return result;  // 返回 L2 平方，不开方
}
```

**关键优化点：**
- 返回平方距离（L2 squared），省去 `sqrt()` 开方操作
- 在 KNN 搜索中，比较平方距离即可判断远近，无需开方
- 负数输入的处理：`diff = lhs[i] - rhs[i]` 不会出现溢出问题

### 余弦距离

```c
// distance.c 中的实现
static float distance_cosine_scalar(const float *lhs, const float *rhs, int32_t dims) {
    float dot = 0.0f;
    float lhs_norm = 0.0f;
    float rhs_norm = 0.0f;

    // 一次遍历同时计算点积和模长
    for (int i = 0; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    // 特殊情况处理（零向量）
    if (lhs_norm <= 0.0f && rhs_norm <= 0.0f) return 0.0f;
    if (lhs_norm <= 0.0f || rhs_norm <= 0.0f) return 1.0f;

    return 1.0f - dot / (sqrtf(lhs_norm) * sqrtf(rhs_norm));
}
```

**优化点：**
- 一次性遍历计算三个值，减少内存访问
- 零向量特殊处理，避免除零

### 内积（点积）

```c
// distance.c 中返回负值
static float distance_inner_product_scalar(const float *lhs, const float *rhs, int32_t dims) {
    float dot = 0.0f;
    for (int i = 0; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
    }
    return -dot;  // 返回负值，用于 ANN 搜索时越大越好
}
```

**设计原因：**
- 向量数据库的 ANN 搜索通常要找"最近"的向量
- 内积越大表示越相似
- 返回负值后，内积大的反而距离小，便于统一排序

## 2. SIMD 优化策略

### 平台检测

```c
#if defined(__ARM_NEON) || defined(_M_ARM64) || defined(_M_ARM64EC)
    #include <arm_neon.h>
    #define DISTANCE_USE_NEON 1
#elif defined(__AVX__) || defined(__AVX2__)
    #include <immintrin.h>
    #define DISTANCE_USE_AVX 1
#elif defined(__SSE__) || defined(_M_X64)
    #include <xmmintrin.h>
    #define DISTANCE_USE_SSE 1
#endif
```

### AVX 批量计算示例

```c
// AVX 一次处理 8 个 float
for (i = 0; i <= dims - 8; i += 8) {
    __m256 lhs_vec = _mm256_loadu_ps(lhs + i);
    __m256 rhs_vec = _mm256_loadu_ps(rhs + i);
    __m256 diff = _mm256_sub_ps(lhs_vec, rhs_vec);
    acc = _mm256_add_ps(acc, _mm256_mul_ps(diff, diff));
}
```

## 3. 与本演示程序的区别

| 特性 | distance.c | main.c 演示 |
|------|------------|-------------|
| 距离类型 | L2/余弦/内积/汉明 | L2/余弦/点积 |
| SIMD | NEON/AVX/SSE | 无 |
| 批量计算 | 支持 4 向量批量 | 无 |
| 内存布局 | 行主序（vectors[id*dims]） | 独立向量对象 |
| 零向量处理 | 特殊边界检查 | 无 |

## 4. 总结

向量距离计算的核心要点：
1. **L2 平方距离**省去开方操作，在排序比较时等效
2. **余弦距离**衡量方向相似性，适合文本嵌入
3. **内积返回负值**，统一 ANN 搜索的排序逻辑
4. **SIMD 优化**是高性能向量检索的关键
