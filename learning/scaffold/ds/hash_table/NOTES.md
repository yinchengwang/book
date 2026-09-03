# 工程对照笔记

## 概述

本学习卡片演示的哈希表概念直接对应工程代码中的实际应用。以下对照工程源码中的哈希表实现模式，帮助理解理论与实践的关联。

---

## 对照一：dict 的 FNV-1a 哈希函数

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

分词器的 dict 模块实现了 FNV-1a 哈希算法：

```c
/* FNV-1a 64-bit 哈希算法
 * 与本卡片 djb2 算法的区别：
 *   - FNV 使用 XOR 而非乘法
 *   - FNV 使用质数乘以而不是加法
 *   - 两者都是经典的字符串哈希算法
 */
uint64_t dict_hash_fnv1a(const char *value)
{
    uint64_t hash = 14695981039346656037ULL;  /* FNV offset basis */
    const unsigned char *bytes = (const unsigned char *)value;

    if (!value) return 0;
    while (*bytes) {
        hash ^= (uint64_t)(*bytes);           /* XOR byte */
        hash *= 1099511628211ULL;             /* FNV prime */
        bytes += 1;
    }
    return hash;
}
```

**对照学习点**：
- FNV-1a 的 `offset basis` 和 `prime` 是标准常量，与本卡片 djb2 不同
- 使用 64 位无符号整数避免溢出问题
- 两种算法的选择取决于具体应用场景

---

## 对照二：dict 的哈希表结构与操作

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

dict 实现了完整的哈希表操作（set 和 map）：

```c
#define DICT_HASH_INITIAL_BUCKETS 64
#define DICT_HASH_LOAD_FACTOR 2.0  /* 每个桶允许的链表长度 */

/* 哈希映射节点（链地址法） */
typedef struct dict_hash_map_bucket_t {
    char *word;
    char *canonical_word;
    uint64_t hash_value;        /* 缓存哈希值，避免重复计算 */
    struct dict_hash_map_bucket_t *next;
} dict_hash_map_bucket_t;

/* 哈希表初始化 */
void dict_hash_map_init(dict_hash_map_t *map)
{
    map->buckets = (dict_hash_map_bucket_t **)calloc(
        DICT_HASH_INITIAL_BUCKETS, sizeof(dict_hash_map_bucket_t *));
    map->bucket_count = map->buckets ? DICT_HASH_INITIAL_BUCKETS : 0;
    map->item_count = 0;
}
```

**对照学习点**：
- 使用链地址法而非开放地址法（本卡片用线性探测）
- 缓存 `hash_value` 避免 rehashing 时重复计算
- 负载因子 2.0 表示每个桶允许平均 2 个元素

---

## 对照三：dict 的再哈希（rehashing）

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

```c
/* 哈希表扩容与再哈希 */
static int dict_hash_set_resize(dict_hash_set_t *set, int32_t new_bucket_count)
{
    dict_hash_bucket_t **new_buckets;
    int32_t i;

    /* 分配新的桶数组 */
    new_buckets = (dict_hash_bucket_t **)calloc(
        (size_t)new_bucket_count, sizeof(dict_hash_bucket_t *));
    if (!new_buckets) return -1;

    /* 遍历所有旧桶，重新计算索引并插入新表 */
    for (i = 0; i < set->bucket_count; ++i) {
        dict_hash_bucket_t *b = set->buckets[i];
        while (b) {
            dict_hash_bucket_t *next = b->next;
            int32_t idx = (int32_t)(b->hash_value % (uint64_t)new_bucket_count);
            /* 头插法插入新桶 */
            b->next = new_buckets[idx];
            new_buckets[idx] = b;
            b = next;
        }
    }

    free(set->buckets);
    set->buckets = new_buckets;
    set->bucket_count = new_bucket_count;
    return 0;
}

/* 插入时检查是否需要扩容 */
int dict_hash_set_add(dict_hash_set_t *set, const char *word)
{
    uint64_t hash = dict_hash_fnv1a(word);
    int32_t idx = (int32_t)(hash % (uint64_t)set->bucket_count);

    /* ... 查找重复 ... */

    /* 检查负载因子，超过阈值则扩容 */
    if ((double)set->item_count / (double)set->bucket_count >= DICT_HASH_LOAD_FACTOR) {
        if (dict_hash_set_resize(set, set->bucket_count * 2) != 0) {
            return -1;
        }
        /* 扩容后需要重新计算索引 */
        idx = (int32_t)(hash % (uint64_t)set->bucket_count);
    }

    /* ... 插入新节点 ... */
}
```

**对照学习点**：
- 扩容因子 `bucket_count * 2` 与本卡片一致
- 链地址法的 rehashing 只需重新计算 `idx`，无需迁移所有节点
- 扩容后使用 `hash_value % new_bucket_count` 计算新位置

---

## 对照四：uthash 的哈希表实现

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

dict 的 trie 节点使用 uthash 管理 children：

```c
/* uthash 宏使用示例
 * HASH_FIND: 在哈希表中查找
 * HASH_ADD: 添加新元素
 * HASH_DEL: 删除元素
 * HASH_ITER: 遍历所有元素
 */
dict_edge_t *dict_find_edge(const dict_node_t *node, uint32_t codepoint)
{
    if (node->root_children && codepoint < DICT_TRIE_ROOT_SIZE) {
        return node->root_children[codepoint];  /* 数组快速路径 */
    }

    dict_edge_t *edge = NULL;
    HASH_FIND(hh, node->children, &codepoint, sizeof(codepoint), edge);
    return edge;
}
```

**对照学习点**：
- uthash 是一个头文件-only 的哈希表库（无需编译）
- 使用时只需 `#include "uthash.h"`，无需额外依赖
- `hh` 是哈希表的句柄，用于 HASH_* 宏族

---

## 工程代码模式总结

| 组件 | 哈希算法 | 冲突解决 | 负载因子 | 特点 |
|------|---------|---------|---------|------|
| dict (set/map) | FNV-1a | 链地址法 | 2.0 | 缓存 hash_value |
| uthash | 可配置 | 链地址法 | - | 宏实现，零依赖 |
| 本卡片 | djb2 | 开放地址法 | 0.75 | 简单演示 |

---

## 延伸阅读

- `engineering/src/algo-prod/dict/dict_core.c` - 哈希表完整实现
- `engineering/src/algo-prod/dict/dict_private.h` - dict 模块内部结构
