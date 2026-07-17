# 量化算法扩展设计文档

## Context

### 背景

当前量化系统 (`src/algo/quantization/`) 存在以下限制：

1. **类型单一**：仅支持 PQ 和 LVQ
2. **配置不灵活**：PQ 的 bits 硬编码为 8
3. **扩展困难**：新增量化类型需要修改多个文件

### 现有架构

```
┌─────────────────────────────────────────────────────┐
│              quantization.h (统一接口)               │
├─────────────────────────────────────────────────────┤
│  quantizer_create(config) → 工厂方法                  │
│  quantizer_train(q, n, vectors)                     │
│  quantizer_encode(q, vector, code)                  │
│  quantizer_compute_distance_table(q, query, table)  │
│  quantizer_adc_distance(q, code, table)              │
└─────────────────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
    ┌────────┐      ┌────────┐      ┌────────┐
    │  PQ    │      │  LVQ   │      │  SQ    │ ← 待新增
    └────────┘      └────────┘      └────────┘
                                        │
                                        ▼
                                  ┌────────┐
                                  │ RabitQ │ ← 待新增
                                  └────────┘
```

### 约束

- 纯 C 实现，无外部依赖
- 通过 `quantization_type_t` 枚举区分量化器类型
- `quantizer_t` 结构体统一管理所有量化器状态
- DiskANN 通过统一接口调用量化器

## Goals / Non-Goals

**Goals:**
- 新增 SQ (Scalar Quantization) 量化器
- 新增 RabitQ (Residual Binary Quantization) 量化器
- 扩展 PQ 支持 4/6/8 bits 可配置
- 保持接口向后兼容，不破坏现有代码

**Non-Goals:**
- 不追求与旧版本索引文件的完全兼容
- 不实现多级量化器组合（复杂度太高）
- 不实现 OPQ (Optimized PQ) - 旋转优化（可在后续扩展）

## Decisions

### Decision 1: 统一量化器配置结构

**选项 A: 在现有 `quantizer_config_t` 上扩展字段**

```c
typedef struct quantizer_config {
    int32_t dims;
    quantization_type_t type;
    int32_t pq_subquantizers;
    int32_t pq_bits;           // 新增
    int32_t lvq_bits;          // 已有
    int32_t sq_bits;           // 新增
    int32_t rq_pq_bits;        // 新增
    int32_t rq_residual_bits;  // 新增
} quantizer_config_t;
```

**选项 B: 每种量化器独立配置结构，通过 union 组合**

```c
typedef struct {
    quantization_type_t type;
    union {
        pq_config_t pq;
        lvq_config_t lvq;
        sq_config_t sq;
        rq_config_t rq;
    };
} quantizer_config_t;
```

**选择: 选项 A**

原因：配置简单直观，每种量化器只关注自己的参数，未使用的字段填 0 即可。

### Decision 2: SQ 的统计方式

**选项 A: 全局 min/max（所有向量一起统计）**

- 优点：编码简单，解码快
- 缺点：精度较低

**选项 B: 每个子空间独立 min/max（类似 PQ 的子空间划分）**

- 优点：精度更高
- 缺点：需要存储每个子空间的 min/max

**选择: 选项 A**

原因：SQ 的设计初衷是简单高效，全局统计足够满足需求。每个子空间的优化可以留给 OPQ 实现。

### Decision 3: RabitQ 的残差量化方案

**选项 A: 固定 1-bit 残差（正/负方向）**

```
residual = (original - pq_approx)
code = (residual > 0) ? 1 : 0
```

**选项 B: 多级残差量化（1/2/4 bits）**

```
residual_code = round(|residual| / step)  // 0, 1, 2, 3...
```

**选择: 选项 B**

原因：支持多种残差精度配置，与 PQ 的 bits 配置保持一致性。

### Decision 4: 文件组织

**每个量化器独立文件**：

```
src/algo/quantization/
├── quantization.h           ← 统一接口 + 枚举 + 配置
├── quantization.c          ← 统一调度器（工厂模式）
├── quantization_private.h  ← 内部结构体
├── pq.h / pq.c           ← 乘积量化
├── lvq.h / lvq.c         ← 局部自适应量化
├── sq.h / sq.c           ← 标量量化
└── rq.h / rq.c          ← 残差量化
```

**选择理由**：与现有架构一致（PQ/LVQ 已经是分文件），新增 SQ/RabitQ 遵循相同模式。

### Decision 5: DiskANN 集成

**目标**：让 DiskANN 能够使用新的量化器（SQ/RabitQ）

需要修改的文件：

| 文件 | 修改内容 |
|------|---------|
| `diskann.h` | 添加量化类型字段到 `diskann_quantization_params_t` |
| `diskann_quantization.c` | 添加 SQ/RabitQ 的编码分支 |
| `diskann_meta_page_t` | 扩展元信息页支持新量化类型标识 |

**量化类型标识**（元信息页扩展）：

```c
// 元信息页中新增字段
int32_t quantizer_type;      // 0=NONE, 1=PQ, 2=LVQ, 3=SQ, 4=RQ
```

## 量化器详细设计

### 1. SQ (Scalar Quantization) 设计

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SQ 量化器原理                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Step 1: 训练阶段 - 全局统计                                         │
│  ─────────────────────────────────────────────────────────────────   │
│                                                                      │
│  遍历所有训练向量，记录全局:                                           │
│    global_min = min(all_vector_dims)                                │
│    global_max = max(all_vector_dims)                                │
│    scale = (global_max - global_min) / (2^sq_bits - 1)            │
│                                                                      │
│  存储: global_min, scale (两个 float)                               │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Step 2: 编码阶段                                                    │
│  ─────────────────                                                   │
│                                                                      │
│  对每个向量分量:                                                     │
│    code[i] = round((x[i] - global_min) / scale)                    │
│    code[i] = clamp(code[i], 0, 2^sq_bits - 1)                      │
│                                                                      │
│  码字布局:                                                          │
│  ┌────────┬──────────────────────────────────────────┐             │
│  │ scale  │         quantized dimensions            │             │
│  │ (4B)   │         (dims × sq_bits bits)            │             │
│  ├────────┴──────────────────────────────────────────┤             │
│  │ global_min (4B)                                    │             │
│  └───────────────────────────────────────────────────┘             │
│                                                                      │
│  码字大小: 8 + ceil(dims × sq_bits / 8) 字节                      │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Step 3: 距离计算 (ADC)                                             │
│  ─────────────────────────                                           │
│                                                                      │
│  compute_distance_table: 将查询向量原样存入 table                      │
│                                                                      │
│  adc_distance:                                                       │
│    for each dimension i:                                            │
│      x_approx = global_min + scale × code[i]                         │
│      dist += (query[i] - x_approx)²                                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

**与 LVQ 的区别**：

| 特性 | SQ | LVQ |
|------|-----|-----|
| 统计范围 | 全局 | 每向量独立 |
| 码字头部 | scale + global_min (固定 8B) | scale + bias (每向量 8B) |
| 适用场景 | 批量索引、低精度 | 高精度需求 |

### 2. RabitQ (Residual Binary Quantization) 设计

```
┌─────────────────────────────────────────────────────────────────────┐
│                     RabitQ 量化器原理                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  核心思想: 两级量化                                                   │
│  ─────────────                                                       │
│  第一级: PQ 粗量化 (用 256 个码字近似每个子空间)                      │
│  第二级: 残差细量化 (补偿 PQ 的量化误差)                              │
│                                                                      │
│  效果: 用 ~12.5% 的额外存储换取 40-50% 的精度提升                    │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Step 1: 训练阶段                                                    │
│  ─────────────────                                                   │
│                                                                      │
│  1.1 PQ 训练 (与标准 PQ 相同)                                        │
│      - 将向量分成 m 个子空间                                         │
│      - 每个子空间运行 K-Means，K = 2^pq_bits                         │
│      - 得到码本: codebooks[sub][k]                                   │
│                                                                      │
│  1.2 残差统计                                                        │
│      - 对每个子空间，计算残差的统计量                                 │
│      - residual[sub] = original - pq_approx[sub]                   │
│      - 记录每个子空间残差的标准差或分位数                              │
│      - 用于确定残差的量化步长                                         │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Step 2: 编码阶段                                                    │
│  ─────────────────                                                   │
│                                                                      │
│  给定向量 x = [x₀, x₁, ..., x_{d-1}]                               │
│                                                                      │
│  2.1 PQ 编码:                                                       │
│      for each sub-space s (含 sub_dims 个分量):                      │
│        sub_vec = x[s × sub_dims : (s+1) × sub_dims]                │
│        pq_code[s] = argmin_k ||sub_vec - codebooks[s][k]||²        │
│        pq_approx = codebooks[s][pq_code[s]]                        │
│                                                                      │
│  2.2 残差编码:                                                      │
│      for each sub-space s:                                         │
│        residual = original_sub_vec - pq_approx                      │
│        rq_code[s] = encode_residual(residual, step[s])              │
│        // rq_code 是 rq_bits 位整数                                  │
│                                                                      │
│  码字布局:                                                          │
│  ┌────────────────────────────────────────────────────────┐        │
│  │  PQ 码 (m bytes)  │  残差码 (m × rq_bits bits)  │      │
│  │  [pq_0, pq_1, ..., pq_{m-1}]                       │      │
│  │                                                    │      │
│  │  每个 pq_i 是 8 bits (0-255)                       │      │
│  └────────────────────────────────────────────────────────┘        │
│                                                                      │
│  码字大小: m + ceil(m × rq_bits / 8) 字节                         │
│                                                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Step 3: 解码阶段                                                    │
│  ─────────────────                                                   │
│                                                                      │
│  给定码字 (pq_code, rq_code)                                        │
│                                                                      │
│  3.1 PQ 解码:                                                       │
│      for each sub-space s:                                          │
│        approx = codebooks[s][pq_code[s]]                            │
│                                                                      │
│  3.2 残差补偿:                                                      │
│      for each sub-space s:                                          │
│        direction = (rq_code[s] & (1 << (rq_bits-1))) ? +1 : -1     │
│        magnitude = rq_code[s] & ((1 << (rq_bits-1)) - 1)          │
│        residual_approx = direction × magnitude × step[s]           │
│        final_approx = approx + residual_approx                      │
│                                                                      │
│  Step 4: 距离计算 (ADC)                                             │
│  ─────────────────────────                                           │
│                                                                      │
│  4.1 预计算 PQ 距离表:                                              │
│      for each sub-space s:                                          │
│        for k in [0, 2^pq_bits):                                     │
│          dist_table[s][k] = ||query_sub[s] - codebooks[s][k]||²   │
│                                                                      │
│  4.2 预计算残差距离表 (可选优化):                                    │
│      如果 rq_bits > 1，需要存储每个子空间的残差距离补偿               │
│                                                                      │
│  4.3 ADC 距离:                                                      │
│      total_dist = 0                                                 │
│      for each sub-space s:                                         │
│        total_dist += dist_table[s][pq_code[s]]                     │
│        if rq_residual_bits > 0:                                    │
│          total_dist += residual_correction[s][rq_code[s]]          │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3. PQ Bits 扩展设计

现有 PQ 已支持可变 bits，只需要：

1. 在 `quantizer_config_t` 中添加 `pq_bits` 字段
2. 在 `quantizer_config_init()` 中设置默认值
3. 在 `pq_init()` 中使用配置的 bits 而非硬编码

```c
// src/algo/quantization/pq.c 修改
int32_t pq_init(quantizer_t *q)
{
    q->subvector_dims    = q->config.dims / q->config.pq_subquantizers;
    q->max_codebook_size = 1 << q->config.pq_bits;  // 2^bits
    q->codebook_size     = q->max_codebook_size;
    // ...
}
```

### 4. 接口扩展

```
┌─────────────────────────────────────────────────────────────────────┐
│                        量化器工厂方法                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  quantizer_create(config)                                            │
│  ├── type == PQ   → pq_init(q)                                     │
│  ├── type == LVQ  → lvq_init(q)                                    │
│  ├── type == SQ   → sq_init(q)                                     │
│  └── type == RQ   → rq_init(q)                                     │
│                                                                      │
│  quantizer_train(q, n, vectors)                                      │
│  ├── type == PQ   → pq_train(q, n, vectors)                        │
│  ├── type == LVQ  → NOP (trained = true)                          │
│  ├── type == SQ   → sq_train(q, n, vectors)  ← 全局统计           │
│  └── type == RQ   → rq_train(q, n, vectors)  ← PQ+残差训练        │
│                                                                      │
│  quantizer_encode(q, vector, code)                                   │
│  ├── type == PQ   → pq_encode(q, vector, code)                     │
│  ├── type == LVQ  → lvq_encode(q, vector, code)                   │
│  ├── type == SQ   → sq_encode(q, vector, code)                     │
│  └── type == RQ   → rq_encode(q, vector, code)                    │
│                                                                      │
│  quantizer_adc_distance(q, code, table)                              │
│  ├── type == PQ   → pq_adc_distance(q, code, table)                │
│  ├── type == LVQ  → lvq_adc_distance(q, code, table)               │
│  ├── type == SQ   → sq_adc_distance(q, code, table)                │
│  └── type == RQ   → rq_adc_distance(q, code, table)                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## Risks / Trade-offs

| Risk | Description | Mitigation |
|------|-------------|------------|
| SQ 精度损失大 | 全局 min/max 导致极端值量化误差大 | SQ 主要用于超大规模初筛，配合重排序使用 |
| RabitQ 内存开销 | 需要存储 PQ 码本 + 残差参数 | 残差 bits 较小 (1-4)，额外开销可控 |
| 多量化器增加复杂度 | 代码分支增多 | 统一工厂模式，屏蔽实现差异 |

## Open Questions

1. **SQ 的距离表优化**: 当前设计将查询向量原样存入距离表，是否需要针对 SQ 优化为预计算每个量化值到原始值的距离表？

2. **RabitQ 残差 bits 默认值**: 建议默认使用 1-bit 残差（精度/压缩比最佳平衡），还是 2-bit？

3. **现有 PQ 测试覆盖**: 需要确保修改后 PQ 的 4/6/8 bits 配置都有测试覆盖。
