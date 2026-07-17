# OPQ 优化乘积量化

## Purpose

实现 OPQ (Optimized Product Quantization) 量化器，通过 PCA 旋转优化子空间划分，提升量化精度。

## Requirements

### Requirement: OPQ 量化器操作

OPQ 量化器 SHALL 支持以下操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `opq_quantizer_t *opq_create(int dims, int m, int bits)` | 创建 OPQ 量化器 |
| 训练 | `int opq_train(opq_quantizer_t *opq, int n, const float *vectors)` | 训练旋转矩阵和码书 |
| 编码 | `int opq_encode(opq_quantizer_t *opq, const float *vector, uint8_t *code)` | 编码单个向量 |
| 批量编码 | `int opq_encode_batch(opq_quantizer_t *opq, int n, const float *vectors, uint8_t *codes)` | 批量编码 |
| 距离表 | `int opq_compute_distance_table(opq_quantizer_t *opq, distance_metric_t metric, const float *query, float *table)` | 计算距离表 |
| ADC 距离 | `float opq_adc_distance(opq_quantizer_t *opq, const uint8_t *code, const float *table)` | ADC 距离计算 |
| 解码 | `int opq_decode(opq_quantizer_t *opq, const uint8_t *code, float *vector)` | 解码向量 |
| 销毁 | `void opq_destroy(opq_quantizer_t *opq)` | 释放资源 |

### Requirement: OPQ 核心数据结构

```c
// OPQ 量化器
typedef struct opq_quantizer {
    int dims;                      // 向量维度
    int m;                         // 子空间数量
    int ks;                        // 每个子空间的码书大小 (2^bits)
    int bits;                      // 每子空间比特数

    // PCA 旋转矩阵
    float *rotation_matrix;        // [dims, dims] 或 [dims, m] 压缩形式
    float *rotation_transposed;    // 转置版本用于解码

    // PQ 码书
    float *codebooks;               // [m, ks, m] 或展平形式

    // 辅助数据
    float *workspace;              // 临时工作空间
    float *rotated_vector;          // 旋转后的向量
    int is_trained;                 // 是否已训练
} opq_quantizer_t;
```

### Requirement: OPQ vs PQ 对比

```
PQ (普通乘积量化):
┌────────────────────────────────────────────────────────────┐
│                                                            │
│  原始向量: [d0 d1 d2 d3 d4 d5 d6 d7]                       │
│                   ↓                                        │
│           按顺序切分                                        │
│                   ↓                                        │
│           [d0 d1] | [d2 d3] | [d4 d5] | [d6 d7]             │
│                   ↓                                        │
│           各子空间独立量化                                   │
│                                                            │
│  问题: 如果 d0,d1 变化范围大，8 比特不够分配               │
│                                                            │
└────────────────────────────────────────────────────────────┘

OPQ (优化乘积量化):
┌────────────────────────────────────────────────────────────┐
│                                                            │
│  原始向量: [d0 d1 d2 d3 d4 d5 d6 d7]                       │
│                   ↓                                        │
│           PCA 旋转 (使方差均衡)                             │
│                   ↓                                        │
│  [d'0 d'1 d'2 d'3 d'4 d'5 d'6 d'7]                        │
│                   ↓                                        │
│           旋转后再切分                                      │
│                   ↓                                        │
│  [d'0 d'1] | [d'2 d'3] | [d'4 d'5] | [d'6 d'7]            │
│                   ↓                                        │
│           各子空间方差均衡，量化更精准                       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Requirement: OPQ 训练流程

```
1. 数据预处理：归一化（可选）
2. 计算协方差矩阵
3. PCA 分解：提取前 m 个主成分
4. 计算旋转矩阵 R
5. 旋转数据：X_rotated = X × R
6. 对旋转后的数据执行 PQ 训练
7. 保存旋转矩阵和 PQ 码书
```

### Requirement: 量化精度指标

| 指标 | PQ | OPQ | 改进 |
|------|-----|-----|------|
| MSE (均方误差) | 较高 | 较低 | ⬇️ 20-30% |
| 重构误差 | 较大 | 较小 | ⬇️ 显著 |
| 召回率 | 基准 | 更高 | ⬆️ 5-15% |

#### Scenario: 高维向量量化

- **WHEN** 用户有 256 维向量
- **AND** 使用 OPQ(16, 8)（16 个子空间，每子空间 8 比特）
- **THEN** 旋转后各子空间方差更均衡
- **AND** 量化误差比 PQ 降低约 25%
