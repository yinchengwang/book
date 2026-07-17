# R-Tree 空间索引

## Purpose

实现 R-Tree 空间索引，用于学习多维空间数据索引。R-Tree 是地理信息系统（GIS）和地图应用的核心数据结构，支持矩形区域查询。

## Requirements

### Requirement: R-Tree 核心操作

R-Tree 实现 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 描述 |
|------|----------|------|
| 创建 | `rtree_t* rtree_create(uint32_t max_entries)` | 创建索引 |
| 销毁 | `void rtree_drop(rtree_t *tree)` | 释放所有内存 |
| 插入 | `int rtree_insert(...)` | 插入矩形和数据 |
| 删除 | `int rtree_delete(...)` | 删除指定矩形 |
| 搜索 | `int rtree_search(...)` | 矩形查询 |
| 迭代 | `rtree_iter_t* rtree_iter_create(...)` | 创建迭代器 |

### Requirement: R-Tree 数据结构

R-Tree 节点 SHALL 包含：
- `is_leaf`：是否为叶子节点
- `n_entries`：当前条目数
- `mbb`：最小边界矩形（MBR）
- `entries[]`：条目数组
  - 叶子节点：`rect + data`
  - 内部节点：`rect + child`

默认配置：
- `MAX_ENTRIES = 9`（类似 R-Tree 原始定义）

### Requirement: R-Tree 操作语义

- **插入**：选择最佳子节点插入，可能触发节点分裂
- **删除**：定位并移除条目，可能触发节点重平衡
- **搜索**：递归检查 MBR 相交性，剪枝无效分支
- **分裂**：使用 Quadratic 分裂算法

### Scenario: 基本操作

- **WHEN** 用户创建 `rtree_create(9)`
- **THEN** 创建 max_entries=9 的 R-Tree

- **WHEN** 用户插入矩形 R1=(0,0,10,10) 关联数据 D1
- **THEN** R1 被添加到根节点，更新根节点 MBR

- **WHEN** 用户调用 `rtree_search(query_rect)`
- **THEN** 返回所有与 query_rect 相交的矩形及其数据