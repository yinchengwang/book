/*
 * lru_cache.h —— LRU 缓存（Least Recently Used）
 *
 * ============================================================
 * 概述
 * ============================================================
 * LRU（最近最少使用）缓存是一种常用的缓存淘汰策略。
 * 当缓存容量满时，淘汰"最久未被访问"的元素。
 *
 * 核心思想：如果一个数据最近被访问过，那么它在未来被访问的概率也更高。
 *
 * 实现方案：哈希表 + 双向链表
 * - 哈希表：O(1) 快速查找 key 对应的节点
 * - 双向链表：维护访问顺序（最近访问的放在头部，最久未访问的在尾部）
 *
 * ============================================================
 * LRU 访问序列示例（容量=3）
 * ============================================================
 *
 *   put(1, A)   链表: [1]           缓存: {1:A}
 *   put(2, B)   链表: [2→1]         缓存: {1:A, 2:B}
 *   put(3, C)   链表: [3→2→1]       缓存: {1:A, 2:B, 3:C}  满！
 *   get(1)      链表: [1→3→2]       缓存: {1:A, 2:B, 3:C}  1 被移到头部
 *   put(4, D)   链表: [4→1→3]       缓存: {1:A, 3:C, 4:D}  淘汰 2（尾部）
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作   | 复杂度 | 说明                                      |
 * |-------|--------|------------------------------------------|
 * | get   | O(1)   | 查找 key，将该节点移到链表头部              |
 * | put   | O(1)   | 插入/更新，若满则淘汰尾部节点              |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 操作系统页面置换算法
 * - 数据库缓冲池（Buffer Pool）
 * - Redis 的内存淘汰策略（allkeys-lru）
 * - CDN 缓存策略
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. LRU 的实现为什么用哈希表+双向链表？→ 哈希表 O(1) 查找，双向链表 O(1) 插入/删除/移动。
 * 2. LRU vs LFU？→ LRU 关注"最近性"，LFU 关注"频率"。热点数据用 LFU 更稳定。
 * 3. 如何线程安全地实现 LRU？→ 使用读写锁或 ConcurrentHashMap + 锁分段。
 */

#ifndef DS_LRU_CACHE_H
#define DS_LRU_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_lru_cache ds_lru_cache_t;

/*
 * 创建 LRU 缓存。
 * capacity: 最大容量
 * key_size / value_size: key 和 value 的字节大小
 * hash / equals: key 的哈希和比较函数
 */
typedef unsigned long long (*ds_lru_hash_fn)(const void *key);
typedef bool (*ds_lru_equals_fn)(const void *a, const void *b);

ds_lru_cache_t *ds_lru_cache_create(size_t capacity,
                                     size_t key_size,
                                     size_t value_size,
                                     ds_lru_hash_fn hash,
                                     ds_lru_equals_fn equals);
void ds_lru_cache_destroy(ds_lru_cache_t *cache);

/* 获取 key 对应的 value，访问后该 key 被标记为最近使用。返回 NULL 表示不存在 */
const void *ds_lru_cache_get(ds_lru_cache_t *cache, const void *key);

/* 放入 key-value。若 key 已存在则更新 value */
int ds_lru_cache_put(ds_lru_cache_t *cache, const void *key, const void *value);

/* 移除 key */
int ds_lru_cache_remove(ds_lru_cache_t *cache, const void *key);

size_t ds_lru_cache_size(const ds_lru_cache_t *cache);
bool ds_lru_cache_empty(const ds_lru_cache_t *cache);

/* 演示函数 */
void ds_lru_cache_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_LRU_CACHE_H */
