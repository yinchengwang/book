/*
 * main.c - 哈希索引演示：线性哈希 vs 可扩展哈希
 *
 * 演示内容：
 *   1. 线性哈希（Linear Hashing）：初始桶数 N，负载因子超阈值后分裂，分裂指针前进
 *   2. 可扩展哈希（Extendible Hashing）：局部深度 + 全局深度，桶分裂时深度翻倍
 *   3. 冲突解决：链地址法（拉链法）
 *   4. 行为对比：展示两种哈希扩容策略的差异
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BUCKET_SIZE 4        /* 每个桶的最大容量 */
#define INITIAL_BUCKETS 2    /* 初始桶数量 */
#define LOAD_FACTOR 75       /* 负载因子阈值（百分比） */

/* ===================== 链地址法冲突解决 ===================== */

/* 键值对节点 */
typedef struct HashNode {
    int                key;
    int                value;
    struct HashNode   *next;      /* 拉链法：指向下一个冲突节点 */
} HashNode_t;

/* 桶结构 */
typedef struct Bucket {
    HashNode_t *head;             /* 链表头 */
    int         count;            /* 桶内元素数量 */
} Bucket_t;

/* ===================== 线性哈希 ===================== */

typedef struct LinearHash {
    Bucket_t *buckets;            /* 桶数组 */
    int       bucket_count;       /* 当前桶数量 */
    int       split_pointer;      /* 分裂指针：指向下一个要分裂的桶 */
    int       total_items;        /* 总元素数 */
    int       level;              /* 当前轮次（log2(bucket_count)） */
} LinearHash_t;

/* 简单哈希函数 */
static uint32_t hash_func(int key) {
    return (uint32_t)(key * 2654435761u);  /* Knuth 乘法哈希 */
}

/* 线性哈希：计算桶号 */
static int linear_hash_get_bucket(LinearHash_t *lh, int key) {
    uint32_t h = hash_func(key);
    int bucket = (int)(h & ((1u << (lh->level + 1)) - 1));  /* 使用 level+1 位 */

    /* 如果桶号 >= 当前桶数，则回退到 level 位 */
    if (bucket >= lh->bucket_count) {
        bucket = (int)(h & ((1u << lh->level) - 1));
    }
    return bucket;
}

/* 向桶中插入节点（链地址法） */
static void bucket_insert(Bucket_t *bucket, int key, int value) {
    HashNode_t *node = (HashNode_t *)malloc(sizeof(HashNode_t));
    node->key = key;
    node->value = value;
    node->next = bucket->head;
    bucket->head = node;
    bucket->count++;
}

/* 线性哈希：分裂一个桶 */
static void linear_hash_split(LinearHash_t *lh) {
    int old_bucket = lh->split_pointer;
    int new_bucket = lh->bucket_count;

    /* 扩展桶数组 */
    lh->buckets = (Bucket_t *)realloc(lh->buckets,
                                       (size_t)(new_bucket + 1) * sizeof(Bucket_t));
    lh->buckets[new_bucket].head = NULL;
    lh->buckets[new_bucket].count = 0;
    lh->bucket_count++;

    /* 重哈希旧桶中的元素 */
    HashNode_t *node = lh->buckets[old_bucket].head;
    lh->buckets[old_bucket].head = NULL;
    lh->buckets[old_bucket].count = 0;

    while (node) {
        HashNode_t *next = node->next;
        int bucket = linear_hash_get_bucket(lh, node->key);
        bucket_insert(&lh->buckets[bucket], node->key, node->value);
        free(node);
        node = next;
    }

    /* 移动分裂指针 */
    lh->split_pointer++;

    /* 一轮结束，重置分裂指针，增加 level */
    if (lh->split_pointer >= (1 << lh->level)) {
        lh->split_pointer = 0;
        lh->level++;
    }
}

/* 线性哈希：插入 */
static void linear_hash_insert(LinearHash_t *lh, int key, int value) {
    int bucket = linear_hash_get_bucket(lh, key);
    bucket_insert(&lh->buckets[bucket], key, value);
    lh->total_items++;

    /* 检查是否需要分裂 */
    int threshold = (LOAD_FACTOR * lh->bucket_count * BUCKET_SIZE) / 100;
    if (lh->total_items > threshold) {
        linear_hash_split(lh);
    }
}

/* ===================== 可扩展哈希 ===================== */

typedef struct ExtendibleHash {
    Bucket_t *directory;          /* 目录数组（每个槽位指向一个桶） */
    int       global_depth;       /* 全局深度 */
    int       total_items;
} ExtendibleHash_t;

/* 可扩展哈希：计算目录索引 */
static int extendible_hash_get_dir_idx(ExtendibleHash_t *eh, int key) {
    uint32_t h = hash_func(key);
    return (int)(h & ((1u << eh->global_depth) - 1));
}

/* 可扩展哈希：分裂桶（简化版，只展示核心逻辑） */
static void extendible_hash_split(ExtendibleHash_t *eh, int dir_idx) {
    Bucket_t *old_bucket = &eh->directory[dir_idx];

    /* 增加全局深度，目录翻倍 */
    int old_size = 1 << eh->global_depth;
    int new_size = old_size * 2;

    eh->directory = (Bucket_t *)realloc(eh->directory,
                                        (size_t)new_size * sizeof(Bucket_t));

    /* 复制旧目录到新位置（共享桶） */
    for (int i = 0; i < old_size; i++) {
        eh->directory[old_size + i] = eh->directory[i];
    }
    eh->global_depth++;

    printf("  [可扩展哈希] 目录翻倍！全局深度 %d -> %d，目录大小 %d\n",
           eh->global_depth - 1, eh->global_depth, new_size);

    /* 重哈希：将旧桶元素重新分配到两个新桶 */
    HashNode_t *node = old_bucket->head;
    old_bucket->head = NULL;
    old_bucket->count = 0;

    (void)dir_idx;  /* 简化版不追踪具体槽位 */

    while (node) {
        HashNode_t *next = node->next;
        int new_idx = extendible_hash_get_dir_idx(eh, node->key);
        bucket_insert(&eh->directory[new_idx], node->key, node->value);
        free(node);
        node = next;
    }
}

/* 可扩展哈希：插入（简化版） */
static void extendible_hash_insert(ExtendibleHash_t *eh, int key, int value) {
    int dir_idx = extendible_hash_get_dir_idx(eh, key);
    bucket_insert(&eh->directory[dir_idx], key, value);
    eh->total_items++;

    /* 如果桶满，触发分裂 */
    if (eh->directory[dir_idx].count > BUCKET_SIZE) {
        printf("  [可扩展哈希] 桶 %d 溢出（count=%d），触发分裂\n",
               dir_idx, eh->directory[dir_idx].count);
        extendible_hash_split(eh, dir_idx);
    }
}

/* ===================== 演示主程序 ===================== */

static void print_linear_hash(LinearHash_t *lh) {
    printf("线性哈希状态：桶数=%d，分裂指针=%d，level=%d，总元素=%d\n",
           lh->bucket_count, lh->split_pointer, lh->level, lh->total_items);
    for (int i = 0; i < lh->bucket_count && i < 8; i++) {
        printf("  桶[%d]: %d 项 -> ", i, lh->buckets[i].count);
        HashNode_t *n = lh->buckets[i].head;
        int cnt = 0;
        while (n && cnt < 6) { printf("%d ", n->key); n = n->next; cnt++; }
        printf("\n");
    }
}

static void print_extendible_hash(ExtendibleHash_t *eh) {
    printf("可扩展哈希状态：全局深度=%d，目录大小=%d，总元素=%d\n",
           eh->global_depth, 1 << eh->global_depth, eh->total_items);
    int size = 1 << eh->global_depth;
    for (int i = 0; i < size && i < 8; i++) {
        printf("  目录[%d]: %d 项 -> ", i, eh->directory[i].count);
        HashNode_t *n = eh->directory[i].head;
        int cnt = 0;
        while (n && cnt < 6) { printf("%d ", n->key); n = n->next; cnt++; }
        printf("\n");
    }
}

int main(void) {
    printf("========== 哈希索引演示：线性哈希 vs 可扩展哈希 ==========\n\n");

    /* 初始化线性哈希 */
    LinearHash_t lh;
    lh.buckets = (Bucket_t *)calloc((size_t)INITIAL_BUCKETS, sizeof(Bucket_t));
    lh.bucket_count = INITIAL_BUCKETS;
    lh.split_pointer = 0;
    lh.total_items = 0;
    lh.level = 1;

    /* 初始化可扩展哈希 */
    ExtendibleHash_t eh;
    eh.global_depth = 1;
    eh.directory = (Bucket_t *)calloc(2, sizeof(Bucket_t));
    eh.total_items = 0;

    int test_keys[] = {10, 20, 30, 40, 50, 15, 25, 35, 45, 55, 60, 70, 80, 90, 100, 5};
    int n = sizeof(test_keys) / sizeof(test_keys[0]);

    printf("--- 插入 %d 个键值对，观察扩容行为 ---\n\n", n);

    for (int i = 0; i < n; i++) {
        int key = test_keys[i];
        printf("插入 key=%d\n", key);

        linear_hash_insert(&lh, key, key * 10);
        extendible_hash_insert(&eh, key, key * 10);

        printf("  线性哈希：桶数=%d, 分裂指针=%d\n", lh.bucket_count, lh.split_pointer);
        printf("  可扩展哈希：全局深度=%d, 目录大小=%d\n\n", eh.global_depth, 1 << eh.global_depth);
    }

    printf("--- 最终状态对比 ---\n\n");
    print_linear_hash(&lh);
    printf("\n");
    print_extendible_hash(&eh);

    printf("\n========== 关键差异总结 ==========\n");
    printf("1. 线性哈希：按顺序分裂桶（分裂指针前进），渐进式扩容\n");
    printf("2. 可扩展哈希：溢出桶分裂时翻倍目录，瞬间扩容\n");
    printf("3. 冲突解决：两者均使用链地址法处理哈希冲突\n");
    printf("4. 哈希索引 O(1) 点查，但不支持范围查询\n");

    /* 释放内存 */
    for (int i = 0; i < lh.bucket_count; i++) {
        HashNode_t *n = lh.buckets[i].head;
        while (n) { HashNode_t *next = n->next; free(n); n = next; }
    }
    free(lh.buckets);

    int size = 1 << eh.global_depth;
    for (int i = 0; i < size; i++) {
        HashNode_t *n = eh.directory[i].head;
        while (n) { HashNode_t *next = n->next; free(n); n = next; }
    }
    free(eh.directory);

    return 0;
}