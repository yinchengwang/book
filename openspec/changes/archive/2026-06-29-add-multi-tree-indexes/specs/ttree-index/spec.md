# T-Tree 内存索引

## Purpose

实现 T-Tree 索引数据结构，用于学习内存数据库索引设计原理。T-Tree 是专为内存优化的多路搜索树，平衡了二叉搜索树的简洁性和 B-Tree 的高效性。

## Requirements

### Requirement: T-Tree 核心操作

T-Tree 实现 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 描述 |
|------|----------|------|
| 创建 | `ttree_index_t* ttree_index_create(uint32_t min_keys)` | 创建索引，指定最小 key 数 |
| 销毁 | `void ttree_index_drop(ttree_index_t *index)` | 释放所有内存 |
| 插入 | `int ttree_index_insert(...)` | 插入 key-value |
| 删除 | `int ttree_index_delete(...)` | 删除指定 key |
| 查找 | `void* ttree_index_search(...)` | 查找 key 对应的 value |
| 范围查询 | `int ttree_index_range(...)` | 范围查找 |

### Requirement: T-Tree 数据结构

T-Tree 节点 SHALL 包含：
- `is_leaf`：是否为叶子节点
- `nkeys`：当前 key 数量
- `keys[]`：key 数组
- `values[]`：value 数组
- `left/right`：左右子树指针
- `prev/next`：兄弟链表指针（仅叶子节点）

每个节点存储 `[min_keys, 2*min_keys-1]` 个 key。

### Requirement: T-Tree 操作语义

- **插入**：节点未满时直接插入排序；满时尝试旋转或分裂
- **删除**：节点 key 过少时尝试借位或与兄弟合并
- **查找**：从根向下，利用多路搜索定位

### Scenario: 基本操作

- **WHEN** 用户调用 `ttree_index_create(4)`
- **THEN** 创建 min_keys=4 的 T-Tree，根节点初始为空

- **WHEN** 用户插入一系列 key
- **THEN** T-Tree 自动平衡，保持每个节点 key 数在 [min_keys, 2*min_keys-1]

- **WHEN** 用户调用 `ttree_index_search(key)`
- **THEN** 返回对应 value 或 NULL