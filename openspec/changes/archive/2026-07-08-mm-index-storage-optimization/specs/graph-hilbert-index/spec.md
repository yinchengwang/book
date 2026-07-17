# 图 Hilbert 曲线索引规范

## ADDED Requirements

### Requirement: Hilbert 曲线映射

图存储引擎 SHALL 支持 Hilbert 曲线将多维坐标映射到一维索引。

#### Scenario: 计算 Hilbert 索引
- **WHEN** 调用 `hilbert_encode(x, y, order)`
- **THEN** 系统 SHALL 将 2D 坐标 (x, y) 映射到 1D Hilbert 索引
- **AND** 系统 SHALL 返回 uint64_t 类型的 Hilbert 曲线位置

#### Scenario: 计算 Hilbert 逆映射
- **WHEN** 调用 `hilbert_decode(h, order, &x, &y)`
- **THEN** 系统 SHALL 将 Hilbert 索引解码回 2D 坐标
- **AND** 系统 SHALL 返回坐标 (x, y)

### Requirement: 空间查询加速

Hilbert 曲线 SHALL 用于加速空间范围查询。

#### Scenario: 范围查询优化
- **WHEN** 调用 `graph_spatial_range_query(csr, bbox, results)`
- **THEN** 系统 SHALL 将 bbox 转换为 Hilbert 范围 [h_min, h_max]
- **AND** 系统 SHALL 使用 Hilbert 索引快速定位候选顶点
- **AND** 系统 SHALL 对候选顶点验证 bbox 交集

#### Scenario: KNN 查询优化
- **WHEN** 调用 `graph_spatial_knn(csr, point, k, results)`
- **THEN** 系统 SHALL 计算 point 的 Hilbert 索引
- **AND** 系统 SHALL 从 Hilbert 索引向外扩展搜索
- **AND** 系统 SHALL 返回最近的 k 个顶点

### Requirement: Hilbert 阶数动态选择

系统 SHALL 支持根据数据规模动态选择 Hilbert 阶数。

#### Scenario: 自动选择阶数
- **WHEN** 创建 Hilbert 索引时
- **THEN** 系统 SHALL 根据数据范围计算最小阶数
- **AND** 阶数 SHALL 满足 2^order >= max(width, height)
- **AND** 默认阶数 SHALL 为 16（支持 65536x65536 坐标）

### Requirement: Hilbert 索引存储 SHALL 遵循标准二进制布局

```
hilbert_index.bin:
  - header: magic, version, order, vertex_count
  - hilbert_codes[0..n-1]: uint64_t (按 Hilbert 排序)
  - vertex_ids[0..n-1]: 对应的顶点 ID
  - bbox_min: (min_x, min_y)
  - bbox_max: (max_x, max_y)
```

#### Scenario: 构建 Hilbert 索引
- **WHEN** 调用 `hilbert_index_build(csr)`
- **THEN** 系统 SHALL 读取所有顶点的空间坐标
- **AND** 系统 SHALL 计算每个顶点的 Hilbert 码
- **AND** 系统 SHALL 按 Hilbert 码排序
- **AND** 系统 SHALL 保存索引文件

#### Scenario: 更新 Hilbert 索引
- **WHEN** 添加或移动顶点时
- **THEN** 系统 SHALL 更新 Hilbert 索引
- **AND** 系统 SHALL 使用增量更新而非全量重建
