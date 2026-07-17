/*
 * ds_hash_table.c —— 拉链法哈希表演示（学习层独立实现）
 *
 * ============================================================
 * 本文件是学习层独立实现的拉链法哈希表 demo，用于：
 * 1. 演示哈希表的核心操作（insert/search/delete）
 * 2. 演示装载因子 + rehash 触发（动态扩容到 2 倍桶数）
 * 3. 用英文词频统计 demo 展示 string->int 映射场景
 * 4. 不依赖 algo-prod 的 map_t 或 utash 宏（自定义简单实现）
 *
 * 历史：
 * - S5 实施双轨纪律时 hash/impl/hash_table.c 被删（依赖 algo-prod/map/map.h）
 * - S7 重新独立实现（专用拉链法，不复用 utash 宏系统）
 *
 * 设计选择：
 * - 桶数 = 16（初始），rehash 触发条件：元素数 >= 桶数 * 0.75
 * - 哈希函数：djb2（Dan Bernstein）—— 经典简单分布良好
 * - 键类型：字符串；值类型：int
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "ds/hash_table.h"

#define DS_HT_INITIAL_BUCKETS 16
#define DS_HT_LOAD_FACTOR_NUM 3   /* 装载因子 = 3/4 = 0.75 */
#define DS_HT_LOAD_FACTOR_DEN 4

/*
 * 哈希表节点：链地址法（chaining）
 */
typedef struct ds_ht_node {
    char *key;             /* strdup 出来的 key，释放时一并 free */
    int value;
    struct ds_ht_node *next;
} ds_ht_node_t;

/*
 * 哈希表：桶数组 + 元素计数
 */
typedef struct {
    ds_ht_node_t **buckets;
    size_t bucket_count;
    size_t size;
} ds_ht_t;

static ds_ht_t *ds_ht_create(void);
static void ds_ht_destroy(ds_ht_t *ht);
static int ds_ht_insert(ds_ht_t *ht, const char *key, int value);
static bool ds_ht_search(const ds_ht_t *ht, const char *key, int *value);
static int ds_ht_delete(ds_ht_t *ht, const char *key);
static size_t ds_ht_size(const ds_ht_t *ht);

/*
 * djb2 哈希函数
 * 来自：http://www.cse.yorku.ca/~oz/hash.html
 */
static unsigned long ds_ht_hash(const char *key, size_t bucket_count) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + (unsigned char)c;  /* hash * 33 + c */
    }
    return hash % bucket_count;
}

/*
 * 创建新桶数组，并把旧表所有 entry 重新插入（新位置）。
 */
static int ds_ht_rehash(ds_ht_t *ht, size_t new_bucket_count) {
    ds_ht_node_t **new_buckets = (ds_ht_node_t **)calloc(new_bucket_count, sizeof(ds_ht_node_t *));
    if (!new_buckets) {
        return -1;
    }
    /* 把旧桶所有节点搬到新桶 */
    for (size_t i = 0; i < ht->bucket_count; i++) {
        ds_ht_node_t *node = ht->buckets[i];
        while (node) {
            ds_ht_node_t *next = node->next;
            size_t idx = ds_ht_hash(node->key, new_bucket_count);
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            node = next;
        }
    }
    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->bucket_count = new_bucket_count;
    return 0;
}

static ds_ht_t *ds_ht_create(void) {
    ds_ht_t *ht = (ds_ht_t *)calloc(1, sizeof(ds_ht_t));
    if (!ht) {
        return NULL;
    }
    ht->buckets = (ds_ht_node_t **)calloc(DS_HT_INITIAL_BUCKETS, sizeof(ds_ht_node_t *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    ht->bucket_count = DS_HT_INITIAL_BUCKETS;
    ht->size = 0;
    return ht;
}

static void ds_ht_destroy(ds_ht_t *ht) {
    if (!ht) {
        return;
    }
    for (size_t i = 0; i < ht->bucket_count; i++) {
        ds_ht_node_t *node = ht->buckets[i];
        while (node) {
            ds_ht_node_t *next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

static int ds_ht_insert(ds_ht_t *ht, const char *key, int value) {
    if (!ht || !key) {
        return -1;
    }
    /* 检查是否存在，存在则更新 */
    size_t idx = ds_ht_hash(key, ht->bucket_count);
    for (ds_ht_node_t *n = ht->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            n->value = value;
            return 0;
        }
    }
    /* 不存在则插入头 */
    ds_ht_node_t *node = (ds_ht_node_t *)calloc(1, sizeof(ds_ht_node_t));
    if (!node) {
        return -1;
    }
    node->key = strdup(key);
    if (!node->key) {
        free(node);
        return -1;
    }
    node->value = value;
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
    ht->size++;

    /* 检查装载因子，必要时 rehash */
    if (ht->size * DS_HT_LOAD_FACTOR_DEN >= ht->bucket_count * DS_HT_LOAD_FACTOR_NUM) {
        if (ds_ht_rehash(ht, ht->bucket_count * 2) != 0) {
            /* rehash 失败不致命（仍可用），仅警告 */
            fprintf(stderr, "warning: ht rehash failed\n");
        }
    }
    return 0;
}

static bool ds_ht_search(const ds_ht_t *ht, const char *key, int *value) {
    if (!ht || !key) {
        return false;
    }
    size_t idx = ds_ht_hash(key, ht->bucket_count);
    for (ds_ht_node_t *n = ht->buckets[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (value) {
                *value = n->value;
            }
            return true;
        }
    }
    return false;
}

static int ds_ht_delete(ds_ht_t *ht, const char *key) {
    if (!ht || !key) {
        return -1;
    }
    size_t idx = ds_ht_hash(key, ht->bucket_count);
    ds_ht_node_t *prev = NULL;
    for (ds_ht_node_t *n = ht->buckets[idx]; n; prev = n, n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (prev) {
                prev->next = n->next;
            } else {
                ht->buckets[idx] = n->next;
            }
            free(n->key);
            free(n);
            ht->size--;
            return 0;
        }
    }
    return -1;  /* 没找到 */
}

static size_t ds_ht_size(const ds_ht_t *ht) {
    return ht ? ht->size : 0;
}

/*
 * 演示函数：英文词频统计（拆分 → 标准化 → 计数 → 输出 top）
 * 输入：短语数组，每个短语拆分出单词（按空格）并转为小写
 */
void ds_hash_table_demo(void) {
    printf("\n=== ds_hash_table_demo (拉链法 + 词频统计) ===\n");

    ds_ht_t *ht = ds_ht_create();
    if (!ht) {
        printf("  创建失败\n");
        return;
    }

    const char *docs[] = {
        "the quick brown fox",
        "jumps over the lazy dog",
        "the fox is quick",
    };

    /* 分词 + 标准化 + 计数 */
    for (size_t i = 0; i < sizeof(docs) / sizeof(docs[0]); i++) {
        char buf[256];
        strncpy(buf, docs[i], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *saveptr = NULL;
        char *tok = strtok_r(buf, " ", &saveptr);
        while (tok) {
            /* 转小写 */
            for (char *p = tok; *p; p++) {
                if (*p >= 'A' && *p <= 'Z') {
                    *p = (char)(*p + ('a' - 'A'));
                }
            }
            int curr = 0;
            ds_ht_search(ht, tok, &curr);
            ds_ht_insert(ht, tok, curr + 1);
            tok = strtok_r(NULL, " ", &saveptr);
        }
    }

    printf("  size=%zu, bucket_count=%zu\n", ds_ht_size(ht), ht->bucket_count);

    /* 查询关键单词频次 */
    const char *queries[] = { "the", "fox", "quick", "dog", "missing" };
    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); i++) {
        int v = 0;
        bool found = ds_ht_search(ht, queries[i], &v);
        printf("  %-8s -> %s (count=%d)\n",
               queries[i], found ? "FOUND" : "MISS", v);
    }

    /* 删除一个 key 再查 */
    ds_ht_delete(ht, "fox");
    int v;
    bool still = ds_ht_search(ht, "fox", &v);
    printf("  删除 'fox' 后再查 -> %s\n", still ? "STILL" : "GONE");

    ds_ht_destroy(ht);
    printf("=== ds_hash_table_demo 结束 ===\n\n");
}
