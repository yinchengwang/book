# R-Tree 持久化存储规范

## ADDED Requirements

### Requirement: R-Tree 持久化格式

空间存储引擎 SHALL 支持 R-Tree 索引的持久化存储。

#### Scenario: 保存 R-Tree 索引
- **WHEN** 调用 `rtree_save(tree, path)`
- **THEN** 系统 SHALL 将所有节点写入文件
- **AND** 系统 SHALL 维护节点偏移映射
- **AND** 系统 SHALL 写入根节点指针

#### Scenario: 加载 R-Tree 索引
- **WHEN** 调用 `rtree_load(path)`
- **THEN** 系统 SHALL 读取根节点指针
- **AND** 系统 SHALL 按需加载节点（延迟加载）
- **AND** 系统 SHALL 重建 R-Tree 结构

#### Scenario: R-Tree 文件损坏恢复
- **WHEN** 加载时检测到文件损坏
- **THEN** 系统 SHALL 尝试重建 R-Tree
- **OR** 系统 SHALL 返回错误并提供修复建议

### Requirement: 页面化存储

R-Tree SHALL 使用固定大小页面存储以支持 mmap。

#### Scenario: 页面化存储
- **WHEN** 保存 R-Tree 时
- **THEN** 系统 SHALL 将节点组织为 4KB 页面
- **AND** 每个页面 SHALL 包含一个或多个节点
- **AND** 页面 SHALL 支持 mmap 内存映射

#### Scenario: 页面缓存
- **WHEN** 访问 R-Tree 节点时
- **THEN** 系统 SHALL 首先检查页面缓存
- **AND** 缓存未命中时 SHALL 从磁盘加载页面
- **AND** 系统 SHALL 使用 LRU 淘汰策略

### Requirement: 节点序列化

R-Tree 节点 SHALL 使用紧凑的二进制格式。

#### Scenario: 序列化内部节点
- **WHEN** 序列化内部节点时
- **THEN** 系统 SHALL 写入节点边界框
- **AND** 系统 SHALL 写入子节点数量
- **AND** 系统 SHALL 写入子节点页面偏移

#### Scenario: 序列化叶子节点
- **WHEN** 序列化叶子节点时
- **THEN** 系统 SHALL 写入节点边界框
- **AND** 系统 SHALL 写入条目数量
- **AND** 系统 SHALL 写入对象 ID 和数据偏移

### Requirement: R-Tree 文件格式

```
rtree_file.bin:
  - RTreeHeader (64 bytes):
    - magic: "RTRE"
    - version: uint16_t
    - page_size: uint32_t
    - root_offset: uint64_t
    - root_level: uint32_t
    - node_count: uint32_t
    - item_count: uint32_t
  
  - Page[0..n-1] (4KB each):
    - PageHeader:
      - page_id: int32_t
      - node_count: int32_t
      - free_offset: int32_t
    - Nodes:
      - 内部节点或叶子节点数据
```

#### Scenario: 验证文件格式
- **WHEN** 加载 R-Tree 文件时
- **THEN** 系统 SHALL 验证 magic 字段
- **AND** 系统 SHALL 验证版本兼容性
- **AND** 系统 SHALL 验证页面完整性
