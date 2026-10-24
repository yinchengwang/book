/*
 * skip_list_private.h
 *
 * Skip List 内部结构。
 * Skip List 是一种概率平衡的有序链表，通过随机层级实现平衡。
 */

#ifndef SKIP_LIST_PRIVATE_H
#define SKIP_LIST_PRIVATE_H

#include <db/index/skip_list/skip_list.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SKIP_LIST_MAX_LEVEL 16
#define SKIP_LIST_DEFAULT_LEVEL 1
#define SKIP_LIST_P 0.5  /* 晋升概率 */

/*
 * Skip List 节点。
 */
typedef struct skip_list_node {
    void *key;
    uint32_t keylen;
    void *value;
    uint32_t valuelen;

    uint32_t level;              /* 当前节点层级（1 ~ MAX_LEVEL） */
    struct skip_list_node **forward;  /* 每层的前向指针数组 */
} skip_list_node_t;

struct skip_list {
    skip_list_compare_fn compare;    /* 比较函数 */
    void *compare_ctx;               /* 比较上下文 */

    uint32_t level;                  /* 当前最大层级 */
    uint32_t size;                   /* 节点数量 */
    skip_list_node_t *header;        /* 头节点（虚拟节点） */
};

/* 辅助函数 */
int _skip_list_compare(const skip_list_t *list,
                       const void *lhs, uint32_t lhs_len,
                       const void *rhs, uint32_t rhs_len);

/* 创建新节点。 */
skip_list_node_t *_skip_list_node_create(const skip_list_t *list,
                                         const void *key, uint32_t keylen,
                                         const void *value, uint32_t valuelen,
                                         uint32_t level);

/* 释放节点。 */
void _skip_list_node_drop(skip_list_node_t *node);

/* 随机生成层级。 */
uint32_t _skip_list_random_level(void);

/* 查找 key，返回指向目标节点的指针数组（update[i] = 第 i 层最后一个小于 key 的节点）。 */
skip_list_node_t **_skip_list_find(const skip_list_t *list,
                                   const void *key, uint32_t keylen);

#endif /* SKIP_LIST_PRIVATE_H */
