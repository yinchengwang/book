/*
 * skip_list.h —— 跳表（Skip List）
 *
 * ============================================================
 * 概述
 * ============================================================
 * 跳表是一种随机化的数据结构，可以看作是在有序链表基础上增加了
 * 多层"快车道"（索引层），从而实现 O(log n) 的查找、插入和删除。
 * 它是由 William Pugh 在 1989 年提出的，是平衡树的一种概率性替代方案。
 *
 * 跳表的核心思想：以空间换时间。通过为部分节点增加额外的层级指针，
 * 使得查找时可以"跳过"大量中间节点。
 *
 * 跳表结构示意（4 层）：
 *
 *   Level 3: HEAD ──────────────────────────────> 12 ───> NIL
 *   Level 2: HEAD ───────────> 5 ──────────────> 12 ───> NIL
 *   Level 1: HEAD ───> 2 ───> 5 ───> 8 ──────> 12 ───> NIL
 *   Level 0: HEAD <-> 2 <-> 5 <-> 8 <-> 10 <-> 12 <-> NIL
 *
 * 查找 10 的过程：
 *   Level 3: HEAD → 12（10 < 12，下降到 Level 2）
 *   Level 2: HEAD → 5 → 12（10 < 12，从 5 下降到 Level 1）
 *   Level 1: 5 → 8 → 12（10 < 12，从 8 下降到 Level 0）
 *   Level 0: 8 → 10 ✓
 *
 * 层高随机生成（抛硬币策略）：
 *   每个节点的层高由随机函数决定，新增一层概率为 p（通常 p=0.5）。
 *   这保证了高层节点稀疏、低层节点密集的分布，期望层数为 O(log n)。
 *
 * ============================================================
 * 跳表 vs 平衡树
 * ============================================================
 * | 维度     | 跳表                 | 平衡树（红黑树/AVL）   |
 * |---------|---------------------|----------------------|
 * | 实现复杂度 | 简单（~80行）         | 复杂（旋转逻辑繁琐）     |
 * | 平衡保证 | 概率性（期望 O(log n)）| 确定性 O(log n)       |
 * | 范围查询 | 天然支持              | 中序遍历实现            |
 * | 并发友好 | 容易加锁              | 需要全局或子树级锁      |
 * | 内存     | 平均每节点 2 个指针   | 每节点 3 个指针（含 parent）|
 *
 * ============================================================
 * 核心操作及时间复杂度（期望）
 * ============================================================
 * | 操作   | 复杂度  | 说明                         |
 * |-------|--------|-----------------------------|
 * | search| O(log n) | 利用多层索引跳过大量节点       |
 * | insert| O(log n) | 随机生成层高，插入每层索引      |
 * | delete| O(log n) | 从每层删除节点                 |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - Redis 有序集合（Sorted Set）的底层实现之一
 * - LevelDB/RocksDB 的 MemTable
 * - 需要频繁范围查询 + 动态更新的场景
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. 跳表和平衡树各自适合什么场景？→ 跳表实现简单、范围查询高效；
 *    平衡树查找性能更稳定（确定性）。
 * 2. 为什么 Redis 用跳表实现 ZSet？→ 实现简单、支持范围查询、并发友好。
 * 3. 跳表的空间复杂度？→ O(n) 期望，每节点约 2 个指针（p=0.5 时）。
 */

#ifndef DS_SKIP_LIST_H
#define DS_SKIP_LIST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS_SKIP_LIST_MAX_LEVEL 16u /* 最大层数限制 */

typedef struct ds_skip_list ds_skiplist_t;

/* 比较函数：标准三路比较 */
typedef int (*ds_skiplist_compare_fn)(const void *a, const void *b);

ds_skiplist_t *ds_skiplist_create(size_t element_size, ds_skiplist_compare_fn compare);
void ds_skiplist_destroy(ds_skiplist_t *list);

int ds_skiplist_insert(ds_skiplist_t *list, const void *element);
int ds_skiplist_delete(ds_skiplist_t *list, const void *key);

/* 查找：返回元素指针或 NULL */
const void *ds_skiplist_search(const ds_skiplist_t *list, const void *key);
bool ds_skiplist_contains(const ds_skiplist_t *list, const void *key);

size_t ds_skiplist_size(const ds_skiplist_t *list);
bool ds_skiplist_empty(const ds_skiplist_t *list);

/* 演示函数 */
void ds_skip_list_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_SKIP_LIST_H */
