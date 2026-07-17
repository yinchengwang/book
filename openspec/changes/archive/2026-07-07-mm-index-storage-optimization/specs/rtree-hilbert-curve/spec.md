# R-Tree Hilbert 曲线优化规范

## ADDED Requirements

### Requirement: Hilbert 曲线空间填充

R-Tree SHALL 集成 Hilbert 曲线以优化空间查询。

#### Scenario: 计算几何 Hilbert 码
- **WHEN** 调用 `rtree_hilbert_bbox(bbox, order)`
- **THEN** 系统 SHALL 计算 bbox 中心点的 Hilbert 码
- **AND** 系统 SHALL 返回 Hilbert 范围 [h_min, h_max]

#### Scenario: Hilbert 排序优化
- **WHEN** R-Tree 插入多个条目时
- **THEN** 系统 SHALL 计算每个条目的 Hilbert 码
- **AND** 系统 SHALL 按 Hilbert 码排序后插入
- **AND** 插入性能 SHALL 优于随机插入

### Requirement: Hilbert 辅助索引

系统 SHALL 提供 Hilbert 辅助索引加速 KNN 查询。

#### Scenario: 构建 Hilbert 辅助索引
- **WHEN** 调用 `rtree_build_hilbert_index(tree)`
- **THEN** 系统 SHALL 遍历所有叶子节点
- **AND** 系统 SHALL 计算每个条目的 Hilbert 码
- **AND** 系统 SHALL 按 Hilbert 码排序构建辅助索引

#### Scenario: KNN 查询使用 Hilbert 索引
- **WHEN** 调用 `rtree_knn(tree, point, k, results)`
- **THEN** 系统 SHALL 计算查询点的 Hilbert 码
- **AND** 系统 SHALL 从 Hilbert 索引向外螺旋搜索
- **AND** 系统 SHALL 使用 Hilbert 距离优先队列

### Requirement: Hilbert 索引存储格式

```
rtree_hilbert.idx:
  - header: magic, version, order, item_count
  - HilbertRecord[0..n-1]:
    - hilbert_code: uint64_t
    - item_id: uint64_t
    - bbox: bbox_t
  - HilbertRange[0..level-1]:
    - level: uint8_t
    - min_h: uint64_t
    - max_h: uint64_t
```

#### Scenario: 保存 Hilbert 索引
- **WHEN** 调用 `rtree_hilbert_save(tree, path)`
- **THEN** 系统 SHALL 保存 Hilbert 辅助索引
- **AND** 系统 SHALL 与 R-Tree 文件分开存储

#### Scenario: 加载 Hilbert 索引
- **WHEN** 调用 `rtree_hilbert_load(tree, path)`
- **THEN** 系统 SHALL 读取 Hilbert 索引
- **AND** 系统 SHALL 验证与 R-Tree 一致性
- **AND** 系统 SHALL 启用 KNN 优化

### Requirement: 动态 Hilbert 阶数

系统 SHALL 支持根据数据范围动态选择 Hilbert 阶数。

#### Scenario: 自动选择阶数
- **WHEN** 构建 Hilbert 索引时
- **THEN** 系统 SHALL 分析所有 bbox 的坐标范围
- **AND** 系统 SHALL 选择满足精度要求的最小阶数
- **AND** 阶数 SHALL 在 1-32 之间
