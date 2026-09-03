# Skip List 跳表

## 概述

Skip List（跳表）是一种**概率平衡**的有序链表，通过随机层级实现平衡，无需复杂的旋转操作。

## 设计思想

```
普通链表:  1 → 3 → 5 → 7 → 9 → 11

Skip List: L3: 1 ──────────────────── 9
           L2: 1 ──────── 5 ──────── 9
           L1: 1 → 3 → 5 → 7 → 9 → 11
```

- 高层链表"跳过"低层链表的多个节点
- 查找时从高层向低层逐层下降
- 插入时随机决定节点层级

## 数据结构

```c
typedef struct skip_list_node {
    void *key;
    void *value;
    uint32_t level;              // 当前节点层级
    struct skip_list_node **forward;  // 每层的前向指针
} skip_list_node_t;
```

## 核心参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| MAX_LEVEL | 16 | 最大层级 |
| P | 0.5 | 晋升概率 |

随机层级计算：
```c
level = 1;
while (rand() < (RAND_MAX * P) && level < MAX_LEVEL) {
    level++;
}
```

## 操作复杂度

| 操作 | 时间复杂度 | 说明 |
|------|------------|------|
| 查找 | O(log n) | 期望时间 |
| 插入 | O(log n) | 期望时间 |
| 删除 | O(log n) | 期望时间 |
| 范围查询 | O(log n + k) | k 为结果数 |

## 空间复杂度

- 每个节点的期望空间：O(1/(1-P)) = O(2)
- 总空间：O(n)

## 应用场景

| 系统 | 用途 |
|------|------|
| Redis | ZSet（有序集合）底层实现 |
| LevelDB | MemTable 内存表 |
| MemSQL | 内存数据库索引 |

## 代码实现

项目路径：`src/index/tree/skip_list/`

核心模块：
- `skip_list_core.c` - 创建、销毁、查找
- `skip_list_insert.c` - 插入（含随机层级）
- `skip_list_delete.c` - 删除
- `skip_list_lookup.c` - 查找、范围查询

## 与红黑树的对比

| 维度 | Skip List | 红黑树 |
|------|-----------|--------|
| 实现复杂度 | 简单 | 复杂 |
| 并发友好 | 是（锁粒度小） | 否 |
| 平衡方式 | 概率 | 确定 |
| 内存占用 | 稍高 | 较低 |

## 面试要点

1. **Skip List 为什么是概率平衡？** - 通过随机层级实现平衡
2. **Skip List 的期望时间复杂度为什么是 O(log n)？** - 高层链表节点数约为 n/2^i
3. **Skip List 的优势是什么？** - 实现简单，并发友好
4. **Redis 为什么用 Skip List 而不是红黑树？** - 实现更简单，且 ZSet 还需要链表支持范围操作

## 参考资料

- Pugh, W. (1990). "Skip Lists: A Probabilistic Alternative to Balanced Trees"
- Redis 源码：t_zset.c