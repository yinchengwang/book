/**
 * @file sharding.c
 * @brief 一致性哈希分片实现
 *
 * 基于一致性哈希环实现数据分片路由。
 */
#include "sdk/mmdb_sharding.h"
#include "sdk/impl/mmdb_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* 一致性哈希虚拟节点数量 */
#define VIRTUAL_NODES_PER_SHARD  150

/* 分片节点结构 */
typedef struct {
    uint32_t    id;
    char        addr[256];
    uint64_t    key_count;
    bool        alive;
} shard_node_t;

/* 哈希环节点 */
typedef struct {
    uint32_t    hash;           /* 哈希值 */
    uint32_t    shard_id;       /* 对应的分片 ID */
} hash_ring_node_t;

/* 分片句柄内部结构 */
struct mmdb_shard_s {
    mmdb_t*             db;             /* 数据库句柄 */
    shard_node_t*       shards;         /* 分片节点数组 */
    uint32_t            shard_count;    /* 分片数量 */
    hash_ring_node_t*   ring;           /* 哈希环 */
    uint32_t            ring_size;      /* 哈希环大小 */
};

/* 全局分片实例 */
static mmdb_shard_t* g_sharding = NULL;

/**
 * @brief 计算哈希值（FNV-1a）
 */
static uint32_t fnv1a_hash(const char* key) {
    uint32_t hash = 2166136261u;
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 16777619u;
    }
    return hash;
}

/* 哈希环比较函数（用于 qsort） */
static int cmp_hash_ring(const void* a, const void* b) {
    uint32_t ha = ((const hash_ring_node_t*)a)->hash;
    uint32_t hb = ((const hash_ring_node_t*)b)->hash;
    return (ha > hb) - (ha < hb);
}

/**
 * @brief 构建哈希环
 */
static int build_hash_ring(mmdb_shard_t* s) {
    if (!s || s->shard_count == 0) {
        return MMDB_ERR_INVALID;
    }

    /* 计算哈希环总大小 */
    uint32_t total = s->shard_count * VIRTUAL_NODES_PER_SHARD;
    s->ring = (hash_ring_node_t*)calloc(total, sizeof(hash_ring_node_t));
    if (!s->ring) {
        return MMDB_ERR_NOMEM;
    }
    s->ring_size = total;

    /* 填充哈希环 */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < s->shard_count; i++) {
        char buf[256];
        for (uint32_t v = 0; v < VIRTUAL_NODES_PER_SHARD; v++) {
            snprintf(buf, sizeof(buf), "%u:%u", s->shards[i].id, v);
            s->ring[idx].hash = fnv1a_hash(buf);
            s->ring[idx].shard_id = s->shards[i].id;
            idx++;
        }
    }

    /* 按哈希值排序（用于二分查找） */
    qsort(s->ring, total, sizeof(hash_ring_node_t), cmp_hash_ring);

    return MMDB_OK;
}

/**
 * @brief 解析 JSON 格式的分片列表（简化实现）
 */
static int parse_shards(const char* shards_json, shard_node_t** out_shards, uint32_t* out_count) {
    if (!shards_json || !out_shards || !out_count) {
        return MMDB_ERR_INVALID;
    }

    /* 计算分片数量 */
    uint32_t count = 0;
    const char* p = shards_json;
    while ((p = strstr(p, "\"id\"")) != NULL) {
        count++;
        p++;
    }

    if (count == 0) {
        return MMDB_ERR_INVALID;
    }

    shard_node_t* shards = (shard_node_t*)calloc(count, sizeof(shard_node_t));
    if (!shards) {
        return MMDB_ERR_NOMEM;
    }

    /* 解析每个分片 */
    p = shards_json;
    uint32_t idx = 0;
    while (idx < count && (p = strstr(p, "\"id\"")) != NULL) {
        p += 4;
        while (*p == ' ' || *p == ':') p++;
        shards[idx].id = (uint32_t)atoi(p);
        shards[idx].key_count = 0;
        shards[idx].alive = true;

        /* 提取 addr */
        const char* addr_start = strstr(p, "\"addr\"");
        if (addr_start) {
            addr_start += 6;
            while (*addr_start == ' ' || *addr_start == ':') addr_start++;
            if (*addr_start == '"') addr_start++;
            const char* addr_end = strchr(addr_start, '"');
            if (addr_end) {
                size_t len = (size_t)(addr_end - addr_start);
                if (len >= sizeof(shards[idx].addr)) {
                    len = sizeof(shards[idx].addr) - 1;
                }
                memcpy(shards[idx].addr, addr_start, len);
                shards[idx].addr[len] = '\0';
            }
        }

        idx++;
        p++;
    }

    *out_shards = shards;
    *out_count = count;
    return MMDB_OK;
}

/**
 * @brief 初始化分片集群
 */
int mmdb_sharding_init(mmdb_t* db, const char* shards) {
    if (!db || !shards) {
        return MMDB_ERR_INVALID;
    }

    if (g_sharding) {
        return MMDB_ERR_INTERNAL;
    }

    mmdb_shard_t* s = (mmdb_shard_t*)calloc(1, sizeof(mmdb_shard_t));
    if (!s) {
        return MMDB_ERR_NOMEM;
    }

    s->db = db;

    /* 解析分片列表 */
    int rc = parse_shards(shards, &s->shards, &s->shard_count);
    if (rc != MMDB_OK) {
        free(s);
        return rc;
    }

    /* 构建哈希环 */
    rc = build_hash_ring(s);
    if (rc != MMDB_OK) {
        free(s->shards);
        free(s);
        return rc;
    }

    g_sharding = s;
    return MMDB_OK;
}

/**
 * @brief 获取键所属的分片
 */
int mmdb_sharding_route(mmdb_t* db, const char* key, uint32_t* shard_id) {
    if (!db || !key || !shard_id) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding || g_sharding->ring_size == 0) {
        return MMDB_ERR_INTERNAL;
    }

    /* 计算键的哈希值 */
    uint32_t hash = fnv1a_hash(key);

    /* 二分查找第一个不小于键哈希值的环节点 */
    uint32_t left = 0;
    uint32_t right = g_sharding->ring_size;
    while (left < right) {
        uint32_t mid = left + (right - left) / 2;
        if (g_sharding->ring[mid].hash < hash) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    /* 如果 hash 大于所有节点，取第一个节点（环形） */
    uint32_t result_idx = (left == g_sharding->ring_size) ? 0 : left;

    *shard_id = g_sharding->ring[result_idx].shard_id;
    return MMDB_OK;
}

/**
 * @brief 获取分片信息
 */
int mmdb_sharding_info(mmdb_t* db, uint32_t shard_id, mmdb_shard_info_t* info) {
    if (!db || !info) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding) {
        return MMDB_ERR_INTERNAL;
    }

    for (uint32_t i = 0; i < g_sharding->shard_count; i++) {
        if (g_sharding->shards[i].id == shard_id) {
            info->shard_id = g_sharding->shards[i].id;
            strncpy(info->addr, g_sharding->shards[i].addr, sizeof(info->addr) - 1);
            info->key_count = g_sharding->shards[i].key_count;
            info->alive = g_sharding->shards[i].alive;
            return MMDB_OK;
        }
    }

    return MMDB_ERR_NOT_FOUND;
}

/**
 * @brief 手动迁移分片
 */
int mmdb_sharding_move(mmdb_t* db, const char* key, uint32_t from_shard, uint32_t to_shard) {
    if (!db || !key) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding) {
        return MMDB_ERR_INTERNAL;
    }

    /* 验证分片存在 */
    bool from_exists = false, to_exists = false;
    for (uint32_t i = 0; i < g_sharding->shard_count; i++) {
        if (g_sharding->shards[i].id == from_shard) from_exists = true;
        if (g_sharding->shards[i].id == to_shard) to_exists = true;
    }

    if (!from_exists || !to_exists) {
        return MMDB_ERR_INVALID;
    }

    /* TODO: 实际迁移数据 */

    return MMDB_OK;
}

/**
 * @brief 获取分片统计信息
 */
int mmdb_sharding_stats(mmdb_t* db, mmdb_sharding_stats_t* stats) {
    if (!db || !stats) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding) {
        return MMDB_ERR_INTERNAL;
    }

    stats->shard_count = g_sharding->shard_count;
    stats->total_keys = 0;
    stats->avg_keys = 0;
    stats->skew_factor = 0.0;

    if (g_sharding->shard_count == 0) {
        return MMDB_OK;
    }

    /* 计算总键数和平均值 */
    uint64_t sum = 0;
    for (uint32_t i = 0; i < g_sharding->shard_count; i++) {
        sum += g_sharding->shards[i].key_count;
    }
    stats->total_keys = sum;
    stats->avg_keys = sum / g_sharding->shard_count;

    /* 计算偏差因子 */
    if (stats->avg_keys > 0) {
        double variance = 0.0;
        for (uint32_t i = 0; i < g_sharding->shard_count; i++) {
            double diff = (double)g_sharding->shards[i].key_count - (double)stats->avg_keys;
            variance += diff * diff;
        }
        variance /= g_sharding->shard_count;
        stats->skew_factor = sqrt(variance) / (double)stats->avg_keys;
    }

    return MMDB_OK;
}

/**
 * @brief 停止分片服务
 */
int mmdb_sharding_stop(mmdb_t* db) {
    if (!db) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding) {
        return MMDB_OK;
    }

    if (g_sharding->ring) {
        free(g_sharding->ring);
        g_sharding->ring = NULL;
    }

    if (g_sharding->shards) {
        free(g_sharding->shards);
        g_sharding->shards = NULL;
    }

    free(g_sharding);
    g_sharding = NULL;

    return MMDB_OK;
}

/**
 * @brief 添加分片（在线扩容）
 */
int mmdb_sharding_add(mmdb_t* db, const char* shard_json) {
    if (!db || !shard_json) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding) {
        return MMDB_ERR_INTERNAL;
    }

    /* 解析新分片信息 */
    shard_node_t new_shard = {0};
    const char* id_start = strstr(shard_json, "\"id\"");
    if (!id_start) return MMDB_ERR_INVALID;
    id_start += 4;
    while (*id_start == ' ' || *id_start == ':') id_start++;
    new_shard.id = (uint32_t)atoi(id_start);
    new_shard.alive = true;

    const char* addr_start = strstr(shard_json, "\"addr\"");
    if (addr_start) {
        addr_start += 6;
        while (*addr_start == ' ' || *addr_start == ':') addr_start++;
        if (*addr_start == '"') addr_start++;
        const char* addr_end = strchr(addr_start, '"');
        if (addr_end) {
            size_t len = (size_t)(addr_end - addr_start);
            if (len >= sizeof(new_shard.addr)) len = sizeof(new_shard.addr) - 1;
            memcpy(new_shard.addr, addr_start, len);
            new_shard.addr[len] = '\0';
        }
    }

    /* 检查分片 ID 是否已存在 */
    for (uint32_t i = 0; i < g_sharding->shard_count; i++) {
        if (g_sharding->shards[i].id == new_shard.id) {
            return MMDB_ERR_INVALID;
        }
    }

    /* 扩展分片数组 */
    uint32_t new_count = g_sharding->shard_count + 1;
    shard_node_t* new_shards = (shard_node_t*)realloc(g_sharding->shards,
                                                       new_count * sizeof(shard_node_t));
    if (!new_shards) {
        return MMDB_ERR_NOMEM;
    }

    new_shards[new_count - 1] = new_shard;
    g_sharding->shards = new_shards;
    g_sharding->shard_count = new_count;

    /* 重建哈希环 */
    if (g_sharding->ring) {
        free(g_sharding->ring);
    }
    return build_hash_ring(g_sharding);
}

/**
 * @brief 移除分片（在线缩容）
 */
int mmdb_sharding_remove(mmdb_t* db, uint32_t shard_id) {
    if (!db) {
        return MMDB_ERR_INVALID;
    }

    if (!g_sharding || g_sharding->shard_count == 0) {
        return MMDB_ERR_INTERNAL;
    }

    /* 查找并移除分片 */
    uint32_t idx = 0;
    bool found = false;
    for (uint32_t i = 0; i < g_sharding->shard_count; i++) {
        if (g_sharding->shards[i].id == shard_id) {
            idx = i;
            found = true;
            break;
        }
    }

    if (!found) {
        return MMDB_ERR_NOT_FOUND;
    }

    /* 移动后续元素 */
    for (uint32_t i = idx; i < g_sharding->shard_count - 1; i++) {
        g_sharding->shards[i] = g_sharding->shards[i + 1];
    }

    g_sharding->shard_count--;

    /* 重建哈希环 */
    if (g_sharding->ring) {
        free(g_sharding->ring);
    }
    return build_hash_ring(g_sharding);
}
