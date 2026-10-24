/*
 * radix_tree.h
 *
 * Public API for a Radix Tree (Compressed Trie) index.
 *
 * Radix Tree 是一种压缩前缀树，用于高效存储和查找字符串键：
 * - 相同前缀的边被合并，减少空间
 * - 支持精确查找、前缀匹配、最长前缀匹配
 *
 * 应用场景：
 * - IP 路由表
 * - Redis SDS 内部使用
 * - 字符串键的前缀索引
 */

#ifndef RADIX_TREE_INDEX_H
#define RADIX_TREE_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Radix Tree opaque 类型。 */
typedef struct radix_tree radix_tree_t;

/* 回调函数类型。返回 0 继续遍历，非 0 停止。 */
typedef int (*radix_tree_callback_fn)(const char *key, uint32_t keylen,
                                       const void *value, uint32_t valuelen,
                                       void *ctx);

/**
 * 创建 Radix Tree。
 */
radix_tree_t *radix_tree_create(void);

/**
 * 销毁 Radix Tree。
 */
void radix_tree_drop(radix_tree_t *tree);

/**
 * 插入 key-value。
 *
 * @return 0 成功，-1 失败
 */
int radix_tree_insert(radix_tree_t *tree,
                      const char *key, uint32_t keylen,
                      const void *value, uint32_t valuelen);

/**
 * 删除 key。
 *
 * @return 0 成功删除，-1 未找到
 */
int radix_tree_delete(radix_tree_t *tree,
                      const char *key, uint32_t keylen);

/**
 * 精确查找 key。
 *
 * @return 0 找到，-1 未找到
 */
int radix_tree_search(const radix_tree_t *tree,
                      const char *key, uint32_t keylen,
                      const void **value_out, uint32_t *valuelen_out);

/**
 * 前缀匹配查找：返回所有以 prefix 开头的 key。
 *
 * @return 匹配的 key 数量
 */
int radix_tree_prefix(radix_tree_t *tree,
                      const char *prefix, uint32_t prefixlen,
                      radix_tree_callback_fn callback,
                      void *ctx);

/**
 * 最长前缀匹配：返回 tree 中与 key 匹配的最长前缀。
 *
 * @return 匹配的前缀长度，0 表示没有匹配
 */
uint32_t radix_tree_longest_prefix(const radix_tree_t *tree,
                                    const char *key, uint32_t keylen,
                                    char *matched_key, uint32_t *matched_len);

/**
 * 获取 key 数量。
 */
uint32_t radix_tree_size(const radix_tree_t *tree);

#ifdef __cplusplus
}
#endif

#endif /* RADIX_TREE_INDEX_H */