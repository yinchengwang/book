# Merge-Vamana 子图合并规格

## Purpose

定义 Merge-Vamana 的行为规范，支持将多个子图合并为全局图。

## 术语说明

| 术语 | 说明 |
|------|------|
| Subgraph | 子图，分区独立构建的图 |
| Merge | 合并操作，将多个子图合并为全局图 |
| Partition | 分区，向量集合的划分 |
| Overlap | 重叠区域，跨分区边界的数据 |

## ADDED Requirements

### Requirement: Merge-Vamana 配置

Merge-Vamana SHALL 通过独立的 `diskann_merge_config_t` 配置。

```c
typedef struct diskann_merge_config {
    bool enabled;                    // 是否启用合并模式
    int32_t partition_method;        // 0=auto, 1=random, 2=kmeans
    int32_t partition_count;        // 分区数量
    float overlap_ratio;            // 重叠比例（0.0-1.0）
} diskann_merge_config_t;
```

#### Scenario: 默认配置

- **WHEN** 创建配置时未设置任何参数
- **THEN** `enabled = false`
- **AND** `partition_method = 0` (auto)
- **AND** `partition_count = 4`
- **AND** `overlap_ratio = 0.1`

#### Scenario: 启用合并模式

- **WHEN** `enabled = true`
- **THEN** 系统准备接收多个子图进行合并
- **AND** `partition_count >= 2`

### Requirement: 分区策略

Merge-Vamana SHALL 支持多种分区策略。

#### Scenario: Random 分区

- **WHEN** `partition_method = 1` (random)
- **AND** `partition_count = 4`
- **THEN** 向量随机分配到 4 个分区
- **AND** 每个分区约 1/4 的向量

#### Scenario: K-Means 分区

- **WHEN** `partition_method = 2` (kmeans)
- **AND** `partition_count = 4`
- **THEN** 使用 K-Means 将向量聚类为 4 个分区
- **AND** 每个分区对应一个聚类中心

#### Scenario: Coordinate-Range 分区

- **WHEN** `partition_method = 3` (coordinate-range)
- **THEN** 按坐标范围将向量空间划分为超立方体
- **AND** 适用于地理/坐标类型数据

### Requirement: 分区构建

每个分区 SHALL 独立构建子图。

#### Scenario: 独立子图构建

- **WHEN** 启用 Merge 模式且有 N 个分区
- **THEN** 每个分区独立运行 Vamana 构图算法
- **AND** 子图间无连接

#### Scenario: 分区重叠

- **WHEN** `overlap_ratio = 0.1`
- **THEN** 每个分区包含 10% 的相邻分区边界向量
- **AND** 重叠向量用于后续跨分区边构建

### Requirement: 图合并

多个子图 SHALL 能合并为全局图。

#### Scenario: 基本合并

- **WHEN** 有 3 个子图，每个约 100 万节点
- **AND** 调用 `diskann_merge_graphs(merged, subgraphs, 3)`
- **THEN** 合并后的图包含约 300 万节点
- **AND** 返回合并成功的状态码

#### Scenario: 节点去重

- **WHEN** 合并时发现重复节点 ID
- **THEN** 保留第一个子图中的节点
- **AND** 后续子图的重复节点跳过

#### Scenario: 边合并

- **WHEN** 合并多个子图
- **THEN** 保留所有子图中的边
- **AND** 如果同一对节点有多条边，只保留一条

### Requirement: 跨分区边

合并后 SHALL 构建跨分区边。

#### Scenario: 跨分区边构建

- **WHEN** 合并完成
- **THEN** 从重叠区域的向量出发，连接到相邻分区
- **AND** 使用全局入口点作为跨分区搜索起点

### Requirement: 合并质量

合并后图的搜索质量 SHALL 可接受。

#### Scenario: 召回率基线

- **GIVEN** 相同数据分别用单图和合并方式构建
- **WHEN** 比较搜索召回率
- **THEN** 合并图召回率应达到单图的 95% 以上

### Requirement: 配置验证

`diskann_merge_config_validate()` SHALL 验证配置有效性。

#### Scenario: 无效分区数

- **WHEN** `partition_count < 2`
- **THEN** `validate()` 返回 0

#### Scenario: 无效重叠比例

- **WHEN** `overlap_ratio < 0.0` 或 `overlap_ratio >= 1.0`
- **THEN** `validate()` 返回 0

#### Scenario: 有效配置

- **WHEN** `partition_count = 4, overlap_ratio = 0.15`
- **THEN** `validate()` 返回 1
