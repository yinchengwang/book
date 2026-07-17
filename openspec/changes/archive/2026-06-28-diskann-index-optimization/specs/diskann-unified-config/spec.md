# DiskANN 统一配置规格

## Purpose

定义 DiskANN 统一配置结构的行为规范。

## ADDED Requirements

### Requirement: 统一配置入口

DiskANN SHALL 通过 `diskann_config_t` 统一配置入口。

```c
/* 量化配置 */
typedef struct diskann_quantization_config {
    bool enabled;
    quantization_type_t type;  // PQ/SQ/LVQ/RQ
    int32_t subquantizers;    // m: 子空间数
    int32_t bits;              // bits: 4/6/8
} diskann_quantization_config_t;

/* Merge-Vamana 配置 */
typedef struct diskann_merge_config {
    bool enabled;
    int32_t partition_method;
    int32_t partition_count;
    float overlap_ratio;
} diskann_merge_config_t;

/* SPANN 配置 */
typedef struct diskann_spann_config {
    bool enabled;
    int32_t max_partitions;
    int32_t min_partition_size;
    int32_t search_partitions;
} diskann_spann_config_t;

/* FreshDiskANN 配置 */
typedef struct diskann_fresh_config {
    bool enabled;
    int32_t fresh_capacity;
    int32_t merge_threshold;
} diskann_fresh_config_t;

/* 存储配置 */
typedef struct diskann_storage_config {
    int32_t page_size;
    int32_t frozen_point_count;
} diskann_storage_config_t;

/* 统一配置入口 */
typedef struct diskann_config {
    int32_t dims;
    int32_t index_size;
    int32_t build_search_list_size;
    distance_metric_t metric;

    diskann_quantization_config_t quantization;
    diskann_merge_config_t merge;
    diskann_spann_config_t spann;
    diskann_fresh_config_t fresh;
    diskann_storage_config_t storage;
} diskann_config_t;
```

#### Scenario: 创建默认配置

- **WHEN** 调用 `diskann_config_default(dims, metric)`
- **THEN** 返回包含默认值的配置结构
- **AND** `enabled` 相关字段均为 `false`

#### Scenario: 创建完整配置

- **WHEN** 调用 `diskann_config_create(...)` 并传入所有参数
- **THEN** 返回填充了所有值的配置结构
- **AND** 所有子配置结构独立存在

### Requirement: 配置验证

`diskann_config_validate()` SHALL 验证所有子配置。

#### Scenario: 完整验证

- **WHEN** 调用 `diskann_config_validate(config)`
- **THEN** 依次验证：
  - 基础参数（dims > 0）
  - 量化配置（如果 enabled）
  - Merge 配置（如果 enabled）
  - SPANN 配置（如果 enabled）
  - Fresh 配置（如果 enabled）
- **AND** 所有验证通过返回 1

#### Scenario: 部分启用

- **WHEN** 只有 `quantization.enabled = true`
- **THEN** 只验证量化配置
- **AND** 其他子配置跳过验证

### Requirement: 配置初始化

`diskann_config_init()` SHALL 初始化默认值。

#### Scenario: 基础参数默认值

- **WHEN** 未设置基础参数
- **THEN** `index_size = 32`
- **AND** `build_search_list_size = 48`

#### Scenario: 量化配置默认值

- **WHEN** `quantization.enabled = true` 但未设置其他参数
- **THEN** `quantization.type = QUANTIZATION_TYPE_PQ`
- **AND** `quantization.bits = 8`
- **AND** `quantization.subquantizers` 自动计算

#### Scenario: 存储配置默认值

- **WHEN** 未设置存储参数
- **THEN** `storage.page_size = 4096`
- **AND** `storage.frozen_point_count = 4`

### Requirement: 配置持久化

配置 SHALL 支持持久化。

#### Scenario: 配置保存

- **WHEN** 调用 `diskann_config_save(config, path)`
- **THEN** 将配置写入 JSON 或二进制文件

#### Scenario: 配置加载

- **WHEN** 调用 `diskann_config_load(path)`
- **THEN** 从文件读取配置
- **AND** 返回完整的 `diskann_config_t`

### Requirement: 子配置独立验证

每个子配置 SHALL 支持独立验证。

#### Scenario: 量化配置验证

- **WHEN** 调用 `diskann_quantization_config_validate(config)`
- **THEN** 验证 `type` 是否有效
- **AND** 验证 `bits` 是否在允许范围

#### Scenario: Merge 配置验证

- **WHEN** 调用 `diskann_merge_config_validate(config)`
- **THEN** 验证 `partition_count >= 2`
- **AND** 验证 `overlap_ratio` 在 [0, 1) 范围

#### Scenario: SPANN 配置验证

- **WHEN** 调用 `diskann_spann_config_validate(config)`
- **THEN** 验证 `max_partitions >= 1`
- **AND** 验证 `search_partitions <= max_partitions`

#### Scenario: Fresh 配置验证

- **WHEN** 调用 `diskann_fresh_config_validate(config)`
- **THEN** 验证 `fresh_capacity > 0`
- **AND** 验证 `merge_threshold <= fresh_capacity`

### Requirement: 配置克隆

配置 SHALL 支持克隆。

#### Scenario: 深拷贝

- **WHEN** 调用 `diskann_config_clone(config)`
- **THEN** 返回配置的完整副本
- **AND** 修改副本不影响原配置
