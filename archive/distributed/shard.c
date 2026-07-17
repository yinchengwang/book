/**
 * @file shard.c
 * @brief 分片与路由核心实现
 */

#include "db/core/shard.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Hash 函数实现
 * ======================================================================== */

/* MurmurHash3 优化实现 */
static uint64_t mmhash3_64(const void *key, size_t len, uint64_t seed) {
    const uint8_t *data = (const uint8_t *)key;
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    size_t nblocks = len / 8;
    for (size_t i = 0; i < nblocks; i++) {
        uint64_t k;
        memcpy(&k, data + i * 8, sizeof(k));
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }

    const uint8_t *tail = data + nblocks * 8;
    size_t tail_len = len & 7;

    switch (tail_len) {
    case 7: h ^= (uint64_t)(tail[6]) << 48;
    case 6: h ^= (uint64_t)(tail[5]) << 40;
    case 5: h ^= (uint64_t)(tail[4]) << 32;
    case 4: h ^= (uint64_t)(tail[3]) << 24;
    case 3: h ^= (uint64_t)(tail[2]) << 16;
    case 2: h ^= (uint64_t)(tail[1]) << 8;
    case 1: h ^= (uint64_t)(tail[0]) << 0;
            h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

uint64_t murmurhash3(const void *key, size_t len, uint32_t seed) {
    return mmhash3_64(key, len, seed);
}

uint32_t shard_default_hash(const char *key) {
    return (uint32_t)mmhash3_64(key, strlen(key), 0x1234abcd);
}

uint32_t shard_calc_hash(const void *key, size_t len, uint32_t num_shards) {
    uint64_t h = mmhash3_64(key, len, 0x1234abcd);
    return (uint32_t)(h % num_shards);
}

/* ========================================================================
 * 分片拓扑管理
 * ======================================================================== */

ShardTopology *shard_topology_create(void) {
    ShardTopology *topo = (ShardTopology *)calloc(1, sizeof(ShardTopology));
    if (topo) {
        topo->capacity = 16;
        topo->nodes = (ShardNode *)calloc(topo->capacity, sizeof(ShardNode));
        topo->num_shards = 0;
        topo->num_nodes = 0;
        topo->vnode_capacity = 256;
        topo->vnodes = (VirtualNode *)calloc(topo->vnode_capacity, sizeof(VirtualNode));
        topo->num_vnodes = 0;
        topo->version = 0;
    }
    return topo;
}

void shard_topology_destroy(ShardTopology *topo) {
    if (!topo) return;
    free(topo->nodes);
    free(topo->shards);
    free(topo->vnodes);
    free(topo);
}

static int shard_topology_grow_nodes(ShardTopology *topo) {
    if (topo->num_nodes >= topo->capacity) {
        topo->capacity *= 2;
        topo->nodes = (ShardNode *)realloc(topo->nodes, topo->capacity * sizeof(ShardNode));
        if (!topo->nodes) return -1;
    }
    return 0;
}

int shard_topology_add_node(ShardTopology *topo, const ShardNode *node) {
    if (!topo || !node) return -1;

    /* 检查是否已存在 */
    for (size_t i = 0; i < topo->num_nodes; i++) {
        if (strcmp(topo->nodes[i].node_id, node->node_id) == 0) {
            return -1;  /* 已存在 */
        }
    }

    if (shard_topology_grow_nodes(topo) != 0) return -1;
    topo->nodes[topo->num_nodes++] = *node;
    topo->version++;

    return 0;
}

int shard_topology_remove_node(ShardTopology *topo, const char *node_id) {
    if (!topo || !node_id) return -1;

    for (size_t i = 0; i < topo->num_nodes; i++) {
        if (strcmp(topo->nodes[i].node_id, node_id) == 0) {
            /* 移动后面的节点 */
            for (size_t j = i; j < topo->num_nodes - 1; j++) {
                topo->nodes[j] = topo->nodes[j + 1];
            }
            topo->num_nodes--;
            topo->version++;
            return 0;
        }
    }
    return -1;
}

int shard_topology_add_shard(ShardTopology *topo, const ShardInfo *shard) {
    if (!topo || !shard) return -1;

    /* 扩展分片数组 */
    if (!topo->shards) {
        topo->shards = (ShardInfo *)calloc(16, sizeof(ShardInfo));
    }
    topo->shards[topo->num_shards++] = *shard;
    topo->version++;

    return 0;
}

int shard_topology_remove_shard(ShardTopology *topo, uint32_t shard_id) {
    if (!topo) return -1;

    for (size_t i = 0; i < topo->num_shards; i++) {
        if (topo->shards[i].shard_id == shard_id) {
            for (size_t j = i; j < topo->num_shards - 1; j++) {
                topo->shards[j] = topo->shards[j + 1];
            }
            topo->num_shards--;
            topo->version++;
            return 0;
        }
    }
    return -1;
}

const ShardNode *shard_topology_get_node(const ShardTopology *topo, const char *node_id) {
    if (!topo || !node_id) return NULL;

    for (size_t i = 0; i < topo->num_nodes; i++) {
        if (strcmp(topo->nodes[i].node_id, node_id) == 0) {
            return &topo->nodes[i];
        }
    }
    return NULL;
}

const ShardInfo *shard_topology_get_shard(const ShardTopology *topo, uint32_t shard_id) {
    if (!topo) return NULL;

    for (size_t i = 0; i < topo->num_shards; i++) {
        if (topo->shards[i].shard_id == shard_id) {
            return &topo->shards[i];
        }
    }
    return NULL;
}

const ShardNode **shard_topology_get_active_nodes(const ShardTopology *topo, size_t *out_count) {
    if (!topo || !out_count) return NULL;

    size_t count = 0;
    static const ShardNode *results[256];

    for (size_t i = 0; i < topo->num_nodes; i++) {
        if (topo->nodes[i].is_healthy) {
            results[count++] = &topo->nodes[i];
        }
    }

    *out_count = count;
    return results;
}

/* ========================================================================
 * 路由计算器
 * ======================================================================== */

ShardRouter *shard_router_create(ShardTopology *topo, ShardStrategy strategy, uint32_t num_shards) {
    if (!topo) return NULL;

    ShardRouter *router = (ShardRouter *)calloc(1, sizeof(ShardRouter));
    if (router) {
        router->topo = topo;
        router->default_strategy = strategy;
        router->num_shards = num_shards > 0 ? num_shards : 1;
        router->mur_hash_seed = 0x1234abcd;
    }
    return router;
}

void shard_router_destroy(ShardRouter *router) {
    free(router);
}

void shard_router_set_strategy(ShardRouter *router, ShardStrategy strategy) {
    if (router) router->default_strategy = strategy;
}

/* 路由结果缓存 */
static ShardRouting g_routing_result;

ShardRouting *shard_route(ShardRouter *router, const void *key, size_t key_len) {
    if (!router || !key) return NULL;

    memset(&g_routing_result, 0, sizeof(g_routing_result));

    switch (router->default_strategy) {
    case SHARD_HASH:
    case SHARD_MOD:
        return shard_route_hash(router, key, key_len);
    case SHARD_RANGE:
        return shard_route_range_scan(router, key, key_len, key, key_len);
    default:
        return shard_route_hash(router, key, key_len);
    }
}

/* ========================================================================
 * Hash 分片路由
 * ======================================================================== */

ShardRouting *shard_route_hash(ShardRouter *router, const void *key, size_t key_len) {
    if (!router || !key) return NULL;

    uint32_t shard_id = (uint32_t)(murmurhash3(key, key_len, router->mur_hash_seed) % router->num_shards);

    g_routing_result.shard_id = shard_id;
    g_routing_result.is_local = true;

    /* 查找分片对应节点 */
    const ShardInfo *shard = shard_topology_get_shard(router->topo, shard_id);
    if (shard) {
        snprintf(g_routing_result.node_addr, sizeof(g_routing_result.node_addr), "%s", shard->primary_node);
    }

    return &g_routing_result;
}

/* ========================================================================
 * 一致性 Hash
 * ======================================================================== */

int shard_add_virtual_node(ShardRouter *router, uint32_t shard_id, size_t num_vnodes) {
    if (!router || !router->topo) return -1;

    ShardTopology *topo = router->topo;

    for (size_t i = 0; i < num_vnodes; i++) {
        if (topo->num_vnodes >= topo->vnode_capacity) {
            topo->vnode_capacity *= 2;
            topo->vnodes = (VirtualNode *)realloc(topo->vnodes, topo->vnode_capacity * sizeof(VirtualNode));
        }

        VirtualNode *vn = &topo->vnodes[topo->num_vnodes];
        vn->vnode_id = (uint32_t)topo->num_vnodes;
        vn->shard_id = shard_id;

        /* 计算虚拟节点 Hash */
        char vnode_key[128];
        snprintf(vnode_key, sizeof(vnode_key), "VN-%u-%zu", shard_id, i);
        vn->hash = murmurhash3(vnode_key, strlen(vnode_key), router->mur_hash_seed);

        topo->num_vnodes++;
    }

    return 0;
}

ShardRouting *shard_route_consistent_hash(ShardRouter *router, const void *key, size_t key_len) {
    if (!router || !key || !router->topo) return NULL;

    uint64_t key_hash = murmurhash3(key, key_len, router->mur_hash_seed);

    /* 找到第一个 Hash 值大于 key_hash 的虚拟节点 */
    uint64_t min_diff = UINT64_MAX;
    uint32_t target_shard = 0;

    for (size_t i = 0; i < router->topo->num_vnodes; i++) {
        VirtualNode *vn = &router->topo->vnodes[i];
        uint64_t diff = (vn->hash > key_hash) ? (vn->hash - key_hash) : (UINT64_MAX - key_hash + vn->hash);
        if (diff < min_diff) {
            min_diff = diff;
            target_shard = vn->shard_id;
        }
    }

    memset(&g_routing_result, 0, sizeof(g_routing_result));
    g_routing_result.shard_id = target_shard;

    return &g_routing_result;
}

/* ========================================================================
 * Range 分片
 * ======================================================================== */

int shard_create_range_shard(ShardTopology *topo,
                              uint32_t shard_id,
                              const char *range_key,
                              int64_t start, int64_t end) {
    if (!topo) return -1;

    ShardInfo shard;
    memset(&shard, 0, sizeof(shard));
    shard.shard_id = shard_id;
    shard.strategy = SHARD_RANGE;
    shard.state = SHARD_STATE_ACTIVE;
    shard.range_start = start;
    shard.range_end = end;
    if (range_key) {
        snprintf(shard.range_key, sizeof(shard.range_key), "%s", range_key);
    }

    return shard_topology_add_shard(topo, &shard);
}

ShardRouting *shard_route_range_scan(ShardRouter *router,
                                      const void *start_key, size_t start_len,
                                      const void *end_key, size_t end_len) {
    (void)end_key; (void)end_len;

    if (!router || !start_key || !router->topo) return NULL;

    /* 简化实现：使用 start_key 的 Hash 作为分片 ID */
    uint32_t shard_id = (uint32_t)(murmurhash3(start_key, start_len, router->mur_hash_seed) % router->num_shards);

    memset(&g_routing_result, 0, sizeof(g_routing_result));
    g_routing_result.shard_id = shard_id;

    /* 返回第一个匹配的分片 */
    for (size_t i = 0; i < router->topo->num_shards; i++) {
        const ShardInfo *shard = &router->topo->shards[i];
        if (shard->strategy == SHARD_RANGE) {
            /* 这里应该有范围比较逻辑，简化处理 */
            g_routing_result.shard_id = shard->shard_id;
            break;
        }
    }

    return &g_routing_result;
}

ShardRouting **shard_route_range(ShardRouter *router,
                                  const void *start_key, size_t start_len,
                                  const void *end_key, size_t end_len,
                                  size_t *out_count) {
    if (!router || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    /* 简化：返回所有分片 */
    static ShardRouting results[256];
    size_t count = 0;

    for (size_t i = 0; i < router->topo->num_shards && count < 256; i++) {
        results[count].shard_id = router->topo->shards[i].shard_id;
        results[count].is_local = true;
        count++;
    }

    *out_count = count;
    return results;
}

const char **shard_get_replicas(ShardRouter *router, uint32_t shard_id, size_t *out_count) {
    if (!router || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    static const char *results[16];
    size_t count = 0;

    const ShardInfo *shard = shard_topology_get_shard(router->topo, shard_id);
    if (shard && shard->replica_nodes) {
        /* 解析副本节点列表 */
        results[count++] = shard->primary_node;
    }

    *out_count = count;
    return results;
}

/* ========================================================================
 * 分片迁移
 * ======================================================================== */

MigrationTask *migration_task_create(uint32_t shard_id,
                                      const char *src_node,
                                      const char *dst_node) {
    MigrationTask *task = (MigrationTask *)calloc(1, sizeof(MigrationTask));
    if (task) {
        task->task_id = shard_id;  /* 简化：用 shard_id 作为 task_id */
        task->shard_id = shard_id;
        if (src_node) snprintf(task->src_node, sizeof(task->src_node), "%s", src_node);
        if (dst_node) snprintf(task->dst_node, sizeof(task->dst_node), "%s", dst_node);
        task->state = MIGRATION_PENDING;
        task->progress = 0.0;
        task->start_time = 0;
    }
    return task;
}

void migration_task_update(MigrationTask *task, double progress) {
    if (!task) return;
    task->progress = progress;
    if (progress > 0 && progress < 1.0) {
        task->state = MIGRATION_IN_PROGRESS;
    }
}

MigrationState migration_task_get_state(const MigrationTask *task) {
    return task ? task->state : MIGRATION_FAILED;
}

void migration_task_complete(MigrationTask *task) {
    if (!task) return;
    task->state = MIGRATION_COMPLETE;
    task->progress = 1.0;
}

void migration_task_destroy(MigrationTask *task) {
    free(task);
}

/* ========================================================================
 * 动态扩缩容
 * ======================================================================== */

int shard_split(ShardRouter *router, uint32_t shard_id, const void *split_key) {
    if (!router) return -1;

    /* 创建新分片 */
    ShardInfo new_shard;
    memset(&new_shard, 0, sizeof(new_shard));

    new_shard.shard_id = router->num_shards++;
    new_shard.strategy = router->default_strategy;
    new_shard.state = SHARD_STATE_ACTIVE;

    shard_topology_add_shard(router->topo, &new_shard);

    return 0;
}

int shard_merge(ShardRouter *router, uint32_t shard_id1, uint32_t shard_id2) {
    (void)router; (void)shard_id1; (void)shard_id2;
    /* 简化实现 */
    return 0;
}

int shard_rebalance(ShardRouter *router, ReshardPolicy policy) {
    (void)router; (void)policy;
    /* 简化实现 */
    return 0;
}

int shard_add_node(ShardRouter *router, const ShardNode *node) {
    if (!router) return -1;
    return shard_topology_add_node(router->topo, node);
}

int shard_remove_node(ShardRouter *router, const char *node_id) {
    if (!router) return -1;
    return shard_topology_remove_node(router->topo, node_id);
}

MigrationTask **shard_get_migration_tasks(ShardRouter *router, size_t *out_count) {
    (void)router;
    if (out_count) *out_count = 0;
    return NULL;  /* 简化：暂无迁移任务 */
}

bool shard_needs_reshard(const ShardRouter *router,
                         double load_threshold,
                         size_t size_threshold) {
    (void)router; (void)load_threshold; (void)size_threshold;
    /* 简化：始终返回 false */
    return false;
}

/* ========================================================================
 * 拓扑同步
 * ======================================================================== */

void *shard_topology_serialize(const ShardTopology *topo, size_t *out_size) {
    if (!topo || !out_size) return NULL;

    *out_size = sizeof(ShardTopology);
    void *data = malloc(*out_size);
    if (data) {
        memcpy(data, topo, sizeof(ShardTopology));
    }
    return data;
}

ShardTopology *shard_topology_deserialize(const void *data, size_t size) {
    if (!data || size < sizeof(ShardTopology)) return NULL;

    ShardTopology *topo = (ShardTopology *)malloc(sizeof(ShardTopology));
    if (topo) {
        memcpy(topo, data, sizeof(ShardTopology));
    }
    return topo;
}

bool shard_topology_is_stale(const ShardTopology *local, const ShardTopology *remote) {
    if (!local || !remote) return true;
    return local->version < remote->version;
}

int shard_topology_gen_diff(const ShardTopology *old_topo,
                            const ShardTopology *new_topo,
                            void **out_diff, size_t *out_size) {
    (void)old_topo; (void)new_topo;
    if (out_diff) *out_diff = NULL;
    if (out_size) *out_size = 0;
    return 0;
}
