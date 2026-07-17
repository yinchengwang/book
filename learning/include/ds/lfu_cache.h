/*
 * lfu_cache.h —— LFU 缓存（Least Frequently Used）
 *
 * ============================================================
 * 概述
 * ============================================================
 * LFU（最不经常使用）缓存是一种基于访问频率的缓存淘汰策略。
 * 当缓存容量满时，淘汰"访问次数最少"的元素。
 * 如果存在多个访问次数相同的元素，则淘汰其中最久未被访问的（LRU 作为 tie-breaker）。
 *
 * 实现方案：哈希表（key → 节点） + 频率桶（每个频率维护一个双向链表）
 *
 * ============================================================
 * LFU vs LRU
 * ============================================================
 * | 维度     | LRU                      | LFU                          |
 * |---------|--------------------------|------------------------------|
 * | 淘汰依据 | 最近访问时间              | 访问频率                      |
 * | 优势     | 对突发流量响应快          | 保护历史热点数据               |
 * | 劣势     | 可能误淘汰周期性热点       | 对新数据不友好，历史数据占坑    |
 * | 适用场景 | 一般的缓存场景            | 有明确热点数据的场景           |
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作   | 复杂度 | 说明                                      |
 * |-------|--------|------------------------------------------|
 * | get   | O(1)   | 增加访问频率，节点移到 freq+1 的桶中       |
 * | put   | O(1)   | 插入/更新，若满则淘汰最小频率桶的尾部节点   |
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. LFU 如何解决"历史数据占坑"问题？→ 定期衰减频率、或使用时间窗口内的频率。
 * 2. Redis 的 LFU 实现？→ 使用对数计数器 + 时间衰减，避免精确计数开销。
 * 3. LFU 和 LRU 哪个好？→ 看数据访问模式：有明显热点用 LFU，随机访问用 LRU。
 */

#ifndef DS_LFU_CACHE_H
#define DS_LFU_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long (*ds_lfu_hash_fn)(const void *key);
typedef bool (*ds_lfu_equals_fn)(const void *a, const void *b);

typedef struct ds_lfu_cache ds_lfu_cache_t;

ds_lfu_cache_t *ds_lfu_cache_create(size_t capacity,
                                     size_t key_size,
                                     size_t value_size,
                                     ds_lfu_hash_fn hash,
                                     ds_lfu_equals_fn equals);
void ds_lfu_cache_destroy(ds_lfu_cache_t *cache);

const void *ds_lfu_cache_get(ds_lfu_cache_t *cache, const void *key);
int ds_lfu_cache_put(ds_lfu_cache_t *cache, const void *key, const void *value);

size_t ds_lfu_cache_size(const ds_lfu_cache_t *cache);
bool ds_lfu_cache_empty(const ds_lfu_cache_t *cache);

/* 演示函数 */
void ds_lfu_cache_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_LFU_CACHE_H */
