# Skip List 工程对照说明

## 与 engineering/src/redis/redis_skiplist.c 的联系

`engineering/src/redis/redis_skiplist.c` 是 Redis 中跳表的具体实现，用于实现 **有序集合（Sorted Set）**。学习跳表时，理解 Redis 的实际应用场景有助于掌握其工程价值。

### 1. 核心用途：有序集合

Redis 的 ZSET（有序集合）是跳表最经典的应用场景：

```c
// Redis ZSET 使用跳表 + 字典实现
// 跳表：按分数排序，支持 O(log n) 范围查询
// 字典：按成员查找，支持 O(1) 成员存在性判断
typedef struct zset {
    dict *dict;      // 成员 -> 分数的哈希表
    zskiplist *zsl;  // 跳表，按分数排序
} zset;
```

### 2. 节点结构对比

| 本 Scaffold | Redis redis_skiplist.c |
|-------------|------------------------|
| 固定最大层数 16 | 固定最大层数 32 |
| key/value 分离 | score 在前向指针数组中 |
| 动态分配前向指针数组 | 前向指针数组在节点末尾 |

Redis 的 `zskiplistNode` 定义：
```c
typedef struct zskiplistNode {
    robj *obj;           // 成员对象
    double score;        // 分值（排序依据）
    struct zskiplistNode *backward;  // 反向指针（用于逆序遍历）
    struct zskiplistLevel {
        zskiplistNode *forward;  // 前向指针
        unsigned int span;       // 该层跨越的节点数（用于 rank 计算）
    } level[];
} zskiplistNode;
```

### 3. 层数生成策略

| 本 Scaffold | Redis |
|-------------|-------|
| 概率 P=0.5，几何分布 | 概率 P=0.25，层数限制 1-32 |
| 简单 rand() 实现 | 使用随机数生成器 |

Redis 的随机层数代码思路（简化）：
```c
int randomLevel(void) {
    int level = 1;
    while (random() < (RAND_MAX * 0.25) && level < ZSKIPLIST_MAXLEVEL) {
        level++;
    }
    return level;
}
```

### 4. 特殊能力

**Rank 计算**：`span` 字段记录每层跨越的节点数，可以在 O(log n) 内计算元素的排名，而无需遍历。

**反向遍历**：`backward` 指针支持从高往低逆序遍历有序集合。

### 5. 典型使用场景

| 场景 | 说明 |
|------|------|
| 排行榜 | ZADD/ZREVRANGE 实现实时排行榜 |
| 有序消息队列 | 按时间戳排序的消息 |
| 延迟队列 | 按执行时间排序的任务 |

### 6. 与本 Scaffold 的主要差异

| 特性 | 本 Scaffold | Redis |
|------|-------------|-------|
| 分值类型 | int | double |
| 成员存储 | key 直接存储 | robj 指针（支持字符串对象）|
| 层数上限 | 16 | 32 |
| P 概率 | 0.5 | 0.25 |
| 反向指针 | 无 | 有 |
| span 统计 | 无 | 有 |

### 7. 总结

跳表是一种实现简单但效率很高的有序数据结构。Redis 通过跳表实现 ZSET，使其能够高效支持：
- 按分数范围查询（ZREVRANGEBYSCORE）
- 按排名查询（ZREVRANK）
- 按分数获取排名（ZSCORE）

理解 Redis 的实现细节，有助于将学习成果应用到实际工程场景中。
