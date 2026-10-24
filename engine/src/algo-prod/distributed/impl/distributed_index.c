/*
 * distributed_index.c —— 一致性哈希（Consistent Hashing）完整实现
 *
 * ============================================================
 * 概述
 * ============================================================
 *
 * 一致性哈希是一种分布式系统中的数据分布算法，核心特点：
 * - 节点增减时只影响局部数据
 * - 使用虚拟节点（vnode）均衡负载
 * - 顺时针查找节点归属
 *
 * ============================================================
 * 数据结构
 * ============================================================
 *
 * 使用排序数组存储虚拟节点：
 *   vnodes[] — 按 hash 值排序的虚拟节点数组
 *
 * 查找算法：
 *   1. 计算 key_hash = hash(key)
 *   2. 二分查找 lower_bound(key_hash)
 *   3. 如果找到，返回该 vnode 的物理节点
 *   4. 如果没找到（超出范围），返回第一个 vnode（绕回）
 */

#include <ds/distributed_index.h>

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * 内部数据结构
 * ----------------------------------------------------------------------- */

struct consistent_hash {
    ch_vnode_t *vnodes;       /* 虚拟节点数组（按 hash 排序）*/
    size_t       vnode_count;  /* 虚拟节点总数 */
    size_t       node_count;    /* 物理节点数 */
    size_t       capacity;     /* vnodes 数组容量 */
    char       **node_ids;     /* 物理节点 ID 列表 */
};

/* -----------------------------------------------------------------------
 * 哈希函数
 * ----------------------------------------------------------------------- */

/*
 * fnv_1a_hash — FNV-1a 哈希函数
 *
 * 返回 32 位哈希值。
 */
static uint32_t fnv_1a_hash(const void *key, size_t keylen)
{
    const uint8_t *data = (const uint8_t *)key;
    uint32_t       hash = 2166136261u;
    size_t         i;

    for (i = 0; i < keylen; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

/*
 * vnode_hash — 计算虚拟节点的哈希值
 */
static uint32_t vnode_hash(const char *node_id, size_t vnode_idx)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s#%zu", node_id, vnode_idx);
    return fnv_1a_hash(buffer, strlen(buffer));
}

/* -----------------------------------------------------------------------
 * 比较函数
 * ----------------------------------------------------------------------- */

static int vnode_compare(const void *a, const void *b)
{
    uint32_t ha = ((const ch_vnode_t *)a)->hash;
    uint32_t hb = ((const ch_vnode_t *)b)->hash;
    if (ha < hb) return -1;
    if (ha > hb) return 1;
    return 0;
}

/* -----------------------------------------------------------------------
 * 内部辅助函数
 * ----------------------------------------------------------------------- */

/*
 * find_node_index — 查找物理节点在 node_ids 数组中的索引
 *
 * 返回: 找到返回索引，找不到返回 -1
 */
static int find_node_index(const consistent_hash_t *ring, const char *node_id)
{
    for (size_t i = 0; i < ring->node_count; i++) {
        if (strcmp(ring->node_ids[i], node_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/*
 * resize_vnodes — 扩展 vnodes 数组
 *
 * 返回: 成功返回 0，失败返回 -1
 */
static int resize_vnodes(consistent_hash_t *ring, size_t new_capacity)
{
    ch_vnode_t *new_vnodes = (ch_vnode_t *)realloc(
        ring->vnodes, new_capacity * sizeof(ch_vnode_t));
    if (!new_vnodes) {
        return -1;
    }
    ring->vnodes = new_vnodes;
    ring->capacity = new_capacity;
    return 0;
}

/* -----------------------------------------------------------------------
 * 公开 API 实现
 * ----------------------------------------------------------------------- */

consistent_hash_t *ch_create(void)
{
    consistent_hash_t *ring = (consistent_hash_t *)calloc(
        1, sizeof(consistent_hash_t));
    if (!ring) {
        return NULL;
    }

    ring->vnodes = (ch_vnode_t *)calloc(16, sizeof(ch_vnode_t));
    if (!ring->vnodes) {
        free(ring);
        return NULL;
    }

    ring->node_ids = (char **)calloc(16, sizeof(char *));
    if (!ring->node_ids) {
        free(ring->vnodes);
        free(ring);
        return NULL;
    }

    ring->vnode_count = 0;
    ring->node_count = 0;
    ring->capacity = 16;

    return ring;
}

void ch_destroy(consistent_hash_t *ring)
{
    if (!ring) return;

    /* 释放所有物理节点 ID */
    for (size_t i = 0; i < ring->node_count; i++) {
        free(ring->node_ids[i]);
    }
    free(ring->node_ids);
    free(ring->vnodes);
    free(ring);
}

int ch_add_node(consistent_hash_t *ring, const char *node_id, size_t vnode_count)
{
    size_t i;
    size_t count;

    if (!ring || !node_id) {
        return -1;
    }

    /* 检查节点是否已存在 */
    if (find_node_index(ring, node_id) >= 0) {
        return 1;
    }

    /* 确定虚拟节点数量 */
    count = (vnode_count > 0) ? vnode_count : CH_DEFAULT_VNODE_COUNT;

    /* 检查容量是否足够 */
    size_t needed = ring->vnode_count + count;
    if (needed > ring->capacity) {
        size_t new_capacity = ring->capacity;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }
        if (resize_vnodes(ring, new_capacity) != 0) {
            return -1;
        }
    }

    /* 添加物理节点 ID */
    ring->node_ids[ring->node_count] = (char *)malloc(strlen(node_id) + 1);
    if (!ring->node_ids[ring->node_count]) {
        return -1;
    }
    strcpy(ring->node_ids[ring->node_count], node_id);
    ring->node_count++;

    /* 添加虚拟节点 */
    size_t start_idx = ring->vnode_count;
    for (i = 0; i < count; i++) {
        ring->vnodes[start_idx + i].hash = vnode_hash(node_id, i);
        ring->vnodes[start_idx + i].node_id = ring->node_ids[ring->node_count - 1];
        ring->vnodes[start_idx + i].vnode_idx = i;
    }
    ring->vnode_count += count;

    /* 按 hash 排序 */
    qsort(ring->vnodes, ring->vnode_count, sizeof(ch_vnode_t), vnode_compare);

    return 0;
}

int ch_remove_node(consistent_hash_t *ring, const char *node_id)
{
    size_t i;
    size_t new_vnode_count = 0;

    if (!ring || !node_id) {
        return -1;
    }

    /* 检查节点是否存在 */
    int node_idx = find_node_index(ring, node_id);
    if (node_idx < 0) {
        return 1;
    }

    /* 统计要移除的虚拟节点数量 */
    size_t remove_count = 0;
    for (i = 0; i < ring->vnode_count; i++) {
        if (strcmp(ring->vnodes[i].node_id, node_id) == 0) {
            remove_count++;
        }
    }

    if (remove_count == 0) {
        return 1;
    }

    /* 创建新的虚拟节点数组（移除指定节点的 vnode） */
    ch_vnode_t *new_vnodes = (ch_vnode_t *)malloc(
        ring->vnode_count * sizeof(ch_vnode_t));
    if (!new_vnodes) {
        return -1;
    }

    for (i = 0; i < ring->vnode_count; i++) {
        if (strcmp(ring->vnodes[i].node_id, node_id) != 0) {
            new_vnodes[new_vnode_count++] = ring->vnodes[i];
        }
    }

    free(ring->vnodes);
    ring->vnodes = new_vnodes;
    ring->vnode_count = new_vnode_count;

    /* 释放物理节点 ID */
    free(ring->node_ids[node_idx]);

    /* 从节点列表中移除 */
    for (i = (size_t)node_idx; i < ring->node_count - 1; i++) {
        ring->node_ids[i] = ring->node_ids[i + 1];
    }
    ring->node_count--;

    /* 更新 vnodes 中指向 node_ids 的指针 */
    for (i = 0; i < ring->vnode_count; i++) {
        for (size_t j = 0; j < ring->node_count; j++) {
            if (strcmp(ring->vnodes[i].node_id, ring->node_ids[j]) == 0) {
                ring->vnodes[i].node_id = ring->node_ids[j];
                break;
            }
        }
    }

    return 0;
}

bool ch_has_node(const consistent_hash_t *ring, const char *node_id)
{
    if (!ring || !node_id) {
        return false;
    }
    return find_node_index(ring, node_id) >= 0;
}

const char *ch_find_node(const consistent_hash_t *ring,
                         const void *key, size_t keylen)
{
    uint32_t key_hash;
    int lo, hi, mid;
    int found = -1;

    if (!ring || !key || keylen == 0 || ring->vnode_count == 0) {
        return NULL;
    }

    key_hash = fnv_1a_hash(key, keylen);

    /* 二分查找 lower_bound(key_hash) */
    lo = 0;
    hi = (int)ring->vnode_count - 1;

    while (lo <= hi) {
        mid = (lo + hi) / 2;
        if (ring->vnodes[mid].hash >= key_hash) {
            found = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    /* 如果没找到，返回第一个 vnode（绕回） */
    if (found < 0) {
        found = 0;
    }

    return ring->vnodes[found].node_id;
}

size_t ch_get_node_count(const consistent_hash_t *ring)
{
    return ring ? ring->node_count : 0;
}

size_t ch_get_vnode_count(const consistent_hash_t *ring)
{
    return ring ? ring->vnode_count : 0;
}

bool ch_is_empty(const consistent_hash_t *ring)
{
    return ring ? (ring->node_count == 0) : true;
}

/* -----------------------------------------------------------------------
 * 演示函数
 * ----------------------------------------------------------------------- */

void ds_distributed_index_demo(void)
{
    printf("========== 分布式索引（一致性哈希）演示 ==========\n");

    consistent_hash_t *ring = ch_create();

    /* 添加 3 个节点 */
    ch_add_node(ring, "192.168.1.1", 0);
    ch_add_node(ring, "192.168.1.2", 0);
    ch_add_node(ring, "192.168.1.3", 0);

    printf("\n【3 个节点，各 %d 个虚拟节点】\n", CH_DEFAULT_VNODE_COUNT);
    printf("  总虚拟节点数: %zu\n", ch_get_vnode_count(ring));

    /* 测试数据分布 */
    const char *test_keys[] = {"user_123", "order_456", "product_789",
                                "session_abc", "cache_def"};

    printf("\n  数据分配测试：\n");
    for (size_t i = 0; i < 5; i++) {
        const char *node = ch_find_node(ring, test_keys[i], strlen(test_keys[i]));
        printf("    key=\"%s\" → %s\n", test_keys[i], node ? node : "(null)");
    }

    printf("\n【一致性哈希的优势】\n");
    printf("  - 新增节点：只影响环上新节点周围的约 1/(N+1) 的数据\n");
    printf("  - 移除节点：只需将原节点数据迁移到顺时针下一个节点\n");
    printf("  - 对比取模哈希：增减节点后几乎 100%% 的数据都需要重分配\n");

    printf("\n【其他分布式索引策略】\n");
    printf("  - 倒排索引分片（Elasticsearch）：按文档 ID 或 term 分片\n");
    printf("  - 向量索引（Faiss/Milvus）：IVF 聚类中心分布到不同分片\n");
    printf("  - 范围分区：MySQL 按日期范围分区\n");
    printf("  - 复制（Replication）：每个分片多个副本，提高可用性\n");

    ch_destroy(ring);
    printf("\n========== 分布式索引演示结束 ==========\n");
}