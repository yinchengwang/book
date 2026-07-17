# 图 CSR 存储规范

## ADDED Requirements

### Requirement: CSR 边存储格式

图存储引擎 SHALL 实现 CSR (Compressed Sparse Row) 格式存储边数据。

#### Scenario: 创建 CSR 图存储
- **WHEN** 调用 `graph_csr_create(path, max_vertices)`
- **THEN** 系统 SHALL 创建顶点文件和边文件
- **AND** 系统 SHALL 初始化 offsets 数组
- **AND** 系统 SHALL 返回图句柄

#### Scenario: 添加边
- **WHEN** 调用 `graph_csr_add_edge(csr, src, dst, edge_type, props)`
- **THEN** 系统 SHALL 追加到 COO 缓冲区（增量模式）
- **AND** 系统 SHALL 更新 offsets[src+1]
- **AND** 当 COO 缓冲区达到阈值时触发合并

#### Scenario: 获取顶点出边
- **WHEN** 调用 `graph_csr_get_out_edges(csr, src, &edges, &count)`
- **THEN** 系统 SHALL 计算 offsets[src] 到 offsets[src+1] 的范围
- **AND** 系统 SHALL 从边数组读取目标顶点
- **AND** 系统 SHALL 返回边列表（O(1) 时间复杂度）

#### Scenario: 获取顶点入边
- **WHEN** 调用 `graph_csr_get_in_edges(csr, dst, &edges, &count)`
- **THEN** 系统 SHALL 使用反向索引定位入边
- **AND** 系统 SHALL 返回入边列表

### Requirement: CSR + COO 混合模式

系统 SHALL 支持 CSR + COO 混合模式以优化增量更新。

#### Scenario: COO 缓冲区溢出
- **WHEN** COO 缓冲区达到 coo_capacity
- **THEN** 系统 SHALL 触发后台合并任务
- **AND** 系统 SHALL 将 COO 数据合并到 CSR
- **AND** 系统 SHALL 清空 COO 缓冲区

#### Scenario: 合并 CSR 和 COO
- **WHEN** 调用 `graph_csr_compact(csr)`
- **THEN** 系统 SHALL 读取所有 COO 条目
- **AND** 系统 SHALL 重新排序并追加到 CSR
- **AND** 系统 SHALL 更新 offsets 数组

### Requirement: 顶点标签索引

图存储 SHALL 支持顶点标签索引以加速标签查询。

#### Scenario: 创建标签索引
- **WHEN** 调用 `graph_csr_create_label_index(csr, label_name)`
- **THEN** 系统 SHALL 创建 label -> vertex_ids 的映射
- **AND** 系统 SHALL 维护标签索引文件

#### Scenario: 按标签查询顶点
- **WHEN** 调用 `graph_csr_get_vertices_by_label(csr, label, &vertices, &count)`
- **THEN** 系统 SHALL 从标签索引查找顶点 ID 列表
- **AND** 系统 SHALL 返回匹配的顶点

### Requirement: CSR 文件格式

```
vertices.bin:
  - header: magic, version, vertex_count, label_count
  - vertices[0..n-1]:
    - label_id: uint32_t
    - prop_offset: uint64_t
    - prop_size: uint32_t

edges.bin:
  - header: magic, version, edge_count, src_count
  - offsets[0..vertex_count]: uint64_t (CSR 偏移)
  - edges[0..edge_count-1]:
    - dst: uint64_t
    - edge_type: uint32_t
    - edge_id: uint64_t
  - edge_props: 可变长属性数据

labels.bin:
  - label_count: uint32_t
  - labels[0..n-1]: 标签名字符串

coo_buffer.bin (增量模式):
  - entries: (src, dst, edge_type, props)
```

#### Scenario: 持久化图数据
- **WHEN** 调用 `graph_csr_save(csr)`
- **THEN** 系统 SHALL 刷新所有顶点数据到 vertices.bin
- **AND** 系统 SHALL 刷新所有边数据到 edges.bin
- **AND** 系统 SHALL 更新标签索引

#### Scenario: 加载图数据
- **WHEN** 调用 `graph_csr_load(path)`
- **THEN** 系统 SHALL 读取 vertices.bin 初始化顶点
- **AND** 系统 SHALL 读取 edges.bin 初始化边偏移
- **AND** 系统 SHALL 重建标签索引
