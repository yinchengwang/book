# 跳表（Skip List）

跳表是一种基于多层有序链表实现的有序数据结构，通过随机层数在链表中构建"快速通道"，实现接近红黑树的 O(log n) 查找效率。

## 核心思想

跳表通过在上层维护稀疏索引来加速搜索：
- 第 0 层：完整链表，包含所有元素
- 第 1 层：约 1/2 的元素
- 第 2 层：约 1/4 的元素
- ...

查找时从高层向低层逐层缩小范围，时间复杂度为 O(log n)。

## 数据结构

```c
typedef struct SkipNode {
    int key;                    /* 键 */
    int value;                  /* 值 */
    int level;                  /* 该节点的层数 */
    struct SkipNode **forward;  /* 前向指针数组 */
} SkipNode;

typedef struct {
    SkipNode *header;           /* 头节点（哨兵） */
    int level;                  /* 当前最大层数 */
    int size;                   /* 节点计数 */
} SkipList;
```

## 基本操作

| 操作 | 平均时间复杂度 | 最坏时间复杂度 |
|------|---------------|---------------|
| 查找 | O(log n) | O(n) |
| 插入 | O(log n) | O(n) |
| 删除 | O(log n) | O(n) |
| 空间 | O(n) | O(n log n) |

## 随机层数

跳表的关键是随机层数生成：
- 每次插入时，以概率 P 决定是否提升到上一层
- 通常 P = 0.5（1/2 概率）
- 层数服从几何分布，期望值为 1/(1-P) = 2

```
Level 1: 50% 的节点
Level 2: 25% 的节点
Level 3: 12.5% 的节点
...
```

## 使用方法

```bash
make        # 编译
make run    # 运行
make clean  # 清理
```
