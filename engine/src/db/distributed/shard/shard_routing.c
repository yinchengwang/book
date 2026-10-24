/**
 * @file shard_routing.c
 * @brief 分片路由层实现
 *
 * 实现基于 Hash/Range/List 策略的数据路由分发。
 */

#include "db/shard_routing.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 日志宏定义 */
#define LOG_INFO(fmt, ...) fprintf(stdout, "[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

/* ========================================================================
 * MurmurHash3 实现 (32-bit)
 * ======================================================================== */

static uint32_t murmurhash3_32(const void *key, int len, uint32_t seed) {
    const uint8_t *data = (const uint8_t *)key;
    int nblocks = len / 4;
    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    /* body */
    const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    /* tail */
    const uint8_t *tail = data + nblocks * 4;
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16; /* fallthrough */
        case 2: k1 ^= tail[1] << 8;  /* fallthrough */
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= c2;
                h1 ^= k1;
    }

    /* finalization */
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 复制字符串，失败返回 NULL
 */
static char* safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) {
        LOG_ERROR("内存分配失败: strdup");
    }
    return dup;
}

/**
 * @brief 释放分片范围数组
 */
static void free_ranges(shard_range_t *ranges, uint32_t count) {
    if (!ranges) return;
    for (uint32_t i = 0; i < count; i++) {
        free(ranges[i].start);
        free(ranges[i].end);
    }
    free(ranges);
}

/**
 * @brief 释放分片映射
 */
static void free_shard_map(shard_map_t *map) {
    if (!map) return;
    free(map->key.column);
    free_ranges(map->ranges, map->range_count);
    free(map->node_ids);
}

/* ========================================================================
 * 路由表 API 实现
 * ======================================================================== */

shard_routing_table_t* shard_routing_create(void) {
    shard_routing_table_t *table = (shard_routing_table_t *)calloc(1, sizeof(shard_routing_table_t));
    if (!table) {
        LOG_ERROR("内存分配失败: shard_routing_table_t");
        return NULL;
    }
    table->maps = NULL;
    table->map_count = 0;
    LOG_INFO("创建路由表");
    return table;
}

void shard_routing_free(shard_routing_table_t *table) {
    if (!table) return;

    for (uint32_t i = 0; i < table->map_count; i++) {
        free_shard_map(&table->maps[i]);
    }
    free(table->maps);
    free(table);
    LOG_INFO("释放路由表");
}

int shard_routing_add_map(shard_routing_table_t *table, const shard_map_t *map) {
    if (!table || !map) {
        LOG_ERROR("参数无效: table 或 map 为空");
        return -1;
    }

    /* 扩容映射数组 */
    shard_map_t *new_maps = (shard_map_t *)realloc(table->maps,
        (table->map_count + 1) * sizeof(shard_map_t));
    if (!new_maps) {
        LOG_ERROR("内存分配失败: realloc maps");
        return -1;
    }
    table->maps = new_maps;

    /* 深拷贝 map 数据 */
    shard_map_t *dst = &table->maps[table->map_count];
    dst->key.column = safe_strdup(map->key.column);
    if (!dst->key.column && map->key.column) {
        return -1;
    }
    dst->key.strategy = map->key.strategy;

    /* 复制范围 */
    if (map->ranges && map->range_count > 0) {
        dst->ranges = (shard_range_t *)malloc(map->range_count * sizeof(shard_range_t));
        if (!dst->ranges) {
            LOG_ERROR("内存分配失败: ranges");
            free(dst->key.column);
            return -1;
        }
        dst->range_count = map->range_count;
        for (uint32_t i = 0; i < map->range_count; i++) {
            dst->ranges[i].start = safe_strdup(map->ranges[i].start);
            dst->ranges[i].end = safe_strdup(map->ranges[i].end);
            dst->ranges[i].shard_id = map->ranges[i].shard_id;
        }
    } else {
        dst->ranges = NULL;
        dst->range_count = 0;
    }

    /* 复制节点 ID */
    if (map->node_ids && map->node_count > 0) {
        dst->node_ids = (uint64_t *)malloc(map->node_count * sizeof(uint64_t));
        if (!dst->node_ids) {
            LOG_ERROR("内存分配失败: node_ids");
            free_shard_map(dst);
            return -1;
        }
        memcpy(dst->node_ids, map->node_ids, map->node_count * sizeof(uint64_t));
        dst->node_count = map->node_count;
    } else {
        dst->node_ids = NULL;
        dst->node_count = 0;
    }

    table->map_count++;
    LOG_INFO("添加分片映射: 列=%s, 策略=%d, 节点数=%u",
             map->key.column ? map->key.column : "null",
             map->key.strategy,
             map->node_count);
    return 0;
}

/**
 * @brief Hash 路由
 */
static int route_hash(const shard_map_t *map, const char *key, shard_route_result_t *result) {
    if (!map->node_ids || map->node_count == 0) {
        LOG_ERROR("Hash 路由失败: 无可用节点");
        return -1;
    }

    uint32_t hash = murmurhash3_32(key, (int)strlen(key), 0);
    uint32_t index = hash % map->node_count;

    result->shard_id = (uint64_t)index;
    result->node_id = map->node_ids[index];
    result->is_local = false; /* 假设非本地 */

    return 0;
}

/**
 * @brief Range 路由
 */
static int route_range(const shard_map_t *map, const char *key, shard_route_result_t *result) {
    for (uint32_t i = 0; i < map->range_count; i++) {
        const shard_range_t *r = &map->ranges[i];
        bool match = false;

        if (r->start && strcmp(key, r->start) < 0) continue;

        if (r->end) {
            match = (strcmp(key, r->end) <= 0);
        } else {
            /* 无上界，匹配最后一个范围 */
            match = (i == map->range_count - 1) || (strcmp(key, r->start) >= 0);
        }

        if (match) {
            result->shard_id = r->shard_id;
            /* 找到对应的节点 ID，简单映射 */
            if (map->node_ids && r->shard_id < map->node_count) {
                result->node_id = map->node_ids[r->shard_id];
            } else {
                result->node_id = 0;
            }
            result->is_local = false;
            return 0;
        }
    }

    LOG_ERROR("Range 路由失败: 键 %s 未匹配任何范围", key);
    return -1;
}

/**
 * @brief List 路由
 */
static int route_list(const shard_map_t *map, const char *key, shard_route_result_t *result) {
    for (uint32_t i = 0; i < map->range_count; i++) {
        const shard_range_t *r = &map->ranges[i];
        /* List 策略下 start 是具体的匹配值 */
        if (r->start && strcmp(key, r->start) == 0) {
            result->shard_id = r->shard_id;
            if (map->node_ids && r->shard_id < map->node_count) {
                result->node_id = map->node_ids[r->shard_id];
            } else {
                result->node_id = 0;
            }
            result->is_local = false;
            return 0;
        }
    }

    LOG_ERROR("List 路由失败: 键 %s 未匹配任何列表项", key);
    return -1;
}

int shard_route(const shard_routing_table_t *table, const char *key, shard_route_result_t *result) {
    if (!table || !key || !result) {
        LOG_ERROR("参数无效");
        return -1;
    }

    /* 遍历所有映射，寻找包含该列名的配置 */
    for (uint32_t i = 0; i < table->map_count; i++) {
        const shard_map_t *map = &table->maps[i];

        /* 简单路由: 假设只有一张映射，或者所有映射使用相同的键 */
        switch (map->key.strategy) {
            case SHARD_STRATEGY_HASH:
                return route_hash(map, key, result);
            case SHARD_STRATEGY_RANGE:
                return route_range(map, key, result);
            case SHARD_STRATEGY_LIST:
                return route_list(map, key, result);
            case SHARD_STRATEGY_COMPOSITE:
                /* 复合策略暂退化为 Hash */
                return route_hash(map, key, result);
            default:
                LOG_ERROR("未知分片策略: %d", map->key.strategy);
                return -1;
        }
    }

    LOG_ERROR("路由表为空");
    return -1;
}

int shard_route_batch(const shard_routing_table_t *table, const char **keys, uint32_t count, shard_route_result_t *results) {
    if (!table || !keys || !results || count == 0) {
        LOG_ERROR("参数无效");
        return 0;
    }

    int success_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (shard_route(table, keys[i], &results[i]) == 0) {
            success_count++;
        } else {
            /* 标记失败的结果 */
            results[i].shard_id = 0;
            results[i].node_id = 0;
            results[i].is_local = false;
        }
    }

    return success_count;
}
