# BTree 实现原理

## 1. 概述

BTree 是数据库索引的基石，几乎所有关系型数据库和许多 NoSQL 系统都使用 BTree 或其变体（B+Tree）作为默认的索引结构。

### 1.1 为什么选择 BTree？

| 需求 | BTree 如何满足 |
|------|---------------|
| 磁盘友好 | 节点大小 = 页面大小，减少 IO |
| 范围查询 | 叶子节点有序排列，高效范围扫描 |
| 动态更新 | 支持 O(log n) 的插入和删除 |
| 高扇出 | 内部节点可存放大量键，树高很小 |

### 1.2 BTree vs B+Tree

```
BTree：
┌─────────────┐
│ key₁|key₂|  │  ← 内部节点存放键和数据
├─────────────┤
│ P₀|P₁|P₂|  │
└─────────────┘

B+Tree：
┌─────────────┐                     ┌─────────────┐
│ key₁|key₂|  │  ← 内部节点只存放键   │ value₁│     │  ← 叶子节点存放键和数据
├─────────────┤                     │ value₂│     │
│ P₀|P₁|P₂|  │ ──────▶             └──────┬──────┘
└─────────────┘                            │
                                          ↓
                                    ┌─────────────┐
                                    │ 下一叶子页    │  ← 链表连接
                                    └─────────────┘
```

本实现采用 BTree，在内部节点存放数据和指针。

## 2. 页面结构

### 2.1 页面头部

```c
typedef struct BTPageHeader {
    uint64_t btpo_lsn;        // 最近修改的 LSN
    uint16_t btpo_flags;      // 页面标志
    uint16_t btpo_level;      // 层级（0=叶子）
    uint16_t btpo_count;      // 条目数量
    uint16_t btpo_prev;       // 前置页面号
    uint16_t btpo_next;       // 后继页面号
    uint16_t btpo_lower;      // 数据区起始偏移
    uint16_t btpo_upper;      // 空闲区起始偏移
    BlockNumber btpo_parent;  // 父页面号
} BTPageHeader;
```

### 2.2 页面布局

```
BTree 页面布局（Slotted Page）：

    ┌─────────────────────────────┐  ← 页面起始
    │  BTPageHeader (24 bytes)    │
    ├─────────────────────────────┤  ← btpo_lower
    │  ItemPointer[0]             │  条目指针数组
    │  ItemPointer[1]             │  （向下增长）
    │  ItemPointer[...]           │
    ├═════════════════════════════┤  ← btpo_upper
    │  Free Space                 │  空闲空间
    ├─────────────────────────────┤
    │  Item Data (key + value)    │  条目数据
    │  Item Data [...]            │  （向上增长）
    └─────────────────────────────┘  ← 页面结束

空闲空间 = btpo_upper - btpo_lower
```

### 2.3 条目格式

```c
typedef struct BTItemData {
    uint16_t key_size;      // 键大小
    uint16_t flags;         // 标志位
    uint8_t  key_data[];    // 键数据（变长）
    // 然后是 value 数据
} BTItemData;
```

## 3. 节点分裂算法

### 3.1 基本原理

当节点满了，将其分为两个节点：

```
分裂前：                              分裂后：
┌──────────────────────────┐         ┌──────────┐  ┌──────────┐
│ key₁ key₂ ... keyₙ      │         │ key₁...  │  │ keyₙ/₂+₁ │
│ (n 个键，已满)           │  ──▶    │ keyₙ/₂   │  │ ... keyₙ │
└──────────────────────────┘         └──────────┘  └──────────┘
                                              │
                                        中间键上移
                                              │
                                              ▼
                                     ┌──────────────┐
                                     │   父节点     │
                                     │ (插入中间键) │
                                     └──────────────┘
```

### 3.2 分裂点选择

```c
// 叶子节点分裂：平均分配
// 内部节点分裂：平均分配，中间键上推
static int bt_page_split(Relation rel, char *page, char *new_page) {
    // 计算分裂点
    int split_point = btpo_count / 2;

    // 复制后半部分到新页面
    for (int i = split_point; i < btpo_count; i++) {
        bt_page_copy_item(new_page, page, i, i - split_point);
    }

    // 更新条目数量
    set_btpo_count(page, split_point);
    set_btpo_count(new_page, btpo_count - split_point);

    return 0;
}
```

### 3.3 根分裂

根分裂是树高度增加的唯一方式：

```
根分裂：

分裂前：                分裂后：

    ┌───┐                   ┌───────┐
    │ R │                   │ 新根   │
    └───┘                   └─┬───┬─┘
      │                       │   │
      │                       ▼   ▼
  (已满，需要分裂)       ┌───┐ ┌───┐
                        │ L │ │ R │
                        └───┘ └───┘

树叶高度增加 1
```

## 4. 节点合并算法

### 4.1 基本原理

当节点删除后不足半满时，与兄弟节点合并：

```
合并前：                        合并后：

┌───────┐ ┌───────┐           ┌──────────────┐
│ 节点A  │ │ 节点B  │           │  节点A + 节点B  │
│ (3条)  │ │ (2条)  │   ──▶    │ (5条)          │
└───────┘ └───────┘           └──────────────┘

触发条件：条目数 < BTREE_MIN_ITEMS = n/2
```

### 4.2 再平衡

合并前先尝试从兄弟节点"借"一条：

```
再平衡前：                      再平衡后：

父：[key₃]                      父：[key₂]
     │                               │
 ┌───▼────┐ ┌────────┐       ┌──────▼─┐ ┌────────┐
 │ key₁   │ │ key₄   │       │ key₁   │ │ key₃   │
 │ key₂   │ │ key₅   │  ──▶  │ key₂   │ │ key₄   │
 └────────┘ └────────┘       └────────┘ └────────┘
 (右兄弟有多余条目)             (借一条给左兄弟)
```

```c
static int bt_page_redistribute(Relation rel, char *left_page,
                                 char *right_page, char *parent_page) {
    // 1. 将中间键从父页移到左页
    bt_page_copy_from_parent(left_page, parent_page, mid_key_pos);

    // 2. 将右页第一个键移到父页
    bt_page_copy_to_parent(parent_page, right_page, 0);

    // 3. 移除右页第一个键（已移到父页）
    bt_page_remove_item(right_page, 0);

    return 0;
}
```

## 5. 范围扫描算法

### 5.1 扫描流程

```
范围扫描 [10, 50] 的过程：

1. 从根向下查找 10 的位置
        │
        ▼
2. 在叶子节点找到 >= 10 的第一个条目
        │
        ▼
3. 顺序遍历叶子节点
   ┌─────┬─────┬─────┬─────┬─────┐
   │ 10  │ 20  │ 30  │ 40  │ 50  │ ← 范围扫描
   └─────┴─────┴─────┴─────┴─────┘
        │
        ▼
4. 通过叶子节点的兄弟指针跳转到下一页
   ┌─────┬─────┬─────┬─────┬─────┐
   │ 60  │ 70  │ ... │     │     │ ← 继续扫描（直到 > 50）
   └─────┴─────┴─────┴─────┴─────┘
```

### 5.2 扫描接口

```c
/**
 * @brief 范围扫描
 *
 * 从 start_key 开始扫描到 end_key：
 * 1. 使用二分查找定位起始键
 * 2. 顺叶子节点链表遍历
 * 3. 对每条记录调用 callback
 */
int bt_range_scan(Relation rel, void *start_key, void *end_key,
                  bool(*callback)(void *key, void *value)) {
    BTScanDesc scan = bt_beginscan(rel, 0, NULL);
    int count = 0;

    while (true) {
        void *tuple = bt_getnext(scan, ForwardScanDirection);
        if (!tuple) break;

        void *key = bt_get_key(tuple);
        if (compare_key(key, end_key) > 0) break;

        if (!callback(key, bt_get_data(tuple))) break;
        count++;
    }

    bt_endscan(scan);
    return count;
}
```

## 6. 并发控制

### 6.1 悲观并发控制

传统方法使用锁保护树结构：

```
悲观锁策略：

读操作：                    写操作：
1. 获取读锁（共享）         1. 获取写锁（排他）
2. 从根遍历到叶子           2. 从根遍历到叶子
3. 释放读锁                 3. 修改节点
                            4. 释放写锁

问题：锁粒度太大，并发度低
```

### 6.2 乐观并发控制（B-Link）

B-Link Tree 使用"链接"（兄弟指针）实现高并发：

```
B-Link Tree 的乐观并发控制：

1. 读操作不需要锁节点
2. 写操作使用锁耦合（lock coupling）逐层加锁
3. 如果发现节点发生分裂/合并，通过兄弟指针恢复

读操作的安全性：
┌───────┐  链接  ┌───────┐
│ 节点A  │ ──→   │ 节点B  │
│ [1,3]  │       │ [5,7]  │
└───────┘       └───────┘

如果搜索 key=3，节点A 已经不能找到，但在访问过程中
节点A 发生了分裂，key=3 被移到了节点B。
由于兄弟指针的存在，可以跟着链接到节点B。
```

## 7. 键比较和排序

### 7.1 多态比较

```c
/**
 * @brief 通用键比较
 *
 * 支持多种键类型：
 * 1. 整型键：直接比较
 * 2. 字符串键：strcmp
 * 3. 复合键：逐个分量比较
 */
int bt_compare(Relation rel, void *key1, void *key2, int nkeys) {
    for (int i = 0; i < nkeys; i++) {
        switch (column_type(rel, i)) {
            case INT4:
                if (*(int*)key1 < *(int*)key2) return -1;
                if (*(int*)key1 > *(int*)key2) return 1;
                break;
            case TEXT:
                int cmp = strcmp((char*)key1, (char*)key2);
                if (cmp != 0) return cmp;
                break;
        }
    }
    return 0;
}
```

### 7.2 排序规则（Collation）

```
排序规则对 BTree 的影响：

1. ASCII 排序：'A' < 'a' < 'Z' < 'z'
2. 字典排序：'A' = 'a' < 'B' = 'b'
3. 数字排序：'file2' < 'file10' < 'file100'

BTree 需要支持页面级别的排序规则配置
```

## 8. 持久化

### 8.1 页面写回

```
BTree 页面持久化流程：

1. 在内存中修改页面（标记为 dirty）
       │
       ▼
2. WAL 记录修改前的日志
       │
       ▼
3. 检查点或 eviction 时刷盘
       │
       ├──→ 计算 Checksum
       ├──→ 写入磁盘块
       └──→ 确认写入（fsync）
```

### 8.2 BTree 持久化映射

```c
// 前序遍历将节点映射到物理页面
static int bt_persist_map_node(BTNode *node, int *page_count) {
    int page_no = (*page_count)++;

    // 前序：先处理当前节点
    page_map[node] = page_no;

    // 再遍历子节点（如果是内部节点）
    if (!is_leaf(node)) {
        for (int i = 0; i < node->nkeys + 1; i++) {
            bt_persist_map_node(node->children[i], page_count);
        }
    }

    return page_no;
}
```

## 9. 性能特性

### 9.1 时间复杂度

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 查找 | O(log n) | 树高 ~ O(log n) |
| 插入（无分裂） | O(log n) | 查找位置 + 写入 |
| 插入（有分裂） | O(log n) | 最多 O(log n) 次分裂 |
| 删除（无合并） | O(log n) | 查找位置 + 删除 |
| 删除（有合并） | O(log n) | 最多 O(log n) 次合并 |
| 范围扫描 | O(log n + k) | k 是返回的条目数 |

### 9.2 空间特性

```
BTree 空间利用率：

理想情况（无分裂/合并）：50%-100% 的利用率
最坏情况（多次分裂）：约 50% 的利用率
平均情况：约 69% 的利用率（理论值：ln 2 ≈ 0.69）

这与 B+Tree 不同：
- B+Tree 可以保证叶子节点至少 50% 满
- BTree 没有这个保证
```

### 9.3 与 LSM Tree 对比

| 维度 | BTree | LSM Tree |
|------|-------|----------|
| 读性能 | **优秀** O(log n) | 一般（多层查找）|
| 写性能 | 一般（原地更新 + 随机 IO）| **优秀**（顺序写）|
| 空间放大 | 低（原地更新）| 高（合并过程产生）|
| 碎片 | 有（页分裂）| 无（追加写）|
| 并发 | 中等 | 良好（读多写少）|

## 10. 总结

BTree 的核心要点：

1. **磁盘友好设计**：节点大小 = 页面大小，减少 IO
2. **节点分裂**：保证树保持平衡
3. **节点合并**：回收空间，保持利用率
4. **范围扫描**：利用叶子节点有序性
5. **并发控制**：从笨重锁到乐观并发控制
6. **持久化**：WAL + 页面刷盘保证数据不丢失

---

*文档版本：v1.0*
*最后更新：2026-07-12*
