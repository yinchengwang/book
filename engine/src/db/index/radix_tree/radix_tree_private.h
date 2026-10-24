/*
 * radix_tree_private.h
 *
 * Radix Tree（压缩前缀树）内部结构。
 * 相同前缀的边被压缩合并，节省空间。
 */

#ifndef RADIX_TREE_PRIVATE_H
#define RADIX_TREE_PRIVATE_H

#include <db/index/radix_tree/radix_tree.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Radix Tree 节点。
 * 每个节点存储一个压缩的前缀。
 */
typedef struct radix_node {
    char *prefix;            /* 压缩的前缀字符串 */
    uint32_t prefix_len;     /* 前缀长度 */

    bool is_end;             /* 是否是某个 key 的结尾 */
    void *value;             /* 当 is_end=true 时存储 value */
    uint32_t valuelen;       /* value 长度 */

    struct radix_node **children;  /* 子节点数组（最多 256 个字符） */
    uint8_t child_count;     /* 子节点数量 */
    uint8_t first_child;     /* 第一个子节点的字符值（用于优化） */
} radix_node_t;

struct radix_tree {
    uint32_t size;           /* key 数量 */
    radix_node_t *root;      /* 根节点（虚拟） */
};

/* 辅助函数 */
void _radix_node_drop(radix_node_t *node);
radix_node_t *_radix_node_create(const char *prefix, uint32_t prefix_len);

/* 查找 key。返回最后匹配的节点和剩余未匹配的字符数。 */
radix_node_t *_radix_tree_find_node(radix_tree_t *tree,
                                     const char *key, uint32_t keylen,
                                     uint32_t *matched_len);

/* 插入时的分裂操作。 */
int _radix_tree_split_node(radix_node_t *node, uint32_t split_pos,
                           radix_node_t **new_child_out);

/* 查找指定字符的子节点。 */
radix_node_t *_radix_find_child(radix_node_t *node, char c);

/* 设置子节点。 */
void _radix_set_child(radix_node_t *node, char c, radix_node_t *child);

#endif /* RADIX_TREE_PRIVATE_H */