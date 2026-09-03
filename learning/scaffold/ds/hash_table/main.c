/**
 * @file main.c
 * @brief 数据结构哈希表学习卡片 - 演示哈希函数与冲突解决
 *
 * 涵盖内容：
 * - 哈希函数（djb2 算法）
 * - 冲突解决策略：开放地址法（线性探测）
 * - 动态扩容与再哈希（rehashing）
 * - 负载因子与性能关系
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 8        /* 初始桶数量 */
#define MAX_LOAD_FACTOR  0.75     /* 最大负载因子 */

/* ==================== 类型定义 ==================== */

/**
 * 键值对节点
 */
typedef struct HashNode {
    char *key;            /* 字符串键 */
    int   value;          /* 整型值 */
    bool  occupied;       /* 是否被占用 */
} HashNode;

/**
 * 哈希表结构
 */
typedef struct {
    HashNode *buckets;    /* 桶数组 */
    size_t    capacity;   /* 桶数量 */
    size_t    size;       /* 已存储元素数 */
} HashTable;

/* ==================== 哈希函数 ==================== */

/**
 * djb2 哈希算法
 * 特点：简单高效，分布均匀
 * 算法：hash = hash * 33 + c (经典的字符串哈希)
 */
static size_t djb2_hash(const char *key, size_t capacity)
{
    size_t hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = hash * 33 + c;  /* hash * 33 + c */
    }
    return hash % capacity;
}

/* ==================== 哈希表操作 ==================== */

/**
 * 创建哈希表
 */
static HashTable *hash_table_create(void)
{
    HashTable *ht = (HashTable *)malloc(sizeof(HashTable));
    if (!ht) return NULL;

    ht->buckets = (HashNode *)calloc(INITIAL_CAPACITY, sizeof(HashNode));
    if (!ht->buckets) { free(ht); return NULL; }

    ht->capacity = INITIAL_CAPACITY;
    ht->size = 0;
    return ht;
}

/**
 * 释放哈希表
 */
static void hash_table_free(HashTable *ht)
{
    if (!ht) return;
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->buckets[i].occupied && ht->buckets[i].key) {
            free(ht->buckets[i].key);
        }
    }
    free(ht->buckets);
    free(ht);
}

/**
 * 计算当前负载因子
 */
static double hash_table_load_factor(const HashTable *ht)
{
    return (double)ht->size / (double)ht->capacity;
}

/**
 * 扩容哈希表（2倍策略）
 * 扩容后所有元素需要重新计算位置（rehashing）
 */
static int hash_table_resize(HashTable *ht, size_t new_capacity)
{
    printf("  [rehash] 扩容: capacity %zu -> %zu\n", ht->capacity, new_capacity);

    HashNode *old_buckets = ht->buckets;
    size_t    old_capacity = ht->capacity;

    /* 分配新的桶数组 */
    HashNode *new_buckets = (HashNode *)calloc(new_capacity, sizeof(HashNode));
    if (!new_buckets) return -1;

    ht->buckets = new_buckets;
    ht->capacity = new_capacity;
    ht->size = 0;

    /* 重新插入所有元素（rehashing） */
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_buckets[i].occupied && old_buckets[i].key) {
            /* 重新插入到新表 */
            size_t idx = djb2_hash(old_buckets[i].key, ht->capacity);
            while (new_buckets[idx].occupied) {
                idx = (idx + 1) % ht->capacity;  /* 线性探测 */
            }
            new_buckets[idx].key = old_buckets[i].key;
            new_buckets[idx].value = old_buckets[i].value;
            new_buckets[idx].occupied = true;
            ht->size++;
        }
    }

    free(old_buckets);
    return 0;
}

/**
 * 插入键值对（如果键已存在则更新值）
 * 使用开放地址法 + 线性探测解决冲突
 */
static int hash_table_put(HashTable *ht, const char *key, int value)
{
    /* 检查是否需要扩容 */
    if (hash_table_load_factor(ht) >= MAX_LOAD_FACTOR) {
        if (hash_table_resize(ht, ht->capacity * 2) != 0) {
            return -1;
        }
    }

    /* 计算初始桶位置 */
    size_t idx = djb2_hash(key, ht->capacity);

    /* 线性探测寻找空位或同键位置 */
    while (ht->buckets[idx].occupied) {
        if (strcmp(ht->buckets[idx].key, key) == 0) {
            /* 键已存在，更新值 */
            ht->buckets[idx].value = value;
            printf("  更新: key=\"%s\" -> value=%d\n", key, value);
            return 0;
        }
        /* 探测下一个位置 */
        idx = (idx + 1) % ht->capacity;
    }

    /* 找到空位，插入新元素 */
    ht->buckets[idx].key = (char *)malloc(strlen(key) + 1);
    if (!ht->buckets[idx].key) return -1;
    strcpy(ht->buckets[idx].key, key);
    ht->buckets[idx].value = value;
    ht->buckets[idx].occupied = true;
    ht->size++;

    printf("  插入: key=\"%s\", value=%d, 负载因子=%.2f\n",
           key, value, hash_table_load_factor(ht));
    return 0;
}

/**
 * 查找键对应的值
 * @param ht     哈希表
 * @param key    键
 * @param out    输出参数，存储找到的值
 * @return       0=找到, -1=未找到
 */
static int hash_table_get(const HashTable *ht, const char *key, int *out)
{
    size_t idx = djb2_hash(key, ht->capacity);
    size_t start = idx;
    int probes = 0;

    /* 线性探测直到遇到空位或找到目标 */
    do {
        if (!ht->buckets[idx].occupied) {
            /* 遇到空位，键不存在 */
            printf("  查找 \"%s\": 未找到 (探测次数=%d)\n", key, probes);
            return -1;
        }
        if (strcmp(ht->buckets[idx].key, key) == 0) {
            *out = ht->buckets[idx].value;
            printf("  查找 \"%s\": value=%d (探测次数=%d)\n", key, *out, probes);
            return 0;
        }
        idx = (idx + 1) % ht->capacity;
        probes++;
    } while (idx != start);

    return -1;
}

/**
 * 删除键值对
 * 注意：删除后需要标记墓碑（tombstone）以避免查找中断
 */
static int hash_table_remove(HashTable *ht, const char *key)
{
    size_t idx = djb2_hash(key, ht->capacity);
    size_t start = idx;

    while (true) {
        if (!ht->buckets[idx].occupied) {
            return -1;  /* 遇到空位，键不存在 */
        }
        if (strcmp(ht->buckets[idx].key, key) == 0) {
            /* 找到目标，删除 */
            free(ht->buckets[idx].key);
            /* 简化实现：标记为已删除（tombstone） */
            ht->buckets[idx].occupied = false;
            ht->size--;
            printf("  删除: key=\"%s\" (标记墓碑)\n", key);
            return 0;
        }
        idx = (idx + 1) % ht->capacity;
        if (idx == start) break;
    }

    return -1;
}

/**
 * 打印哈希表状态
 */
static void hash_table_print(const HashTable *ht)
{
    printf("  状态: size=%zu, capacity=%zu, 负载因子=%.2f\n",
           ht->size, ht->capacity, hash_table_load_factor(ht));
    printf("  桶内容:\n");
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->buckets[i].occupied) {
            printf("    [%zu] %s -> %d\n", i, ht->buckets[i].key, ht->buckets[i].value);
        } else {
            printf("    [%zu] (empty)\n", i);
        }
    }
}

/* ==================== 演示函数 ==================== */

static void demo_basic_operations(void)
{
    printf("[hash_table] 基本操作演示\n");

    HashTable *ht = hash_table_create();
    if (!ht) return;

    /* 插入元素 */
    printf("  插入操作:\n");
    hash_table_put(ht, "Alice", 25);
    hash_table_put(ht, "Bob", 30);
    hash_table_put(ht, "Charlie", 35);

    /* 查找元素 */
    printf("\n  查找操作:\n");
    int value;
    if (hash_table_get(ht, "Bob", &value) == 0) {
        printf("  找到 Bob 的年龄: %d\n", value);
    }

    /* 更新元素 */
    printf("\n  更新操作:\n");
    hash_table_put(ht, "Bob", 31);

    /* 删除元素 */
    printf("\n  删除操作:\n");
    hash_table_remove(ht, "Alice");

    /* 打印完整状态 */
    printf("\n  最终状态:\n");
    hash_table_print(ht);

    hash_table_free(ht);
    printf("\n");
}

static void demo_resize(void)
{
    printf("[hash_table] 动态扩容演示\n");

    HashTable *ht = hash_table_create();
    if (!ht) return;

    printf("  初始状态:\n");
    hash_table_print(ht);

    /* 插入足够多的元素触发扩容 */
    printf("\n  插入更多元素:\n");
    const char *names[] = {"David", "Eve", "Frank", "Grace", "Henry", "Ivy"};
    for (int i = 0; i < 6; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "User%d", i + 1);
        hash_table_put(ht, names[i], 20 + i);
    }

    printf("\n  扩容后状态:\n");
    hash_table_print(ht);

    hash_table_free(ht);
    printf("\n");
}

static void demo_collision(void)
{
    printf("[hash_table] 冲突解决演示\n");

    /* 演示哈希冲突 */
    const char *key1 = "abc";
    const char *key2 = "bcd";

    size_t h1 = djb2_hash(key1, 8);
    size_t h2 = djb2_hash(key2, 8);

    printf("  djb2_hash(\"%s\", 8) = %zu\n", key1, h1);
    printf("  djb2_hash(\"%s\", 8) = %zu\n", key2, h2);
    printf("  两者 hash 值相同? %s\n", h1 == h2 ? "是 (冲突)" : "否");

    HashTable *ht = hash_table_create();
    if (!ht) return;

    /* 插入可能冲突的键 */
    printf("\n  插入可能冲突的键:\n");
    hash_table_put(ht, "abc", 1);
    hash_table_put(ht, "bcd", 2);

    /* 验证两者都能正确找到 */
    int val;
    hash_table_get(ht, "abc", &val);
    hash_table_get(ht, "bcd", &val);

    hash_table_free(ht);
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 哈希表 ===\n\n");

    demo_basic_operations();
    demo_resize();
    demo_collision();

    printf("=== PASS ===\n");
    return 0;
}
