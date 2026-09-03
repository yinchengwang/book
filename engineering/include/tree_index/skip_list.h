/*
 * skip_list.h
 *
 * Public API for a Skip List index.
 *
 * Skip List 是一种概率平衡的有序链表，通过随机层级实现平衡：
 * - 插入时随机决定节点层级（1 ~ MAX_LEVEL）
 * - 高层链表跳过低层链表的多个节点，加速查找
 * - 无需复杂的旋转/再平衡操作
 *
 * 应用场景：
 * - Redis ZSet（有序集合）
 * - LevelDB MemTable
 * - 并发友好的有序数据结构
 */

#ifndef SKIP_LIST_INDEX_H
#define SKIP_LIST_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SKIP_LIST_MAX_LEVEL 16
#define SKIP_LIST_DEFAULT_MAX_LEVEL 16

/* key 比较函数。返回负数表示 lhs < rhs。 */
typedef int (*skip_list_compare_fn)(const void *lhs, uint32_t lhs_len,
                                    const void *rhs, uint32_t rhs_len,
                                    void *ctx);

/* Skip List opaque 类型。 */
typedef struct skip_list skip_list_t;

/* 范围查询回调函数。返回 0 继续遍历，非 0 停止。 */
typedef int (*skip_list_range_callback_fn)(const void *key, uint32_t keylen,
                                           const void *value, uint32_t valuelen,
                                           void *ctx);

/**
 * 创建 Skip List。
 *
 * @param compare 自定义比较函数，NULL 使用字节序比较
 * @param compare_ctx 比较函数上下文
 * @param max_level 最大层级，默认 16
 * @return 新建的 Skip List，失败返回 NULL
 */
skip_list_t *skip_list_create(skip_list_compare_fn compare,
                               void *compare_ctx,
                               uint32_t max_level);

/**
 * 销毁 Skip List。
 */
void skip_list_drop(skip_list_t *list);

/**
 * 插入 key-value。
 *
 * @return 0 成功，-1 失败
 */
int skip_list_insert(skip_list_t *list,
                     const void *key, uint32_t keylen,
                     const void *value, uint32_t valuelen);

/**
 * 删除 key。
 *
 * @return 0 成功删除，-1 未找到
 */
int skip_list_delete(skip_list_t *list,
                     const void *key, uint32_t keylen);

/**
 * 查找 key。
 *
 * @return 0 找到，-1 未找到
 */
int skip_list_search(const skip_list_t *list,
                     const void *key, uint32_t keylen,
                     const void **value_out, uint32_t *valuelen_out);

/**
 * 范围查询 [min_key, max_key]。
 *
 * @return 遍历的元素个数
 */
int skip_list_range(skip_list_t *list,
                    const void *min_key, uint32_t min_keylen,
                    const void *max_key, uint32_t max_keylen,
                    skip_list_range_callback_fn callback,
                    void *ctx);

/**
 * 获取节点数量。
 */
uint32_t skip_list_size(const skip_list_t *list);

/**
 * 获取当前最大层级。
 */
uint32_t skip_list_level(const skip_list_t *list);

#ifdef __cplusplus
}
#endif

#endif /* SKIP_LIST_INDEX_H */