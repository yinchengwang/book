## MODIFIED: dict-hash-lookup

### 概述

将停用词和同义词的内部存储从单向链表替换为拉链哈希表，使 `dict_is_stop_word()` 和 `dict_resolve_synonym()` 的查询复杂度从 O(n) 降至 O(1) 均摊。

### 内部数据结构

```c
typedef struct dict_hash_bucket_t {
    char *word;                     // 词文本（指向 dict 内部字符串副本）
    uint64_t hash_value;            // FNV-1a 64-bit 哈希值
    struct dict_hash_bucket_t *next;// 拉链指针
} dict_hash_bucket_t;

typedef struct dict_hash_set_t {
    dict_hash_bucket_t **buckets;   // 桶数组
    int32_t bucket_count;           // 桶数量
    int32_t item_count;             // 已存储条目数
} dict_hash_set_t;

typedef struct dict_hash_map_t {
    dict_hash_bucket_t **buckets;
    int32_t bucket_count;
    int32_t item_count;
    // 每个 bucket 节点额外携带 canonical 指针
} dict_hash_map_t;
```

`dict_t` 结构变更：
- 移除 `dict_stop_entry_t *stop_word_head`
- 移除 `dict_synonym_entry_t *synonym_head`
- 新增 `dict_hash_set_t stop_words`
- 新增 `dict_hash_map_t synonyms`（key → canonical form）

### 哈希函数

使用 FNV-1a 64-bit，与 BM25 `bm25_hash_term()` 和项目内其他哈希保持一致：
```
hash = 14695981039346656037ULL
for each byte c:
    hash ^= c
    hash *= 1099511628211ULL
```

### 扩容策略

- 初始桶数：64
- 负载因子阈值：当 `item_count / bucket_count > 2.0` 时扩容
- 扩容倍数：2×（即 bucket_count 翻倍）
- Rehash：所有已有条目重新计算 `hash_value % new_bucket_count` 后插入

### 接口兼容性

以下公开 API 签名和行为完全不变：
- `bool dict_is_stop_word(const dict_t *dict, const char *word)`
- `const char *dict_resolve_synonym(const dict_t *dict, const char *word)`
- `int32_t dict_add_stop_word(dict_t *dict, const char *word)`
- `int32_t dict_add_synonym(dict_t *dict, const char *word, const char *canonical_word)`
- `int32_t dict_normalize_term(const dict_t *dict, const char *term, char **normalized_term)`（内部行为一致，仅查找变快）

### 约束

- 停用词和同义词不保证迭代顺序（哈希表不维护插入序）
- 重复添加同一停用词/同义词应为无操作（检查重复后跳过）
- 释放时遍历所有桶的拉链并释放节点和字符串副本
