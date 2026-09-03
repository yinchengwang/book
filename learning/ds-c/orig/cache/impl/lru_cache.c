/*
 * lru_cache.c —— LRU 缓存实现（哈希表 + 双向链表）
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   struct ds_lru_node {
 *       void               *key;
 *       void               *value;
 *       struct ds_lru_node *prev;
 *       struct ds_lru_node *next;
 *   };
 *
 *   struct ds_lru_cache {
 *       ds_lru_node_t  **buckets;     // 哈希桶
 *       size_t           bucket_count;
 *       ds_lru_node_t    *head;       // 双向链表头（最近使用）
 *       ds_lru_node_t    *tail;       // 双向链表尾（最久未使用）
 *       size_t           size;
 *       size_t           capacity;
 *       size_t           key_size;
 *       size_t           value_size;
 *       ds_lru_hash_fn   hash;
 *       ds_lru_equals_fn equals;
 *   };
 *
 * ============================================================
 * 双向链表的 4 个基本操作（所有 O(1)）
 * ============================================================
 *
 * 【remove_node】从链表中摘除节点
 *   修改 node->prev->next = node->next
 *   修改 node->next->prev = node->prev
 *
 * 【add_to_head】将节点插入链表头部
 *   node->next = head
 *   node->prev = NULL
 *   head->prev = node
 *   head = node
 *
 * 【move_to_head】将已有节点移到头部 = remove_node + add_to_head
 *
 * 【remove_tail】淘汰尾部节点 = remove_node(tail)
 *
 * ============================================================
 * get(key) 流程
 * ============================================================
 *   1. 计算 hash(key)，定位桶
 *   2. 在桶的链表中查找 key（用 equals 比较）
 *   3. 若找到：move_to_head(node)，返回 node->value
 *   4. 若未找到：返回 NULL
 *
 * ============================================================
 * put(key, value) 流程
 * ============================================================
 *   1. 计算 hash(key)，定位桶
 *   2. 在桶的链表中查找 key：
 *      若找到：更新 node->value，move_to_head(node)
 *      若未找到：
 *        a. 若 size == capacity：remove_tail()，从哈希桶中摘除
 *        b. 创建新节点，add_to_head，插入哈希桶
 *        c. size++
 */

#include <ds/lru_cache.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LRU_DEFAULT_BUCKETS 16u

typedef struct ds_lru_node {
    void               *key;
    void               *value;
    struct ds_lru_node *prev;
    struct ds_lru_node *next;
    struct ds_lru_node *hash_next; /* 哈希桶链表的下一个节点 */
} ds_lru_node_t;

struct ds_lru_cache {
    ds_lru_node_t  **buckets;
    size_t           bucket_count;
    ds_lru_node_t   *head;
    ds_lru_node_t   *tail;
    size_t           size;
    size_t           capacity;
    size_t           key_size;
    size_t           value_size;
    ds_lru_hash_fn   hash;
    ds_lru_equals_fn equals;
};

/* 创建节点 */
static ds_lru_node_t *lru_node_create(size_t key_size, size_t value_size,
                                        const void *key, const void *value)
{
    ds_lru_node_t *node;

    node = (ds_lru_node_t *)calloc(1u, sizeof(ds_lru_node_t));
    if (!node) return NULL;

    node->key = malloc(key_size);
    node->value = malloc(value_size);
    if (!node->key || !node->value) {
        free(node->key);
        free(node->value);
        free(node);
        return NULL;
    }
    memcpy(node->key, key, key_size);
    memcpy(node->value, value, value_size);
    return node;
}

static void lru_node_free(ds_lru_node_t *node)
{
    if (!node) return;
    free(node->key);
    free(node->value);
    free(node);
}

/* 从双向链表中移除节点 */
static void lru_remove_node(ds_lru_cache_t *cache, ds_lru_node_t *node)
{
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        cache->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        cache->tail = node->prev;
    }
}

/* 将节点添加到双向链表头部 */
static void lru_add_to_head(ds_lru_cache_t *cache, ds_lru_node_t *node)
{
    node->prev = NULL;
    node->next = cache->head;
    if (cache->head) {
        cache->head->prev = node;
    }
    cache->head = node;
    if (!cache->tail) {
        cache->tail = node;
    }
}

/* 将节点移到头部 */
static void lru_move_to_head(ds_lru_cache_t *cache, ds_lru_node_t *node)
{
    lru_remove_node(cache, node);
    lru_add_to_head(cache, node);
}

/* 淘汰尾部节点 */
static void lru_evict(ds_lru_cache_t *cache)
{
    ds_lru_node_t *node;

    if (!cache->tail) return;

    node = cache->tail;
    lru_remove_node(cache, node);

    /* 从哈希桶中移除 */
    size_t bucket = cache->hash(node->key) % cache->bucket_count;
    ds_lru_node_t **pp = &cache->buckets[bucket];
    while (*pp) {
        if (*pp == node) {
            *pp = node->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    lru_node_free(node);
    cache->size -= 1u;
}

/* 公共 API */
ds_lru_cache_t *ds_lru_cache_create(size_t capacity, size_t key_size, size_t value_size,
                                     ds_lru_hash_fn hash, ds_lru_equals_fn equals)
{
    ds_lru_cache_t *cache;

    if (capacity == 0u || key_size == 0u || value_size == 0u || !hash || !equals) {
        return NULL;
    }

    cache = (ds_lru_cache_t *)calloc(1u, sizeof(ds_lru_cache_t));
    if (!cache) return NULL;

    cache->bucket_count = LRU_DEFAULT_BUCKETS;
    cache->buckets = (ds_lru_node_t **)calloc(cache->bucket_count, sizeof(ds_lru_node_t *));
    if (!cache->buckets) { free(cache); return NULL; }

    cache->capacity = capacity;
    cache->key_size = key_size;
    cache->value_size = value_size;
    cache->hash = hash;
    cache->equals = equals;
    return cache;
}

void ds_lru_cache_destroy(ds_lru_cache_t *cache)
{
    if (!cache) return;

    ds_lru_node_t *curr = cache->head;
    while (curr) {
        ds_lru_node_t *next = curr->next;
        lru_node_free(curr);
        curr = next;
    }
    free(cache->buckets);
    free(cache);
}

const void *ds_lru_cache_get(ds_lru_cache_t *cache, const void *key)
{
    ds_lru_node_t *curr;

    if (!cache || !key) return NULL;

    size_t bucket = cache->hash(key) % cache->bucket_count;
    curr = cache->buckets[bucket];

    while (curr) {
        if (cache->equals(curr->key, key)) {
            /* 找到：移到头部（标记为最近使用） */
            lru_move_to_head(cache, curr);
            return curr->value;
        }
        curr = curr->hash_next;
    }

    return NULL;
}

int ds_lru_cache_put(ds_lru_cache_t *cache, const void *key, const void *value)
{
    ds_lru_node_t *curr;

    if (!cache || !key || !value) return -1;

    size_t bucket = cache->hash(key) % cache->bucket_count;

    /* 检查 key 是否已存在 */
    curr = cache->buckets[bucket];
    while (curr) {
        if (cache->equals(curr->key, key)) {
            /* 更新值并移到头部 */
            memcpy(curr->value, value, cache->value_size);
            lru_move_to_head(cache, curr);
            return 0;
        }
        curr = curr->hash_next;
    }

    /* key 不存在 —— 需要插入 */
    /* 如果满了，先淘汰 */
    if (cache->size >= cache->capacity) {
        lru_evict(cache);
    }

    ds_lru_node_t *node = lru_node_create(cache->key_size, cache->value_size, key, value);
    if (!node) return -1;

    lru_add_to_head(cache, node);

    /* 插入哈希桶（头插法） */
    node->hash_next = cache->buckets[bucket];
    cache->buckets[bucket] = node;

    cache->size += 1u;
    return 0;
}

int ds_lru_cache_remove(ds_lru_cache_t *cache, const void *key)
{
    if (!cache || !key) return -1;

    size_t bucket = cache->hash(key) % cache->bucket_count;
    ds_lru_node_t **pp = &cache->buckets[bucket];

    while (*pp) {
        if (cache->equals((*pp)->key, key)) {
            ds_lru_node_t *node = *pp;
            *pp = node->hash_next;
            lru_remove_node(cache, node);
            lru_node_free(node);
            cache->size -= 1u;
            return 0;
        }
        pp = &(*pp)->hash_next;
    }

    return -1;
}

size_t ds_lru_cache_size(const ds_lru_cache_t *cache) { return cache ? cache->size : 0u; }
bool ds_lru_cache_empty(const ds_lru_cache_t *cache) { return !cache || cache->size == 0u; }

/* ============================================================
 * 演示函数
 * ============================================================ */

static unsigned long long lru_int_hash(const void *key) { return (unsigned long long)*(const int *)key; }
static bool lru_int_equals(const void *a, const void *b) { return *(const int *)a == *(const int *)b; }

void ds_lru_cache_demo(void)
{
    printf("========== LRU 缓存演示（capacity=3） ==========\n");

    ds_lru_cache_t *cache = ds_lru_cache_create(3, sizeof(int), sizeof(char) * 16,
                                                  lru_int_hash, lru_int_equals);
    if (!cache) { printf("创建失败\n"); return; }

    /* 手机访问顺序模拟 */
    printf("\n【模拟数据访问】\n");
    int k1 = 1; ds_lru_cache_put(cache, &k1, "user_1");
    printf("put(1, user_1) → size=%zu\n", ds_lru_cache_size(cache));

    int k2 = 2; ds_lru_cache_put(cache, &k2, "user_2");
    printf("put(2, user_2) → size=%zu\n", ds_lru_cache_size(cache));

    int k3 = 3; ds_lru_cache_put(cache, &k3, "user_3");
    printf("put(3, user_3) → size=%zu（满！）\n", ds_lru_cache_size(cache));

    printf("get(1) → %s（1 被移到头部）\n", (const char *)ds_lru_cache_get(cache, &k1));

    int k4 = 4; ds_lru_cache_put(cache, &k4, "user_4");
    printf("put(4, user_4) → size=%zu（淘汰了最久未用的 2）\n", ds_lru_cache_size(cache));

    printf("get(2) → %s\n",
           ds_lru_cache_get(cache, &k2) ? (const char *)ds_lru_cache_get(cache, &k2) : "不存在（已被淘汰）");
    printf("get(1) → %s\n", (const char *)ds_lru_cache_get(cache, &k1));

    ds_lru_cache_destroy(cache);
    printf("========== LRU 缓存演示结束 ==========\n");
}
