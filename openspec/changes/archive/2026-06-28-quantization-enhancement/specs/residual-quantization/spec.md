# Residual Quantization (RabitQ) 残差量化规格

## Purpose

定义 RabitQ (Residual Binary Quantization) 量化器的行为规范，包括两级量化（PQ + 残差）的训练、编码和解码流程。

## 术语说明

| 术语 | 说明 |
|------|------|
| PQ | Product Quantization，乘积量化，第一级粗量化 |
| RQ | Residual Quantization，残差量化，第二级细量化 |
| m | 子空间数量 |
| d | 向量维度 |
| sub_dims | 每个子空间的维度数 = d / m |
| K | PQ 码本大小 = 2^pq_bits |
| rq_bits | 残差量化的位数 (1, 2, 4) |

## ADDED Requirements

### Requirement: RabitQ 量化器创建

RabitQ 量化器 SHALL 通过 `quantizer_create(config)` 创建，其中 `config.type == QUANTIZATION_TYPE_RQ`。

#### Scenario: 创建标准 RabitQ 量化器

- **WHEN** 调用方使用 `dims=128, type=RQ, rq_pq_bits=8, rq_residual_bits=1` 创建量化器
- **THEN** 量化器创建成功
- **AND** `quantizer_code_size()` 返回 `m + ceil(m × rq_bits / 8)` 字节
- **AND** `quantizer_distance_table_size()` 返回 PQ 距离表大小
- **AND** `quantizer_is_trained()` 返回 false

#### Scenario: 不同残差 bits 配置

- **WHEN** `rq_residual_bits=2`
- **THEN** 残差编码支持 4 个等级 (0,1,2,3)
- **AND** `rq_residual_bits=4`
- **THEN** 残差编码支持 16 个等级

### Requirement: RabitQ 两级量化原理

RabitQ SHALL 实现两级量化流程：

1. **第一级 PQ**：将向量分成 m 个子空间，每个子空间独立量化
2. **第二级残差量化**：对 PQ 的量化误差（残差）再次量化

#### Scenario: 两级量化编码流程

- **WHEN** 量化器已创建，向量 x = [0.3, 0.7, 1.2, 2.8, 3.5, 4.1, 5.2, 6.9]，dims=8, m=4
- **AND** 子空间划分：s0=[0.3,0.7], s1=[1.2,2.8], s2=[3.5,4.1], s3=[5.2,6.9]
- **THEN** 第一级：每个子空间找到最近的 PQ 码字，记录码字索引 pq_code[i]
- **AND** 第二级：计算残差 residual[i] = original[i] - pq_approx[i]
- **AND** 残差编码为 rq_code[i]（符号位 + 幅度位）

### Requirement: RabitQ 训练阶段

RabitQ 训练 SHALL 完成 PQ 码本训练和残差统计两个步骤。

#### Scenario: 完整训练流程

- **WHEN** 调用 `quantizer_train(q, n=10000, vectors)`
- **THEN** 第一步：对 n 个向量运行 K-Means 训练 PQ 码本
- **AND** 第二步：计算每个子空间的残差统计量（用于确定量化步长）
- **AND** `quantizer_is_trained()` 返回 true
- **AND** `quantizer_train()` 返回 0

#### Scenario: 训练样本不足

- **WHEN** n < codebook_size（小于码本大小）
- **THEN** codebook_size 自动调整为 n
- **AND** K-Means 仍能正常完成

### Requirement: RabitQ 编码

RabitQ 编码 SHALL 输出两部分码字：PQ 码（m 字节）+ 残差码（ceil(m × rq_bits / 8) 字节）。

#### Scenario: PQ 编码部分

- **WHEN** 量化器已训练，rq_pq_bits=8, m=16
- **THEN** PQ 码占用 16 字节
- **AND** 每个子空间的码字索引范围为 0-255

#### Scenario: 残差编码部分

- **WHEN** rq_residual_bits=1（单 bit 残差）
- **THEN** 每个子空间残差编码为 0（负方向）或 1（正方向）
- **AND** 残差码总大小为 ceil(m/8) 字节
- **AND** 当 rq_residual_bits=2
- **THEN** 每个子空间残差编码为 0-3 四个等级

#### Scenario: 1-bit 残差编码逻辑

- **WHEN** 子空间残差 residual > 0
- **THEN** rq_code = 1
- **AND** 子空间残差 residual <= 0
- **THEN** rq_code = 0

### Requirement: RabitQ 解码与距离计算

RabitQ SHALL 支持 ADC 方式计算近似距离。

#### Scenario: 距离表计算

- **WHEN** 调用 `quantizer_compute_distance_table(q, metric, query, table)`
- **THEN** table 中存储每个子空间到所有 PQ 码字的距离
- **AND** table 大小 = m × (2^pq_bits) floats
- **AND** `quantizer_compute_distance_table()` 返回 0

#### Scenario: ADC 距离计算

- **WHEN** 调用 `quantizer_adc_distance(q, code, table)`
- **THEN** 首先查表获取 PQ 距离：`pq_dist = table[sub × codebook_size + pq_code[sub]]`
- **AND** 然后加上残差校正距离
- **AND** 返回总距离

### Requirement: RabitQ 码字大小计算

RabitQ SHALL 正确计算完整码字的字节大小。

#### Scenario: 标准配置码字大小

- **WHEN** dims=128, m=16, rq_bits=1
- **THEN** PQ 码: 16 字节
- **AND** 残差码: ceil(16×1/8) = 2 字节
- **AND** `quantizer_code_size()` 返回 18 字节

#### Scenario: 多 bit 残差码字大小

- **WHEN** dims=128, m=16, rq_bits=4
- **THEN** PQ 码: 16 字节
- **AND** 残差码: ceil(16×4/8) = 8 字节
- **AND** `quantizer_code_size()` 返回 24 字节

### Requirement: RabitQ 配置验证

`quantizer_config_validate()` SHALL 拒绝无效的 RabitQ 配置。

#### Scenario: 无效 PQ bits

- **WHEN** rq_pq_bits 不是 4, 6, 或 8
- **THEN** `quantizer_config_validate()` 返回 0

#### Scenario: 无效残差 bits

- **WHEN** rq_residual_bits 不是 1, 2, 或 4
- **THEN** `quantizer_config_validate()` 返回 0

#### Scenario: 有效配置

- **WHEN** dims=128, m=16, rq_pq_bits=8, rq_residual_bits=1
- **AND** dims % m == 0
- **THEN** `quantizer_config_validate()` 返回 1

### Requirement: RabitQ 模型导出与导入

RabitQ SHALL 支持 PQ 码本的导出和导入。

#### Scenario: 模型导出

- **WHEN** 调用 `quantizer_model_float_count(q)` 对已训练的 RabitQ 量化器
- **THEN** 返回 PQ 码本的 float 数量：`m × codebook_size × sub_dims`
- **AND** `quantizer_export_model()` 导出完整 PQ 码本

#### Scenario: 模型导入

- **WHEN** 调用 `quantizer_create_from_model()` 传入 PQ 码本
- **THEN** 量化器 `quantizer_is_trained()` 返回 true
- **AND** 可直接用于编码

### Requirement: RabitQ 与 PQ 的精度对比

在相同码字大小下，RabitQ SHALL 比纯 PQ 达到更高精度。

#### Scenario: 精度对比条件

- **GIVEN** dims=128, m=16
- **AND** PQ(8 bits): 16 bytes, 256 选 1
- **AND** RabitQ(8+1 bits): 18 bytes, 256×2 选 1
- **WHEN** 编码相同向量
- **THEN** RabitQ 的量化误差应小于 PQ

### Requirement: RabitQ 内存布局

RabitQ 码字 SHALL 按以下顺序存储：

```
┌─────────────────────────────────────────────────────────────┐
│  码字布局 (码字大小 = m + ceil(m × rq_bits / 8) 字节)        │
├─────────────────────────────────────────────────────────────┤
│  [0..m-1]           : PQ 码 (每字节一个子空间的码字索引)      │
│  [m..m+extra-1]     : 残差码 (打包的 m × rq_bits 位)        │
└─────────────────────────────────────────────────────────────┘
```

#### Scenario: 1-bit 残差打包

- **WHEN** m=16, rq_bits=1
- **AND** 残差码: [1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0]
- **THEN** 打包为 2 字节: byte0=0xB6 (11010110), byte1=0x5A (01011010)
