/*
 * cuckoo_core.c — Cuckoo Filter 核心实现
 *
 * ============================================================
 * 原理
 * ============================================================
 *
 * Cuckoo Filter 使用 Cuckoo Hashing 的思想：
 *
 * 1. 每个元素生成一个指纹（fingerprint）
 * 2. 元素有两个候选桶位置：
 *    - h1 = hash(key) % bucket_count
 *    - h2 = h1 XOR hash(fingerprint)
 * 3. 插入时尝试两个候选桶的空槽位
 * 4. 如果都满了，随机踢出一个桶中的元素，被踢出的元素重新插入其另一个位置
 *
 * 这种踢出机制保证了 Cuckoo Filter 可以在高负载下工作。
 */

#include <db/index/hash/cuckoo.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * 内部数据结构
 * ----------------------------------------------------------------------- */

struct cuckoo_filter {
    cuckoo_bucket_t *buckets;     /* 桶数组 */
    size_t           bucket_count; /* 桶数量 */
    size_t           item_count;   /* 已添加元素数量 */
};

/* -----------------------------------------------------------------------
 * 哈希函数
 * ----------------------------------------------------------------------- */

/*
 * base_hash — FNV-1a 哈希函数
 *
 * 返回 32 位哈希值，用于计算桶位置和指纹。
 */
static uint32_t base_hash(const void *key, size_t keylen)
{
    const uint8_t *data = (const uint8_t *)key;
    uint32_t       hash = 2166136261u;
    size_t         i;

    for (i = 0; i < keylen; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

/*
 * get_fingerprint — 从哈希值提取指纹
 *
 * 指纹大小为 CUCKOO_FP_SIZE 字节。
 */
static uint8_t get_fingerprint(uint32_t hash)
{
    return (uint8_t)(hash & 0xFF);
}

/*
 * get_buckets — 计算元素的两个候选桶位置
 *
 * h1 = hash(key) % bucket_count
 * h2 = h1 XOR hash(fingerprint)
 */
static void get_buckets(const cuckoo_filter_t *filter,
                       uint32_t hash, size_t *h1, size_t *h2)
{
    uint8_t fp = get_fingerprint(hash);

    *h1 = (size_t)(hash % (uint32_t)filter->bucket_count);
    *h2 = *h1 ^ (size_t)fp;

    /* 确保 h2 在有效范围内 */
    if (*h2 >= filter->bucket_count) {
        *h2 = (*h1 + fp) % filter->bucket_count;
    }
}

/* -----------------------------------------------------------------------
 * 桶操作
 * ----------------------------------------------------------------------- */

/*
 * find_slot_with_fingerprint — 在桶中查找指定指纹
 *
 * 返回指纹所在的槽位索引（0 或 1），如果没找到返回 -1。
 */
static int find_slot_with_fingerprint(const cuckoo_bucket_t *bucket, uint8_t fp)
{
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (bucket->slots[i].occupied && bucket->slots[i].fingerprint == fp) {
            return i;
        }
    }
    return -1;
}

/*
 * find_empty_slot — 在桶中查找空槽位
 *
 * 返回第一个空槽位的索引，如果桶满返回 -1。
 */
static int find_empty_slot(const cuckoo_bucket_t *bucket)
{
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (!bucket->slots[i].occupied) {
            return i;
        }
    }
    return -1;
}

/* -----------------------------------------------------------------------
 * 插入辅助函数
 * ----------------------------------------------------------------------- */

/*
 * try_insert_bucket — 尝试在桶中找到一个空槽位
 *
 * 这是一个辅助函数，用于 kick_and_reinsert 逻辑。
 * 返回: 找到空槽位返回索引，没找到返回 -1。
 */
static int try_insert_bucket(cuckoo_bucket_t *bucket, uint8_t fingerprint)
{
    for (int i = 0; i < CUCKOO_BUCKET_SIZE; i++) {
        if (!bucket->slots[i].occupied) {
            bucket->slots[i].occupied = true;
            bucket->slots[i].fingerprint = fingerprint;
            return 0;
        }
    }
    return -1;
}

/*
 * kick_and_reinsert — 踢出槽位中的元素并重新插入
 *
 * 这是 Cuckoo Hashing 的核心：随机踢出一个元素，
 * 被踢出的元素尝试插入到它的另一个候选位置。
 *
 * 返回: 成功返回 0，踢出次数超限返回 -1。
 */
static int kick_and_reinsert(cuckoo_filter_t *filter,
                            cuckoo_bucket_t *bucket, int slot_idx,
                            uint8_t fingerprint, int depth)
{
    uint32_t    kick_hash;
    size_t      h1, h2;
    cuckoo_bucket_t *other_bucket;

    if (depth >= CUCKOO_MAX_KICK) {
        return -1;
    }

    /* 记录被踢出的指纹，稍后重新插入 */
    uint8_t kicked_fp = bucket->slots[slot_idx].fingerprint;

    /* 将新元素放入这个槽位 */
    bucket->slots[slot_idx].fingerprint = fingerprint;

    /* 计算被踢出元素的候选位置 */
    kick_hash = (uint32_t)kicked_fp;
    get_buckets(filter, kick_hash, &h1, &h2);

    /* 确定被踢出元素当前所在的桶 */
    other_bucket = (bucket == filter->buckets + h1) ?
                   (filter->buckets + h2) : (filter->buckets + h1);

    /* 尝试在被踢出元素另一个候选桶的空槽位插入 */
    if (try_insert_bucket(other_bucket, kicked_fp) == 0) {
        return 0;
    }

    /* 没有空槽位，踢出随机一个并递归 */
    int kick_slot = depth % CUCKOO_BUCKET_SIZE;
    return kick_and_reinsert(filter, other_bucket, kick_slot,
                            kicked_fp, depth + 1);
}

/* -----------------------------------------------------------------------
 * 公开 API 实现
 * ----------------------------------------------------------------------- */

cuckoo_filter_t *cuckoo_create(const cuckoo_config_t *config)
{
    cuckoo_filter_t *filter;
    size_t           expected_items;
    size_t           bucket_count;

    /* 处理配置参数 */
    if (config && config->expected_items > 0) {
        expected_items = config->expected_items;
    } else {
        expected_items = 1000;
    }

    /* 计算桶数量：每个桶有 2 个槽位，预留一定空间 */
    bucket_count = (expected_items / CUCKOO_BUCKET_SIZE) + 1;
    /* 确保桶数量是 2 的幂次，便于取模操作 */
    bucket_count = 1;
    while (bucket_count < expected_items / CUCKOO_BUCKET_SIZE + 10) {
        bucket_count *= 2;
    }

    /* 分配 Filter 结构 */
    filter = (cuckoo_filter_t *)malloc(sizeof(cuckoo_filter_t));
    if (!filter) {
        return NULL;
    }

    /* 分配桶数组 */
    filter->buckets = (cuckoo_bucket_t *)calloc(bucket_count,
                                                 sizeof(cuckoo_bucket_t));
    if (!filter->buckets) {
        free(filter);
        return NULL;
    }

    filter->bucket_count = bucket_count;
    filter->item_count = 0;

    return filter;
}

void cuckoo_destroy(cuckoo_filter_t *filter)
{
    if (!filter) return;
    free(filter->buckets);
    free(filter);
}

int cuckoo_add(cuckoo_filter_t *filter, const void *key, size_t keylen)
{
    uint32_t  hash;
    size_t    h1, h2;
    int       slot;
    uint8_t   fp;

    if (!filter || !key || keylen == 0) {
        return -1;
    }

    hash = base_hash(key, keylen);
    fp = get_fingerprint(hash);
    get_buckets(filter, hash, &h1, &h2);

    /* Cuckoo Filter 不做去重检查（指纹在踢出过程中会变化）*/

    /* 尝试在 h1 位置插入 */
    slot = find_empty_slot(&filter->buckets[h1]);
    if (slot >= 0) {
        filter->buckets[h1].slots[slot].occupied = true;
        filter->buckets[h1].slots[slot].fingerprint = fp;
        filter->item_count++;
        return 0;
    }

    /* 尝试在 h2 位置插入 */
    slot = find_empty_slot(&filter->buckets[h2]);
    if (slot >= 0) {
        filter->buckets[h2].slots[slot].occupied = true;
        filter->buckets[h2].slots[slot].fingerprint = fp;
        filter->item_count++;
        return 0;
    }

    /* 两个位置都满了，开始踢出 */
    if (kick_and_reinsert(filter, &filter->buckets[h1], 0, fp, 0) == 0) {
        filter->item_count++;
        return 0;
    }

    /* 尝试从 h2 开始踢出 */
    if (kick_and_reinsert(filter, &filter->buckets[h2], 0, fp, 0) == 0) {
        filter->item_count++;
        return 0;
    }

    return -1;
}

bool cuckoo_query(const cuckoo_filter_t *filter, const void *key, size_t keylen)
{
    uint32_t  hash;
    size_t    h1, h2;
    uint8_t   fp;
    int       slot;

    if (!filter || !key || keylen == 0) {
        return false;
    }

    hash = base_hash(key, keylen);
    fp = get_fingerprint(hash);
    get_buckets(filter, hash, &h1, &h2);

    /* 在两个候选桶中查找指纹 */
    slot = find_slot_with_fingerprint(&filter->buckets[h1], fp);
    if (slot >= 0) {
        return true;
    }

    slot = find_slot_with_fingerprint(&filter->buckets[h2], fp);
    return slot >= 0;
}

int cuckoo_delete(cuckoo_filter_t *filter, const void *key, size_t keylen)
{
    uint32_t  hash;
    size_t    h1, h2;
    uint8_t   fp;
    int       slot;

    if (!filter || !key || keylen == 0) {
        return -1;
    }

    hash = base_hash(key, keylen);
    fp = get_fingerprint(hash);
    get_buckets(filter, hash, &h1, &h2);

    /* 在 h1 桶中查找并删除 */
    slot = find_slot_with_fingerprint(&filter->buckets[h1], fp);
    if (slot >= 0) {
        filter->buckets[h1].slots[slot].occupied = false;
        filter->item_count--;
        return 0;
    }

    /* 在 h2 桶中查找并删除 */
    slot = find_slot_with_fingerprint(&filter->buckets[h2], fp);
    if (slot >= 0) {
        filter->buckets[h2].slots[slot].occupied = false;
        filter->item_count--;
        return 0;
    }

    return 1;  /* 元素不存在 */
}

size_t cuckoo_get_item_count(const cuckoo_filter_t *filter)
{
    return filter ? filter->item_count : 0;
}

size_t cuckoo_get_bucket_count(const cuckoo_filter_t *filter)
{
    return filter ? filter->bucket_count : 0;
}

double cuckoo_get_load_factor(const cuckoo_filter_t *filter)
{
    if (!filter || filter->bucket_count == 0) {
        return 0.0;
    }

    size_t total_slots = filter->bucket_count * CUCKOO_BUCKET_SIZE;
    return (double)filter->item_count / (double)total_slots;
}