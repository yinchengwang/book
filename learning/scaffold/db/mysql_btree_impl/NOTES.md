# NOTES.md

## 工程源码对照

### 1. 页内结构对照

**教学代码** (`main.c`):
- 使用简化 Page Directory：`slots[]` 数组存储记录偏移
- 二分查找通过槽偏移定位记录：`page_binary_search()`
- 记录数组直接存储键值，便于教学演示

**工程源码** (`engineering/src/db/index/btree/btree_core.c`):
- `_btree_lower_bound()` 函数实现节点内二分查找
- 节点使用 `records[]` 数组存储有序记录，无 Page Directory 概念
- 记录结构更复杂：`btree_record_t` 包含 `key/keylen/value/valuelen`
- 工程采用内存 B-Tree，无磁盘页面概念

### 2. 页分裂实现对照

**教学代码** (`main.c` 的 `page_split()`):
- 分裂时机：`page->n_records >= PAGE_SIZE` 时触发
- 分裂策略：中间键上浮到父节点，左半保留、右半新建
- 递归分裂：父节点满时递归调用 `page_split()`
- 根节点分裂：创建新根，树高度增加

**工程源码** (`engineering/src/db/index/btree/btree_insert.c`):
- `_btree_split_child()` 实现分裂：
  - **Top-down 策略**：下探前预分裂，避免回溯
  - 分裂公式：满孩子按中位数一分为二（`t-1` 键分到右页）
  - 中位键提升到父节点：`_btree_record_move(&parent->records[child_pos], &child->records[t - 1u])`
- 根节点分裂特殊处理：`_btree_insert_internal()` 检测根满后先创建新根再分裂
- 工程实现更健壮：`_btree_node_create()` 失败时回滚，内存分配错误处理完善

**关键差异**：
- 教学代码采用 **回溯分裂**（Bottom-up），工程采用 **预分裂**（Top-down）
- 工程实现符合经典 B-Tree 算法（Cormen 等），下探前保证目标孩子未满

### 3. 页合并实现对照

**教学代码** (`main.c` 的 `page_merge()`):
- 合并时机：`page->n_records < min_keys` 时触发
- 合并策略：优先与左兄弟合并，其次右兄弟
- 父节点同步删除分隔键
- 递归合并：父节点低于最小键数时继续合并

**工程源码** (`engineering/src/db/index/btree/btree_delete.c`):
- `_btree_merge_children()` 实现合并：
  - 父节点分隔键下沉到左孩子
  - 右孩子整体并入左孩子：`_btree_record_move(&left->records[left->nkeys + i], &right->records[i])`
  - 子节点指针同步迁移
- `_btree_rotate_from_left/right()` 实现借位：
  - 不直接合并，先尝试从兄弟借一个键
  - 借位时父节点分隔键下沉/上浮
- `_btree_delete_from_node()` 采用 **Preemptive 策略**：
  - 下探前保证目标孩子有足够键（`min_degree`）
  - 不足时先借位或合并再下探

**关键差异**：
- 教学代码直接合并，工程优先借位（减少合并开销）
- 工程实现更高效：借位避免创建新节点，减少内存分配

### 4. 平衡维护对照

**教学代码** (`main.c`):
- `update_parent_keys()` 删除后回溯更新父节点键
- 树高度变化：根分裂增加、根合并减少

**工程源码** (`btree_delete.c`):
- `btree_index_delete()` 处理根节点空节点：
  ```c
  if (!index->root->is_leaf && index->root->nkeys == 0) {
      index->root = old_root->children[0];
      _btree_node_free_shallow(old_root);
  }
  ```
- 前驱/后继替换策略：
  - 删除内部节点键时，从左子树找前驱或右子树找后继替换
  - `_btree_descend_max/min()` 下探到叶子节点找替换键

### 5. 其他工程细节

**工程源码独有特性**：
1. **键比较抽象** (`btree_core.c`):
   - `_btree_compare()` 支持用户自定义比较函数
   - 默认字节序比较：`memcmp(lhs, rhs, min_len)`
   
2. **内存管理**:
   - `_btree_record_set()`：键值堆拷贝，防止外部指针失效
   - `_btree_node_free_shallow()`：只释放节点结构，不释放子树
   - `_btree_node_drop()`：后序递归释放整棵树

3. **Upsert 支持** (`btree_insert.c`):
   - `btree_index_upsert()`：存在则更新，不存在则插入
   - 避免先查找再插入的两次遍历开销

4. **统计信息** (`btreeam.h`):
   - `BTREEAMStats` 记录扫描/插入/删除次数
   - PostgreSQL 风格的页面头部：`BTPageHeaderData`

### 6. InnoDB vs 工程 B-Tree

| 特性 | 教学代码 | 工程源码 | InnoDB |
|------|----------|----------|---------|
| 页面大小 | 16 记录 | 内存节点 | 16KB |
| 分裂策略 | 回溯分裂 | Top-down预分裂 | 自顶向下 |
| 合并策略 | 直接合并 | 借位优先 | 延迟合并 |
| 页内查找 | Page Directory二分 | 数组二分 | Page Directory |
| 叶子链表 | next指针 | 无 | 双向链表 |

**总结**：工程源码采用经典 B-Tree 算法（Top-down 分裂、Preemptive 删除），比教学代码的回溯式实现更高效、更健壮。InnoDB 的 Page Directory 和叶子链表设计更适合磁盘存储场景。