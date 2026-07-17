# SPANN 分区层次索引规格

## Purpose

定义 SPANN (SParse ANN) 的行为规范，支持分区索引减少搜索范围。

## 术语说明

| 术语 | 说明 |
|------|------|
| Partition | 分区，向量空间的有序划分 |
| Partition Center | 分区中心点 |
| Partition Radius | 分区半径 |
| Routing | 路由，确定查询应访问哪些分区 |

## ADDED Requirements

### Requirement: SPANN 配置

SPANN SHALL 通过独立的 `diskann_spann_config_t` 配置。

```c
typedef struct diskann_spann_config {
    bool enabled;                    // 是否启用 SPANN
    int32_t max_partitions;        // 最大分区数
    int32_t min_partition_size;    // 每分区最小向量数
    int32_t search_partitions;     // 搜索时最多访问分区数
} diskann_spann_config_t;
```

#### Scenario: 默认配置

- **WHEN** 创建配置时未设置任何参数
- **THEN** `enabled = false`
- **AND** `max_partitions = 128`
- **AND** `min_partition_size = 10000`
- **AND** `search_partitions = 8`

#### Scenario: 启用 SPANN

- **WHEN** `enabled = true`
- **THEN** 系统启用分区索引模式
- **AND** `max_partitions >= 1`

### Requirement: 分区构建

索引 SHALL 支持分区构建。

#### Scenario: 分区元数据

- **WHEN** SPANN 模式启用
- **THEN** 每个分区存储元数据：
  - 分区 ID
  - 分区中心点坐标
  - 分区半径
  - 向量数量
  - 起始偏移

#### Scenario: 分区大小约束

- **WHEN** `min_partition_size = 10000`
- **THEN** 每个分区的向量数量 >= 10000
- **AND** 最后一个分区可能小于此值

#### Scenario: 最大分区数

- **WHEN** `max_partitions = 128`
- **AND** 数据量可以产生更多分区
- **THEN** 实际分区数限制为 128

### Requirement: 分区选择

搜索时 SHALL 选择相关分区。

#### Scenario: 分区选择算法

- **WHEN** 搜索查询向量 q
- **AND** `search_partitions = 8`
- **THEN** 计算 q 到所有分区的距离
- **AND** 选择距离最近的 8 个分区

#### Scenario: 分区搜索

- **WHEN** 选择了 8 个分区
- **THEN** 仅在这 8 个分区中进行图搜索
- **AND** 其他分区跳过，不访问

#### Scenario: 搜索结果合并

- **WHEN** 多个分区返回候选结果
- **THEN** 合并所有候选并按距离排序
- **AND** 返回全局 top-k 结果

### Requirement: 分区元数据持久化

分区元数据 SHALL 支持持久化。

#### Scenario: 元数据保存

- **WHEN** 调用 `diskann_index_save()`
- **THEN** 分区元数据与主索引一起保存
- **AND** 元数据大小应尽量小（轻量级）

#### Scenario: 元数据加载

- **WHEN** 调用 `diskann_index_load()`
- **THEN** 分区元数据一起加载
- **AND** 重建分区路由结构

### Requirement: 配置验证

`diskann_spann_config_validate()` SHALL 验证配置有效性。

#### Scenario: 无效最大分区数

- **WHEN** `max_partitions < 1`
- **THEN** `validate()` 返回 0

#### Scenario: 无效搜索分区数

- **WHEN** `search_partitions > max_partitions`
- **THEN** `validate()` 返回 0

#### Scenario: 有效配置

- **WHEN** `max_partitions = 64, search_partitions = 4`
- **THEN** `validate()` 返回 1

### Requirement: 召回率保证

SPANN SHALL 尽可能保证召回率。

#### Scenario: 召回率基线

- **GIVEN** 相同数据分别用全图搜索和 SPANN 搜索
- **WHEN** 比较搜索召回率
- **THEN** SPANN 召回率应达到全图的 90% 以上

#### Scenario: 搜索分区数影响

- **WHEN** `search_partitions` 增大
- **THEN** 召回率提高
- **AND** 搜索延迟增加
