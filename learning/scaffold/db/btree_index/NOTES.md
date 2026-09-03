# BTree 索引对照笔记

> 对照 `engineering/src/db/storage/access/btree/btreeam.c` 分析 BTree 实现细节

## 1. 页面结构

### 页面头部（BTPageHeaderData）

```c
typedef struct BTPageHeaderData {
    uint16_t    btpo_flags;       // 页面标志（BTP_LEAF_FLAG/BTP_INTERNAL_FLAG）
    uint16_t    btpo_level;       // 树层级（0=叶子）
    uint32_t    btpo_prev;        // 上一页页面号
    uint32_t    btpo_next;        // 下一页页面号
    uint32_t    btpo_xact;        // 事务 ID
    uint16_t    btpo_offset;      // 空闲空间起始偏移
    uint16_t    btpo_count;       // 页面中的条目数
} BTPageHeaderData;
```

**对照分析：**
- `btpo_flags` 使用位标志区分页面类型，与本项目的 `type` 字段功能相同
- `btpo_level` 表示树深度，叶子节点 level=0
- `btpo_prev/btpo_next` 形成叶子节点的双向链表，支持顺序扫描
- `btpo_offset` 从页面末尾向前增长，实现空间复用

### 页面大小

- `BTREE_PAGE_SIZE = 8192`（8KB，与 PostgreSQL 一致）
- `BTREE_PAGE_HEADER_SIZE = 20` 字节
- 实际可用空间 = 8192 - 20 = 8172 字节

### 条目结构

**内部节点条目（BTInternalTupleData）：**
```c
typedef struct BTInternalTupleData {
    uint32_t    block_number;     // 子页面块号
    uint16_t    offnum;           // 元组偏移
    uint8_t     downlink_offset;  // 下链偏移
    uint8_t     flags;            // 标志
} BTInternalTupleData;
```

**叶子节点条目（BTreekirtData）：**
```c
typedef struct BTreekirtData {
    Oid         heap_node;        // 堆页面节点
    uint32_t    block_number;     // 块号
    uint16_t    offset;           // 在页面中的偏移
    uint16_t    flags;            // 标志
} BTreekirtData;
```

## 2. 键比较

### 比较函数

```c
int bt_compare(Relation rel, const void *key1, const void *key2, int nkeys) {
    if (!key1 || !key2) {
        // NULL 键处理：NULL 被视为最小值
        if (!key1 && !key2) return 0;
        return key1 ? 1 : -1;
    }

    // 简化：假设键是整数
    int k1 = *(const int *)key1;
    int k2 = *(const int *)key2;
    return k1 - k2;
}
```

**关键点：**
- NULL 值被视为最小值（符合 SQL 标准）
- 返回值：<0 表示 key1 < key2，=0 表示相等，>0 表示 key1 > key2
- 实际实现中需要考虑数据类型和多列键

### 搜索定位

```c
int bt_page_get_item(void *page, const void *key, int nkeys, Relation rel) {
    // 简化实现使用线性查找
    // 实际 PostgreSQL 使用二分查找优化
}
```

## 3. 分裂策略

### 分裂触发条件

当 `bt_page_get_free_space(page) < needed` 时触发分裂。

### 分裂流程

1. **创建新页面**：分配右兄弟页面
2. **移动条目**：将后半部分条目移动到新页面
3. **更新链表**：设置 `btpo_next` 和 `btpo_prev`
4. **上推键值**：将分隔键插入父节点

### 分裂点选择

```
原始页面: [k1, k2, k3, k4, k5, k6, k7, k8]  (MAX_KEYS=8)

分裂后:
左页面:   [k1, k2, k3, k4]  (分裂键 k4)
右页面:   [k5, k6, k7, k8]

上推键值: k4
```

**分裂平衡原则：**
- `BTREE_MIN_ITEMS = 4`：保证每个页面至少保留 4 个条目
- 避免页面过空导致空间浪费

## 4. 扫描操作

### 扫描描述符

```c
typedef struct BTScanDescData {
    Relation    bt_relation;      // 索引 Relation
    ScanKey     bt_key;           // 扫描键
    int         bt_nkeys;         // 键数量
    ScanDirection bt_direction;   // 扫描方向（ForwardScanDirection/BackwardScanDirection）
    BufferDesc  *bt_curbuf;       // 当前缓冲区
    uint32_t    bt_curpage;       // 当前页面
    int         bt_curitem;       // 当前条目索引
} BTScanDescData;
```

### 顺序扫描实现

```c
void *bt_getnext(BTScanDesc scan, ScanDirection direction) {
    // 1. 如果没有当前缓冲区，读取根页面
    if (!scan->bt_curbuf) {
        scan->bt_curbuf = buf_read(scan->bt_relation->rd_relfilenode, 0, 0);
    }

    // 2. 检查当前页面的条目
    if (scan->bt_curitem >= ph->btpo_count) {
        // 3. 移到下一页（通过链表）
        if (ph->btpo_next == 0) {
            return NULL;  // 扫描结束
        }
        scan->bt_curpage = ph->btpo_next;
    }

    // 4. 返回当前条目
    return (void *)scan->bt_curitem;
}
```

### 范围扫描优化

1. **定位起始位置**：使用键比较找到第一个 >= 起始键的条目
2. **顺序遍历**：沿 `btpo_next` 链表顺序扫描
3. **提前终止**：遇到 > 结束键时停止

## 5. 页面空间管理

### 空闲空间计算

```c
int bt_page_get_free_space(void *page) {
    BTPageHeader ph = (BTPageHeader)page;
    return (int)(ph->btpo_offset - BTREE_PAGE_HEADER_SIZE -
                 ph->btpo_count * sizeof(BTInternalTupleData));
}
```

### 空间分配策略

- **从两端向中间**：头部从前往后，空闲空间从后往前
- **元组压缩**：删除时移动后续元组，避免碎片
- **HOT 优化**：Heap-Only Tuple 避免索引更新

## 6. 页面初始化

```c
void bt_page_init(void *page, uint16_t level) {
    BTPageHeader ph = (BTPageHeader)page;

    ph->btpo_flags = (level == 0) ? BTP_LEAF_FLAG : BTP_INTERNAL_FLAG;
    ph->btpo_level = level;
    ph->btpo_prev = 0;
    ph->btpo_next = 0;
    ph->btpo_xact = 0;
    ph->btpo_offset = BTREE_PAGE_SIZE;  // 空闲空间从页面末尾开始
    ph->btpo_count = 0;
}
```

## 7. 与本演示程序的区别

| 特性 | btreeam.c | main.c 演示 |
|------|-----------|-------------|
| 页面大小 | 8192 字节 | 4096 字节 |
| 存储层 | Buffer Pool + WAL | 内存数组 |
| 键类型 | 可配置 | 整数 |
| 查找算法 | 二分查找 | 线性查找 |
| 分裂处理 | 递归上推 | 简化为单层分裂 |
| 并发控制 | MVCC | 无 |
| 持久化 | 支持 | 无 |

## 8. 总结

BTree 索引的核心设计：
1. **页面结构**：固定大小页面 + 变长条目
2. **键比较**：支持多列键和 NULL 值
3. **分裂策略**：保证页面利用率 >= 50%
4. **扫描优化**：双向链表支持高效范围扫描
5. **空间复用**：从两端向中间管理空间
