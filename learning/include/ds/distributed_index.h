/*
 * distributed_index.h —— 一致性哈希（Consistent Hashing）
 *
 * ============================================================
 * 概述
 * ============================================================
 * Consistent Hash 是一种分布式系统中的数据分布算法，用于在节点
 * 增减时最小化数据迁移。它将节点和数据映射到同一个哈希环上，
 * 数据被分配到哈希环上顺时针遇到的第一个节点。
 *
 * 一致性哈希是 Redis Cluster、Cassandra、DynamoDB 等分布式
 * 系统的核心组件。
 *
 * ============================================================
 * 核心概念
 * ============================================================
 *
 * 哈希环：
 *   - 环的大小为 2^32（0 到 0xFFFFFFFF）
 *   - 每个虚拟节点（vnode）对应环上的一个哈希值
 *   - 数据 key 也在环上有一个位置
 *
 * 虚拟节点（vnode）：
 *   - 每个物理节点对应多个虚拟节点
 *   - 虚拟节点数量越多，负载越均衡
 *   - 默认每个物理节点 150 个 vnode
 *
 * 数据分配：
 *   - 计算 key 的哈希值
 *   - 在环上顺时针找到第一个 vnode
 *   - 该 vnode 所属的物理节点即为数据归属
 *
 * ============================================================
 * 使用示例
 * ============================================================
 *
 *   consistent_hash_t* ring = ch_create();
 *
 *   // 添加节点（默认 150 个虚拟节点）
 *   ch_add_node(ring, "192.168.1.1", 0);
 *   ch_add_node(ring, "192.168.1.2", 0);
 *   ch_add_node(ring, "192.168.1.3", 0);
 *
 *   // 查找 key 归属的节点
 *   const char* node = ch_find_node(ring, "user_123", 8);
 *
 *   // 添加新节点（只有约 1/4 的数据需要迁移）
 *   ch_add_node(ring, "192.168.1.4", 0);
 *
 *   ch_destroy(ring);
 */

#ifndef DS_DISTRIBUTED_INDEX_H
#define DS_DISTRIBUTED_INDEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * 常量定义
 * ----------------------------------------------------------------------- */

#define CH_DEFAULT_VNODE_COUNT 150  /* 每个物理节点的默认虚拟节点数 */
#define CH_RING_SIZE          0x100000000ULL  /* 2^32 */

/* -----------------------------------------------------------------------
 * 类型定义
 * ----------------------------------------------------------------------- */

/*
 * ch_vnode_t — 虚拟节点结构
 */
typedef struct {
    uint32_t hash;      /* 虚拟节点在环上的位置 */
    char    *node_id;   /* 所属物理节点 ID */
    size_t   vnode_idx; /* 虚拟节点序号 */
} ch_vnode_t;

/*
 * consistent_hash_t — 一致性哈希环
 */
typedef struct consistent_hash consistent_hash_t;

/* -----------------------------------------------------------------------
 * 生命周期函数
 * ----------------------------------------------------------------------- */

/*
 * ch_create — 创建新的哈希环
 *
 * 返回: 成功返回哈希环指针，失败返回 NULL
 */
consistent_hash_t *ch_create(void);

/*
 * ch_destroy — 释放哈希环内存
 *
 * 参数:
 *   ring - 要释放的哈希环
 */
void ch_destroy(consistent_hash_t *ring);

/* -----------------------------------------------------------------------
 * 节点管理函数
 * ----------------------------------------------------------------------- */

/*
 * ch_add_node — 添加物理节点到哈希环
 *
 * 参数:
 *   ring        - 哈希环
 *   node_id     - 物理节点 ID（字符串）
 *   vnode_count - 虚拟节点数量，0 表示使用默认值
 *
 * 返回:
 *   成功返回 0，节点已存在返回 1，失败返回 -1
 */
int ch_add_node(consistent_hash_t *ring, const char *node_id, size_t vnode_count);

/*
 * ch_remove_node — 从哈希环移除物理节点
 *
 * 参数:
 *   ring    - 哈希环
 *   node_id - 要移除的物理节点 ID
 *
 * 返回:
 *   成功返回 0，节点不存在返回 1，失败返回 -1
 */
int ch_remove_node(consistent_hash_t *ring, const char *node_id);

/*
 * ch_has_node — 检查节点是否存在
 *
 * 返回: 存在返回 true，不存在返回 false
 */
bool ch_has_node(const consistent_hash_t *ring, const char *node_id);

/* -----------------------------------------------------------------------
 * 查询函数
 * ----------------------------------------------------------------------- */

/*
 * ch_find_node — 查找 key 所属的节点
 *
 * 顺时针查找：计算 key 的哈希值，在环上找到第一个 >= hash 的 vnode，
 * 如果没有则绕回到第一个 vnode。
 *
 * 参数:
 *   ring   - 哈希环
 *   key    - 要查询的键
 *   keylen - 键的字节长度
 *
 * 返回:
 *   成功返回节点 ID 字符串（不要 free），环为空返回 NULL
 */
const char *ch_find_node(const consistent_hash_t *ring, const void *key, size_t keylen);

/* -----------------------------------------------------------------------
 * 统计函数
 * ----------------------------------------------------------------------- */

/*
 * ch_get_node_count — 获取物理节点数量
 */
size_t ch_get_node_count(const consistent_hash_t *ring);

/*
 * ch_get_vnode_count — 获取虚拟节点数量
 */
size_t ch_get_vnode_count(const consistent_hash_t *ring);

/*
 * ch_is_empty — 检查哈希环是否为空
 */
bool ch_is_empty(const consistent_hash_t *ring);

/* -----------------------------------------------------------------------
 * 演示函数
 * ----------------------------------------------------------------------- */

/*
 * ds_distributed_index_demo — 演示一致性哈希的基本原理
 */
void ds_distributed_index_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_DISTRIBUTED_INDEX_H */