# 工程对照说明

## 与 engineering/src/db/storage/access/btree/btreeam.c 的对照

本 scaffold 演示了 m 叉 BTree 的核心原理，与工程实现 `btreeam.c` 有以下对应关系：

### 1. 节点结构

| Scaffold | btreeam.c | 说明 |
|----------|-----------|------|
| `BTreeNode.keys[]` | `BTPageHeader` 页面布局 | 键的存储方式 |
| `BTreeNode.nkeys` | `btpo_count` | 当前键数量 |
| `BTreeNode.is_leaf` | `btpo_flags & BTP_LEAF_FLAG` | 叶子节点标识 |
| `BTreeNode.children[]` | 页面间指针链 | 子节点访问 |

### 2. 核心操作对照

**查找操作**
- Scaffold: `search()` 递归遍历节点
- btreeam.c: `bt_getnext()` 从 Buffer Pool 读取页面，遍历索引条目

**插入操作**
- Scaffold: `insert()` + `split_child()` 处理节点分裂
- btreeam.c: `btinsert()` 从 Buffer 获取页面，空间不足时需要分裂逻辑

**遍历操作**
- Scaffold: `inorder()` 递归中序遍历
- btreeam.c: `bt_beginscan()` + `bt_getnext()` 实现扫描器，通过 `btpo_next` 链表遍历页面

### 3. 主要差异

| 方面 | Scaffold | btreeam.c (工程实现) |
|------|----------|---------------------|
| 存储介质 | 内存 | 磁盘（Buffer Pool） |
| 页面大小 | 动态分配 | 固定 `BTREE_PAGE_SIZE` |
| 持久化 | 无 | WAL 日志支持 |
| 并发控制 | 无 | MVCC / 锁支持 |
| 键比较 | 整数比较 | `bt_compare()` 函数指针 |

### 4. 工程实现的额外复杂性

`btreeam.c` 是完整的数据库索引实现，包含：

1. **Buffer Pool 集成**: 通过 `buf_read()` / `buf_get_data()` 管理页面缓存
2. **页面头部**: `BTPageHeader` 包含 `btpo_flags`, `btpo_level`, `btpo_prev/next` 等元数据
3. **扫描器**: `BTScanDesc` 结构维护扫描状态，支持向前/向后扫描
4. **统计信息**: `BTREEAMStats` 收集索引操作统计

### 5. 学习建议

学习完本 scaffold 后，建议：

1. 阅读 `btreeam.c` 理解页面级别的 BTree 操作
2. 对比 `engineering/src/db/index/btree/` 中的完整实现
3. 研究 PostgreSQL 源码 `reference/open-source/postgres/src/backend/access/nbtree/` 中的生产级 BTree

## 总结

本 scaffold 聚焦于 BTree 的核心算法（分裂、查找），而工程实现在此基础上增加了存储管理、并发控制、持久化等数据库特性。理解 scaffold 的原理有助于掌握工程实现的本质。
