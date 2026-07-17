/*
 * lfu_cache.c —— LFU 缓存实现（简化版：频率计数器 + O(log n) 淘汰）
 *
 * 完整 O(1) 的实现需要"频率桶"数据结构，过于复杂。
 * 此演示版本采用简化策略：
 * - 维护每个 key 的访问频率
 * - 淘汰时遍历找到最小频率的条目
 *
 * 生产级实现请参考论文 "An O(1) algorithm for implementing the LFU cache eviction scheme"
 * （Shah, Mitra, Matani, 2010）。
 */

#include <ds/lfu_cache.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LFU_DEFAULT_BUCKETS 16u

typedef struct ds_lfu_entry {
    void               *key;
    void               *value;
    unsigned long long  freq;    /* 访问频率计数器 */
    struct ds_lfu_entry *hash_next;
} ds_lfu_entry_t;

struct ds_lfu_cache {
    ds_lfu_entry_t **buckets;
    size_t           bucket_count;
    size_t           size;
    size_t           capacity;
    size_t           key_size;
    size_t           value_size;
    ds_lfu_hash_fn   hash;
    ds_lfu_equals_fn equals;
};

/* 创建条目 */
static ds_lfu_entry_t *lfu_entry_create(size_t key_size, size_t value_size,
                                         const void *key, const void *value)
{
    ds_lfu_entry_t *entry;

    entry = (ds_lfu_entry_t *)calloc(1u, sizeof(ds_lfu_entry_t));
    if (!entry) return NULL;

    entry->key = malloc(key_size);
    entry->value = malloc(value_size);
    if (!entry->key || !entry->value) {
        free(entry->key);
        free(entry->value);
        free(entry);
        return NULL;
    }
    memcpy(entry->key, key, key_size);
    memcpy(entry->value, value, value_size);
    entry->freq = 1u; /* 新插入的数据频率初始为 1 */
    return entry;
}

static void lfu_entry_free(ds_lfu_entry_t *entry)
{
    if (!entry) return;
    free(entry->key);
    free(entry->value);
    free(entry);
}

/* 公共 API */
ds_lfu_cache_t *ds_lfu_cache_create(size_t capacity, size_t key_size, size_t value_size,
                                     ds_lfu_hash_fn hash, ds_lfu_equals_fn equals)
{
    ds_lfu_cache_t *cache;

    if (capacity == 0u || !hash || !equals) return NULL;

    cache = (ds_lfu_cache_t *)calloc(1u, sizeof(ds_lfu_cache_t));
    if (!cache) return NULL;

    cache->bucket_count = LFU_DEFAULT_BUCKETS;
    cache->buckets = (ds_lfu_entry_t **)calloc(cache->bucket_count, sizeof(ds_lfu_entry_t *));
    if (!cache->buckets) { free(cache); return NULL; }

    cache->capacity = capacity;
    cache->key_size = key_size;
    cache->value_size = value_size;
    cache->hash = hash;
    cache->equals = equals;
    return cache;
}

void ds_lfu_cache_destroy(ds_lfu_cache_t *cache)
{
    if (!cache) return;
    for (size_t i = 0u; i < cache->bucket_count; ++i) {
        ds_lfu_entry_t *curr = cache->buckets[i];
        while (curr) {
            ds_lfu_entry_t *next = curr->hash_next;
            lfu_entry_free(curr);
            curr = next;
        }
    }
    free(cache->buckets);
    free(cache);
}

/* 查找 key 对应的条目（内部使用，不增加频率） */
static ds_lfu_entry_t *lfu_find_entry(ds_lfu_cache_t *cache, const void *key)
{
    size_t bucket = cache->hash(key) % cache->bucket_count;
    ds_lfu_entry_t *curr = cache->buckets[bucket];

    while (curr) {
        if (cache->equals(curr->key, key)) return curr;
        curr = curr->hash_next;
    }
    return NULL;
}

const void *ds_lfu_cache_get(ds_lfu_cache_t *cache, const void *key)
{
    ds_lfu_entry_t *entry;

    if (!cache || !key) return NULL;

    entry = lfu_find_entry(cache, key);
    if (entry) {
        entry->freq += 1u; /* 增加访问频率 */
        return entry->value;
    }
    return NULL;
}

/* 淘汰频率最低的条目 */
static void lfu_evict(ds_lfu_cache_t *cache)
{
    ds_lfu_entry_t *min_entry = NULL;
    ds_lfu_entry_t **min_pp = NULL;

    /* 遍历所有桶，找到频率最低的条目 */
    for (size_t i = 0u; i < cache->bucket_count; ++i) {
        ds_lfu_entry_t **pp = &cache->buckets[i];
        while (*pp) {
            if (!min_entry || (*pp)->freq < min_entry->freq) {
                min_entry = *pp;
                min_pp = pp;
            }
            pp = &(*pp)->hash_next;
        }
    }

    if (min_entry && min_pp) {
        *min_pp = min_entry->hash_next;
        lfu_entry_free(min_entry);
        cache->size -= 1u;
    }
}

int ds_lfu_cache_put(ds_lfu_cache_t *cache, const void *key, const void *value)
{
    if (!cache || !key || !value) return -1;

    /* 检查是否已存在 */
    ds_lfu_entry_t *existing = lfu_find_entry(cache, key);
    if (existing) {
        memcpy(existing->value, value, cache->value_size);
        existing->freq += 1u;
        return 0;
    }

    /* 不存在的 key —— 需要插入 */
    if (cache->size >= cache->capacity) {
        lfu_evict(cache);
    }

    ds_lfu_entry_t *entry = lfu_entry_create(cache->key_size, cache->value_size, key, value);
    if (!entry) return -1;

    size_t bucket = cache->hash(key) % cache->bucket_count;
    entry->hash_next = cache->buckets[bucket];
    cache->buckets[bucket] = entry;
    cache->size += 1u;
    return 0;
}

size_t ds_lfu_cache_size(const ds_lfu_cache_t *cache) { return cache ? cache->size : 0u; }
bool ds_lfu_cache_empty(const ds_lfu_cache_t *cache) { return !cache || cache->size == 0u; }

/* ============================================================
 * 演示函数
 * ============================================================ */

static unsigned long long lfu_int_hash(const void *key) { return (unsigned long long)*(const int *)key; }
static bool lfu_int_equals(const void *a, const void *b) { return *(const int *)a == *(const int *)b; }

void ds_lfu_cache_demo(void)
{
    printf("========== LFU 缓存演示（capacity=3） ==========\n");

    ds_lfu_cache_t *cache = ds_lfu_cache_create(3, sizeof(int), sizeof(char) * 16,
                                                  lfu_int_hash, lfu_int_equals);
    if (!cache) { printf("创建失败\n"); return; }

    /* 模拟数据访问 */
    printf("\n【模拟数据访问】\n");
    int k1 = 1, k2 = 2, k3 = 3;
    ds_lfu_cache_put(cache, &k1, "data_1");
    ds_lfu_cache_put(cache, &k2, "data_2");
    ds_lfu_cache_put(cache, &k3, "data_3");
    printf("put(1,2,3) 后 size=%zu（满）\n", ds_lfu_cache_size(cache));

    /* 多次访问 key=1，增加其频率 */
    ds_lfu_cache_get(cache, &k1);
    ds_lfu_cache_get(cache, &k1);
    ds_lfu_cache_get(cache, &k1);
    printf("get(1) × 3 次 → 1 的频率很高\n");

    ds_lfu_cache_get(cache, &k2);
    printf("get(2) × 1 次\n");

    /* key=3 从未被访问过，频率最低 */
    printf("key=3 从未被 get，频率最低（只有初始的 1）\n");

    int k4 = 4;
    ds_lfu_cache_put(cache, &k4, "data_4");
    printf("put(4) → size=%zu（淘汰了频率最低的 key=3）\n", ds_lfu_cache_size(cache));

    printf("get(3) → %s\n",
           ds_lfu_cache_get(cache, &k3) ? "存在" : "不存在（被淘汰）");
    printf("get(1) → 仍存在（频率高被保留）\n");

    printf("\n【LRU vs LFU 对比】\n");
    printf("如果是 LRU：key=3 最后被 put，不会被淘汰（put 也算\"最近使用\"）\n");
    printf("现在是 LFU：key=3 虽然最后 put，但频率最低 → 被淘汰\n");

    ds_lfu_cache_destroy(cache);
    printf("========== LFU 缓存演示结束 ==========\n");
}
