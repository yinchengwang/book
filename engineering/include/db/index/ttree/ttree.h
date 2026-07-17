/*
 * ttree.h
 *
 * T-Tree 索引公共 API。
 *
 * T-Tree 是一种专为内存优化的多路搜索树，结合了二叉搜索树的简洁性和
 * B-Tree 的高效性。典型应用于内存数据库如 TimesTen、MySQL NDB Cluster。
 *
 * 特点：
 *   - 每个节点存储 [min_keys, 2*min_keys-1] 个 key
 *   - 节点既可以是叶子也可以是内部节点
 *   - 叶子节点通过 prev/next 链表连接，支持高效范围查询
 *   - 支持用户自定义比较函数
 */

#ifndef DB_INDEX_TTREE_H
#define DB_INDEX_TTREE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TTREE_DEFAULT_MIN_KEYS 4u
#define TTREE_MIN_MIN_KEYS 2u

/* key 比较函数类型。返回负数表示 lhs < rhs，0 表示相等，正数表示 lhs > rhs。 */
typedef int (*ttree_compare_fn)(const void *lhs, uint32_t lhs_len,
                                const void *rhs, uint32_t rhs_len,
                                void *ctx);

/* T-Tree 索引 opaque 类型。 */
typedef struct ttree_index ttree_index_t;

/*
 * 回调函数类型，用于范围查询。
 *   key/keylen: 键
 *   value/valuelen: 值
 *   ctx: 用户上下文
 * 返回值: 0 表示继续遍历，非 0 表示停止遍历
 */
typedef int (*ttree_range_callback_fn)(const void *key, uint32_t keylen,
                                        const void *value, uint32_t valuelen,
                                        void *ctx);

/**
 * 创建 T-Tree 索引。
 *
 * @param min_keys 每个节点最小 key 数（节点最多存储 2*min_keys-1 个 key）
 * @param compare 自定义比较函数，NULL 则使用字节序比较
 * @param compare_ctx 比较函数的上下文参数
 * @return 新建的索引，失败返回 NULL
 */
ttree_index_t *ttree_index_create(uint32_t min_keys,
                                  ttree_compare_fn compare,
                                  void *compare_ctx);

/**
 * 销毁 T-Tree 索引，释放所有内存。
 *
 * @param index 要销毁的索引
 */
void ttree_index_drop(ttree_index_t *index);

/**
 * 插入 key-value。
 *
 * @param index 索引
 * @param key 键（不可为 NULL）
 * @param keylen 键长度
 * @param value 值（可为 NULL 表示删除）
 * @param valuelen 值长度
 * @return 0 成功，-1 失败
 */
int ttree_index_insert(ttree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void *value, uint32_t valuelen);

/**
 * 删除指定 key。
 *
 * @param index 索引
 * @param key 键
 * @param keylen 键长度
 * @return 0 成功删除，-1 未找到或失败
 */
int ttree_index_delete(ttree_index_t *index,
                       const void *key, uint32_t keylen);

/**
 * 查找 key 对应的 value。
 *
 * @param index 索引
 * @param key 键
 * @param keylen 键长度
 * @param value_out 输出：值指针（外部不负责释放）
 * @param valuelen_out 输出：值长度
 * @return 0 找到，-1 未找到
 */
int ttree_index_lookup(const ttree_index_t *index,
                       const void *key, uint32_t keylen,
                       const void **value_out, uint32_t *valuelen_out);

/**
 * 范围查询 [min_key, max_key]。
 *
 * @param index 索引
 * @param min_key 下界键（可为 NULL 表示负无穷）
 * @param min_keylen 下界键长度
 * @param max_key 上界键（可为 NULL 表示正无穷）
 * @param max_keylen 上界键长度
 * @param callback 回调函数
 * @param ctx 用户上下文
 * @return 遍历的元素个数，-1 表示失败
 */
int ttree_index_range(const ttree_index_t *index,
                      const void *min_key, uint32_t min_keylen,
                      const void *max_key, uint32_t max_keylen,
                      ttree_range_callback_fn callback,
                      void *ctx);

/**
 * 获取索引中的记录数。
 *
 * @param index 索引
 * @return 记录数
 */
uint32_t ttree_index_size(const ttree_index_t *index);

/**
 * 获取索引树的高度。
 *
 * @param index 索引
 * @return 树高（根节点高度为 1）
 */
uint32_t ttree_index_height(const ttree_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_TTREE_H */
