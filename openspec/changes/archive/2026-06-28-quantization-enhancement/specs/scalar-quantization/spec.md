# Scalar Quantization (SQ) 标量量化规格

## Purpose

定义 SQ 标量量化的行为规范，包括训练、编码和解码的接口契约。

## ADDED Requirements

### Requirement: SQ 量化器创建

SQ 量化器 SHALL 通过 `quantizer_create(config)` 创建，其中 `config.type == QUANTIZATION_TYPE_SQ`。

#### Scenario: 创建 8-bit SQ 量化器

- **WHEN** 调用方使用 `dims=128, type=SQ, sq_bits=8` 创建量化器
- **THEN** 量化器创建成功，`quantizer_code_size()` 返回 `8 + ceil(128*8/8) = 136` 字节
- **AND** `quantizer_is_trained()` 返回 false

#### Scenario: 创建 4-bit SQ 量化器

- **WHEN** 调用方使用 `dims=128, type=SQ, sq_bits=4` 创建量化器
- **THEN** 量化器创建成功，`quantizer_code_size()` 返回 `8 + ceil(128*4/8) = 72` 字节

### Requirement: SQ 训练阶段

SQ 量化器 SHALL 在训练阶段扫描所有向量，统计全局 min 和 max 值，计算 scale 参数。

#### Scenario: 正常训练

- **WHEN** 调用 `quantizer_train(q, n=10000, vectors)` 且 n > 0
- **THEN** 量化器内部记录全局 min 和 max
- **AND** scale = (max - min) / (2^sq_bits - 1)
- **AND** `quantizer_is_trained()` 返回 true
- **AND** `quantizer_train()` 返回 0

#### Scenario: 空训练集

- **WHEN** 调用 `quantizer_train(q, n=0, vectors=NULL)`
- **THEN** `quantizer_train()` 返回 -1
- **AND** `quantizer_is_trained()` 返回 false

### Requirement: SQ 编码

SQ 量化器 SHALL 将每个向量分量量化为 sq_bits 位的无符号整数，并存储在码字中。

#### Scenario: 8-bit 编码

- **WHEN** 量化器已训练，`dims=8, sq_bits=8`，向量 x = [0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5]
- **AND** global_min=0.0, global_max=8.0, scale=8.0/255
- **THEN** `quantizer_encode()` 产生码字 `code = [0, 48, 96, 144, 191, 239, 255, 255]`
- **AND** `quantizer_encode()` 返回 0

#### Scenario: 4-bit 编码

- **WHEN** 量化器已训练，`dims=8, sq_bits=4`，向量 x = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0]
- **AND** global_min=0.0, global_max=8.0, scale=8.0/15
- **THEN** `quantizer_encode()` 产生码字，每 2 个维度打包为 1 字节

### Requirement: SQ 解码距离计算

SQ 量化器 SHALL 支持通过 ADC (Asymmetric Distance Computation) 方式计算近似距离。

#### Scenario: 距离表计算

- **WHEN** 调用 `quantizer_compute_distance_table(q, metric, query, table)`
- **THEN** table 中存储 query 向量的原始值（dims 个 float）
- **AND** `quantizer_compute_distance_table()` 返回 0

#### Scenario: ADC 距离计算

- **WHEN** 调用 `quantizer_adc_distance(q, code, table)` 且 table 为查询向量
- **THEN** 返回 L2 距离：`sum((query[i] - (global_min + scale * code[i]))^2)`
- **AND** `quantizer_adc_distance()` 返回 float 距离值

### Requirement: SQ 码字大小计算

SQ 量化器 SHALL 正确计算码字字节大小。

#### Scenario: 8-bit 码字大小

- **WHEN** `dims=128, sq_bits=8`
- **THEN** `quantizer_code_size()` 返回 `8 + 128 = 136` 字节

#### Scenario: 4-bit 码字大小

- **WHEN** `dims=128, sq_bits=4`
- **THEN** `quantizer_code_size()` 返回 `8 + ceil(128*4/8) = 8 + 64 = 72` 字节

#### Scenario: 4-bit 奇数维度

- **WHEN** `dims=127, sq_bits=4`
- **THEN** `quantizer_code_size()` 返回 `8 + ceil(127*4/8) = 8 + 64 = 72` 字节（向上取整）

### Requirement: SQ 配置验证

`quantizer_config_validate()` SHALL 拒绝无效的 SQ 配置。

#### Scenario: 无效 bits

- **WHEN** `sq_bits` 不是 4 或 8
- **THEN** `quantizer_config_validate()` 返回 0

#### Scenario: 有效配置

- **WHEN** `dims=128, type=SQ, sq_bits=8`
- **THEN** `quantizer_config_validate()` 返回 1

### Requirement: SQ 模型导出与导入

SQ 量化器 SHALL 支持模型参数（global_min, scale）的导出和导入。

#### Scenario: 模型导出

- **WHEN** 调用 `quantizer_model_float_count(q)` 对已训练的 SQ 量化器
- **THEN** 返回 2（global_min 和 scale 两个 float）

#### Scenario: 模型导入

- **WHEN** 调用 `quantizer_create_from_model()` 传入 global_min 和 scale
- **THEN** 量化器 `quantizer_is_trained()` 返回 true
- **AND** 可直接用于编码
