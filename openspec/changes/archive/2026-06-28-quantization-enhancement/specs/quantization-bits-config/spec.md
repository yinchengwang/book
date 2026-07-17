# Quantization Bits Configuration 量化器 Bit 配置规格

## Purpose

定义 PQ、LVQ、SQ、RabitQ 等量化器的 bit 位数可配置化规范。

## ADDED Requirements

### Requirement: PQ bits 可配置

PQ 量化器 SHALL 支持可配置的 pq_bits 参数，支持 4-bit、6-bit、8-bit。

#### Scenario: 4-bit PQ 配置

- **WHEN** `pq_bits = 4` 且 `pq_subquantizers = 16`
- **THEN** `quantizer_code_size()` 返回 16 字节
- **AND** `quantizer_distance_table_size()` 返回 `16 × 16 = 256` floats
- **AND** 码本大小 = `2^4 = 16` 个码字

#### Scenario: 6-bit PQ 配置

- **WHEN** `pq_bits = 6` 且 `pq_subquantizers = 16`
- **THEN** 码本大小 = `2^6 = 64` 个码字
- **AND** `quantizer_distance_table_size()` 返回 `16 × 64 = 1024` floats

#### Scenario: 8-bit PQ 配置 (默认)

- **WHEN** `pq_bits = 8` 且 `pq_subquantizers = 16`
- **THEN** 码本大小 = `2^8 = 256` 个码字
- **AND** `quantizer_distance_table_size()` 返回 `16 × 256 = 4096` floats

### Requirement: PQ bits 默认值

`quantizer_config_init()` SHALL 为 PQ 设置合理的默认值。

#### Scenario: PQ 默认 bits

- **WHEN** 创建 PQ 量化器时未显式设置 pq_bits
- **THEN** `pq_bits` 默认为 8
- **AND** `pq_subquantizers` 自动设置为 dims 的因数（优先 8 或 dims 本身）

#### Scenario: PQ subquantizers 自动计算

- **WHEN** dims=128 且未指定 subquantizers
- **THEN** `pq_subquantizers` 尝试 128, 64, 32, 16, 8, 4, 2, 1 中第一个整除 dims 的值
- **AND** 如果 dims=128，则 `pq_subquantizers = 16`（因为 128 % 16 == 0）

### Requirement: LVQ bits 验证

LVQ 量化器 SHALL 仅支持 4-bit 和 8-bit 配置。

#### Scenario: 4-bit LVQ

- **WHEN** `lvq_bits = 4`
- **THEN** `quantizer_code_size()` 返回 `8 + ceil(dims × 4 / 16)` 字节
- **AND** 4-bit 模式采用半字节打包

#### Scenario: 8-bit LVQ

- **WHEN** `lvq_bits = 8`
- **THEN** `quantizer_code_size()` 返回 `8 + dims` 字节
- **AND** 8-bit 模式每维度一字节

#### Scenario: 无效 LVQ bits

- **WHEN** `lvq_bits` 不是 4 或 8
- **THEN** `quantizer_config_validate()` 返回 0

### Requirement: SQ bits 验证

SQ 量化器 SHALL 支持 4-bit 和 8-bit 配置。

#### Scenario: SQ 4-bit 码字大小

- **WHEN** `dims=128, sq_bits=4`
- **THEN** `quantizer_code_size()` 返回 `8 + 64 = 72` 字节

#### Scenario: SQ 8-bit 码字大小

- **WHEN** `dims=128, sq_bits=8`
- **THEN** `quantizer_code_size()` 返回 `8 + 128 = 136` 字节

### Requirement: RabitQ bits 验证

RabitQ SHALL 支持可配置的 PQ bits 和残差 bits。

#### Scenario: RabitQ PQ bits

- **WHEN** `rq_pq_bits = 8` 且 `rq_residual_bits = 1`
- **THEN** PQ 码本大小 = `2^8 = 256`
- **AND** 每子空间距离表大小 = 256

#### Scenario: RabitQ 残差 bits

- **WHEN** `rq_residual_bits = 1`
- **THEN** 残差编码为二进制（0/1）
- **AND** `rq_residual_bits = 2`
- **THEN** 残差编码为四进制（0,1,2,3）
- **AND** `rq_residual_bits = 4`
- **THEN** 残差编码为十六进制（0-15）

### Requirement: 量化器配置结构验证

`quantizer_config_validate()` SHALL 对每种量化类型执行正确的参数验证。

#### Scenario: PQ 配置验证

- **WHEN** `type = PQ`
- **THEN** 验证 `dims > 0`
- **AND** `pq_subquantizers > 0`
- **AND** `dims % pq_subquantizers == 0`
- **AND** `pq_bits ∈ {4, 6, 8}`

#### Scenario: SQ 配置验证

- **WHEN** `type = SQ`
- **THEN** 验证 `dims > 0`
- **AND** `sq_bits ∈ {4, 8}`

#### Scenario: RabitQ 配置验证

- **WHEN** `type = RQ`
- **THEN** 验证 `dims > 0`
- **AND** `pq_subquantizers > 0`
- **AND** `dims % pq_subquantizers == 0`
- **AND** `rq_pq_bits ∈ {4, 6, 8}`
- **AND** `rq_residual_bits ∈ {1, 2, 4}`

### Requirement: Bits 配置对码字大小的影响

码字大小 SHALL 正确反映 bits 配置的变化。

#### Scenario: PQ 码字大小对比

- **WHEN** dims=128, m=16
- **AND** `pq_bits=4`: code_size=16 bytes, dist_table=16×16=256 floats
- **AND** `pq_bits=8`: code_size=16 bytes, dist_table=16×256=4096 floats
- **THEN** 8-bit 配置的距离表是 4-bit 的 16 倍

#### Scenario: SQ 码字大小对比

- **WHEN** dims=128
- **AND** `sq_bits=4`: code_size=8+64=72 bytes（压缩比 128:72 ≈ 1.78x）
- **AND** `sq_bits=8`: code_size=8+128=136 bytes（压缩比 128:136 ≈ 0.94x，不压缩）

### Requirement: 不同量化类型的压缩比对比

相同维度下，不同量化器应提供不同的压缩比。

#### Scenario: 压缩比对比表

- **GIVEN** dims=128, m=16
- **WHEN** 比较各量化器的码字大小
- **THEN** PQ(8 bits): 16 bytes (8x 压缩)
- **AND** SQ(8 bits): 136 bytes (几乎不压缩)
- **AND** SQ(4 bits): 72 bytes (1.78x 压缩)
- **AND** LVQ(8 bits): 136 bytes
- **AND** LVQ(4 bits): 72 bytes
- **AND** RabitQ(8+1 bits): 18 bytes (7.1x 压缩)

### Requirement: 配置参数一致性

量化器配置 SHALL 在创建后保持一致性。

#### Scenario: 配置与量化器绑定

- **WHEN** `quantizer_create(config)` 成功
- **THEN** 量化器内部存储的配置与输入 config 一致
- **AND** 查询 `quantizer_config` 返回相同的值

#### Scenario: bits 参数对距离表大小的影响

- **WHEN** `quantizer_compute_distance_table()` 被调用
- **THEN** table 的大小与 bits 配置一致
- **AND** 调用方应预先分配足够空间：`pq_bits → 2^pq_bits × m floats`
