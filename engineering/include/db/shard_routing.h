/**
 * @file shard_routing.h
 * @brief 分片路由层接口
 *
 * 实现数据在多节点间的路由分发，支持 Hash/Range/List/Composite 策略。
 */

#ifndef DB_SHARD_ROUTING_H
#define DB_SHARD_ROUTING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 分片策略
 * ======================================================================== */

typedef enum {
    SHARD_STRATEGY_HASH = 0,      /* Hash 分片 */
    SHARD_STRATEGY_RANGE,         /* Range 分片 */
    SHARD_STRATEGY_LIST,          /* List 分片 */
    SHARD_STRATEGY_COMPOSITE,     /* 复合分片 */
} shard_strategy_t;

/* ========================================================================
 * 分片键
 * ======================================================================== */

typedef struct shard_key_s {
    char *column;                 /* 分片键列名 */
    shard_strategy_t strategy;    /* 分片策略 */
} shard_key_t;

/* ========================================================================
 * 分片范围 (Range/List 策略)
 * ======================================================================== */

typedef struct shard_range_s {
    char *start;                  /* 范围起始值 (List 时为具体值) */
    char *end;                    /* 范围结束值 (List 时可为 NULL) */
    uint64_t shard_id;            /* 对应的分片 ID */
} shard_range_t;

/* ========================================================================
 * 分片映射
 * ======================================================================== */

typedef struct shard_map_s {
    shard_key_t key;              /* 分片键定义 */
    shard_range_t *ranges;        /* 范围数组 (Hash 策略时为 NULL) */
    uint32_t range_count;         /* 范围数量 */
    uint64_t *node_ids;           /* 节点 ID 数组 */
    uint32_t node_count;          /* 节点数量 */
} shard_map_t;

/* ========================================================================
 * 路由表
 * ======================================================================== */

typedef struct shard_routing_table_s {
    shard_map_t *maps;            /* 分片映射数组 */
    uint32_t map_count;           /* 映射数量 */
} shard_routing_table_t;

/* ========================================================================
 * 路由结果
 * ======================================================================== */

typedef struct shard_route_result_s {
    uint64_t shard_id;            /* 目标分片 ID */
    uint64_t node_id;             /* 目标节点 ID */
    bool is_local;                /* 是否为本地节点 */
} shard_route_result_t;

/* ========================================================================
 * 路由表操作 API
 * ======================================================================== */

/**
 * @brief 创建空的路由表
 * @return 路由表指针，失败返回 NULL
 */
shard_routing_table_t* shard_routing_create(void);

/**
 * @brief 释放路由表
 * @param table 路由表指针
 */
void shard_routing_free(shard_routing_table_t *table);

/**
 * @brief 向路由表添加分片映射
 * @param table 路由表
 * @param map 分片映射配置
 * @return 0 成功，-1 失败
 */
int shard_routing_add_map(shard_routing_table_t *table, const shard_map_t *map);

/**
 * @brief 单键路由
 * @param table 路由表
 * @param key 分片键值
 * @param result 路由结果输出
 * @return 0 成功，-1 失败
 */
int shard_route(const shard_routing_table_t *table, const char *key, shard_route_result_t *result);

/**
 * @brief 批量路由
 * @param table 路由表
 * @param keys 分片键值数组
 * @param count 键值数量
 * @param results 路由结果数组 (需预分配至少 count 个空间)
 * @return 成功路由的数量
 */
int shard_route_batch(const shard_routing_table_t *table, const char **keys, uint32_t count, shard_route_result_t *results);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARD_ROUTING_H */
