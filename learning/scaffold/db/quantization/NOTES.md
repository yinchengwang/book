# 量化技术工程对照笔记

> 对照 `engineering/src/algo-prod/quantization/quantization.c` 分析量化实现

## 1. 量化器接口

```c
// quantization.h 中的核心接口
typedef enum quantization_type {
    QUANTIZATION_TYPE_NONE = 0,
    QUANTIZATION_TYPE_PQ   = 1,   // 乘积量化
    QUANTIZATION_TYPE_LVQ  = 2,   // 局部自适应量化
    QUANTIZATION_TYPE_SQ   = 3,   // 标量量化
    QUANTIZATION_TYPE_RQ   = 4,   // 残差量化
} quantization_type_t;

quantizer_t *quantizer_create(const quantizer_config_t *config);
int32_t quantizer_train(quantizer_t *quantizer, int32_t n, const float *vectors);
int32_t quantizer_encode(const quantizer_t *quantizer, const float *vector, uint8_t *code);
```

## 2. 配置结构

```c
typedef struct quantizer_config {
    int32_t dims;              // 向量维度
    quantization_type_t type;  // 量化器类型
    int32_t pq_subquantizers;  // PQ 子空间数量
    int32_t pq_bits;           // PQ 编码位数
    int32_t sq_bits;           // SQ 编码位数
} quantizer_config_t;
```

## 3. SQ 标量量化

**工程实现** (`sq.c`):
- 根据数据的 min/max 计算缩放因子
- 支持 4/8 位量化
- 编码: `code = (value - min) / scale`
- 解码: `value = min + code * scale`

**与演示程序对照**:
| 特性 | quantization.c | main.c 演示 |
|------|---------------|-------------|
| 缩放计算 | (max - min) / (2^bits - 1) | 相同 |
| 溢出处理 | clamp 到 [0, 2^bits) | 相同 |
| 训练 | 计算全局 min/max | 相同 |

## 4. PQ 乘积量化

**工程实现** (`pq.c`):
- 将 D 维向量分成 M 个子空间
- 每个子空间独立训练 K-Means 码本
- 编码: M 个子码字组合
- 距离: 预计算查表 + ADC

```c
// PQ 距离计算
float quantizer_adc_distance(const quantizer_t *quantizer, 
                            const uint8_t *code, 
                            const float *distance_table);
```

## 5. OPQ 优化

**工程实现** (`quantization.c`):
- OPQ 通过 PCA 旋转使子空间方差均衡
- 迭代优化旋转矩阵

```c
int32_t quantizer_enable_opq(quantizer_t *quantizer);
int32_t quantizer_get_opq_rotation(const quantizer_t *quantizer,
                                   float *rotation_matrix,
                                   int32_t matrix_size);
```

## 6. 量化类型对比

| 类型 | 压缩率 | 精度损失 | 适用场景 |
|------|--------|----------|----------|
| SQ | 4x | 中等 | 通用 |
| PQ | 32x (D=128,M=16,K=256) | 较高 | 大规模向量 |
| OPQ | 同 PQ | 较低 | 高精度需求 |
| AVQ | 可调 | 自适应 | 分布不均匀数据 |

## 7. 本演示与工程实现差异

| 特性 | 工程实现 | 演示程序 |
|------|----------|----------|
| 维度 | 任意 D | 固定 8 |
| 码本大小 | 2^8 = 256 | 16 (简化) |
| 距离计算 | SIMD 优化 | 朴素实现 |
| 训练算法 | K-Means++ | 简化采样 |
| OPQ 旋转 | 迭代优化 | 简化版本 |

## 8. 关键源码位置

- 量化器接口: `quantization.h`
- SQ 实现: `sq.c`
- PQ 实现: `pq.c`
- 统一量化器: `quantization.c`
- 自适应量化: `adaptive_quantization.c`
