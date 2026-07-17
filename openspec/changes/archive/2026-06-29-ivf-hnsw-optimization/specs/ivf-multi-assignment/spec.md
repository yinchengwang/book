# IVF Multi-assignment 规格

## Purpose

定义 Multi-assignment 功能的需求、接口规范和行为约束。

## ADDED Requirements

### Requirement: Multi-assignment 配置

Multi-assignment SHALL 通过 `max_assignments` 参数控制每个向量分配到的桶数量。

#### Scenario: 设置 max_assignments

- **WHEN** 调用 `faiss_ivf_hnsw_index_set_max_assignments()` 设置 k 值
- **THEN** 后续添加的向量将分配到最近的 k 个桶

#### Scenario: max_assignments 边界

- **WHEN** max_assignments 设置为 ≤ 0 或超过合理上限（如 100）
- **THEN** 返回错误码 -1，参数保持不变

### Requirement: Multi-assignment 分配算法

向量 SHALL 基于到一级中心点的距离分配到最近的 k 个桶。

#### Scenario: k=1 默认分配

- **WHEN** max_assignments = 1
- **THEN** 每个向量只分配到距离最近的一个桶

#### Scenario: k>1 多桶分配

- **WHEN** max_assignments = k 且 k > 1
- **THEN** 每个向量分配到距离最近的 k 个一级簇

### Requirement: Multi-assignment 搜索展开

搜索时 SHALL 展开所有向量被分配到的桶。

#### Scenario: 搜索时展开多桶

- **WHEN** 搜索且向量分配到多个桶
- **THEN** 该向量的距离计算可能在多个桶内执行，最终取最优结果

#### Scenario: 重复向量处理

- **WHEN** 同一向量被分配到多个桶且在多个桶的扫描中被发现
- **THEN** 使用堆机制保证最终只保留一个结果

### Requirement: Multi-assignment 内存管理

Multi-assignment SHALL 记录每个向量分配到的所有桶信息。

#### Scenario: 内存布局

- **WHEN** N 个向量，max_assignments = k
- **THEN** 额外分配 N × k × sizeof(assign_entry_t) 字节

#### Scenario: 分配表大小查询

- **WHEN** 调用 `faiss_ivf_hnsw_index_assignment_size()`
- **THEN** 返回分配表的总字节数

### Requirement: Multi-assignment 与量化

Multi-assignment SHALL 与 PQ/SQ/LVQ/RQ 量化正交使用。

#### Scenario: 量化模式下多桶分配

- **WHEN** 启用量化且 max_assignments > 1
- **THEN** 向量在分配到的每个桶中存储其 PQ 编码
