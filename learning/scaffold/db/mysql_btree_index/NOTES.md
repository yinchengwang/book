# 工程对照笔记

## B-Tree 实现对照

本教学代码对照工程源码 `engineering/src/db/index/btree/` 目录。

### 数据结构对应关系

| 教学代码 | 工程源码 | 说明 |
|---------|---------|------|
| `btree_node_t` | `btree_node_t` (btree_private.h) | 节点结构一致 |
| `keys[ORDER-1]` | `btree_record_t *records` | 工程用动态数组存键值对 |
| `values[ORDER-1]` | `record->value/valuelen` | 工程支持变长 value |
| `children[ORDER]` | `btree_node_t **children` | 子指针数组 |
| `n` | `nkeys` | 当前键数量 |

### 核心函数对照

| 教学函数 | 工程函数 | 差异 |
|---------|---------|------|
| `find_key_pos()` | `_btree_lower_bound()` | 工程用标准二分，返回 lower_bound |
| `split_child()` | `_btree_split_child()` | 工程支持变长 key/value，用 move 而非拷贝 |
| `insert_nonfull()` | `_btree_insert_nonfull()` | 工程检测重复 key 返回 1 |
| `search()` | `btree_index_lookup()` | 工程返回 value 指针和长度 |

### 工程实现的关键增强

1. **变长键值**：工程支持任意长度 key/value，教学代码用 int 固定长度
2. **自定义比较**：工程支持 `btree_compare_fn` 回调，教学代码用整数比较
3. **持久化**：工程有 `btree_persist.c` 实现磁盘序列化，教学代码纯内存
4. **删除操作**：工程有完整的 `_btree_delete_from_node()` 借位/合并逻辑
5. **错误处理**：工程检查 malloc 失败，教学代码简化未处理

### B-Tree vs B+Tree 说明

工程实现的是 **B-Tree**（内部节点存数据），而非 B+Tree。

MySQL InnoDB 的 B+Tree 特点：
- 数据全在叶子层，内部节点只存 key
- 叶子层通过双向链表连接，范围查询高效
- 聚簇索引：主键索引的叶子存完整行数据
- 辅助索引：叶子存主键值，需回表查聚簇索引

工程的 B-Tree 是教学简化版，适合理解索引原理。