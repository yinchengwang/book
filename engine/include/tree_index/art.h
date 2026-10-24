/*
 * art.h
 *
 * Public API for an Adaptive Radix Tree (ART) index.
 *
 * ART 是一种自适应基数树，根据 key 前缀长度选择最优节点类型：
 * - N4: 最多 4 个子节点
 * - N16: 最多 16 个子节点
 * - N48: 最多 48 个子节点
 * - N256: 最多 256 个子节点
 *
 * 应用场景：
 * - 内存数据库索引
 * - 键值存储
 */

#ifndef ART_INDEX_H
#define ART_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ART opaque 类型。 */
typedef struct art_tree art_tree_t;

/* 范围查询回调函数。返回 0 继续遍历，非 0 停止。 */
typedef int (*art_callback_fn)(const uint8_t *key, uint32_t keylen,
                                const void *value, uint32_t valuelen,
                                void *ctx);

/**
 * 创建 ART 树。
 */
art_tree_t *art_create(void);

/**
 * 销毁 ART 树。
 */
void art_destroy(art_tree_t *tree);

/**
 * 插入 key-value。
 *
 * @return 0 成功，-1 失败
 */
int art_insert(art_tree_t *tree,
               const uint8_t *key, uint32_t keylen,
               const void *value, uint32_t valuelen);

/**
 * 删除 key。
 *
 * @return 0 成功删除，-1 未找到
 */
int art_delete(art_tree_t *tree,
               const uint8_t *key, uint32_t keylen);

/**
 * 查找 key。
 *
 * @return 0 找到，-1 未找到
 */
int art_search(const art_tree_t *tree,
               const uint8_t *key, uint32_t keylen,
               const void **value_out, uint32_t *valuelen_out);

/**
 * 范围查询 [min_key, max_key]。
 *
 * @return 匹配的数量
 */
int art_range(art_tree_t *tree,
              const uint8_t *min_key, uint32_t min_keylen,
              const uint8_t *max_key, uint32_t max_keylen,
              art_callback_fn callback, void *ctx);

/**
 * 获取节点数量。
 */
uint32_t art_size(const art_tree_t *tree);

#ifdef __cplusplus
}
#endif

#endif /* ART_INDEX_H */