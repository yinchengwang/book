/*
 * shard_coordinator.h - 分片协调器接口
 */

#ifndef DB_SHARDING_COORDINATOR_H
#define DB_SHARDING_COORDINATOR_H

#include "shard_balance.h"
#include "sharding.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 负载信息 */
typedef struct shard_load {
    int shard_id;
    uint64_t row_count;
    double qps;
    double latency_ms;
    double cpu_usage;
    int64_t size_bytes;
    time_t last_updated;
} shard_load_t;

/* 负载收集器 */
typedef struct load_collector load_collector_t;

/* 分片协调器 */
typedef struct shard_coordinator shard_coordinator_t;

/**
 * @brief 创建负载收集器
 */
load_collector_t *load_collector_create(int initial_capacity);

/**
 * @brief 销毁负载收集器
 */
void load_collector_destroy(load_collector_t *collector);

/**
 * @brief 更新分片负载信息
 */
int load_collector_update(load_collector_t *collector, const shard_load_t *load);

/**
 * @brief 获取分片负载信息
 */
const shard_load_t *load_collector_get(load_collector_t *collector, int shard_id);

/**
 * @brief 计算倾斜度（max / avg）
 */
double load_collector_calculate_skew(load_collector_t *collector);

/**
 * @brief 创建分片协调器
 */
shard_coordinator_t *shard_coordinator_create(const shard_balance_config_t *config,
                                               shard_router_t *router);

/**
 * @brief 销毁分片协调器
 */
void shard_coordinator_destroy(shard_coordinator_t *coord);

/**
 * @brief 启动协调器（启动后台监控线程）
 */
int shard_coordinator_start(shard_coordinator_t *coord);

/**
 * @brief 停止协调器
 */
void shard_coordinator_stop(shard_coordinator_t *coord);

/**
 * @brief 手动触发再平衡检查
 */
int shard_coordinator_check_and_rebalance(shard_coordinator_t *coord);

/**
 * @brief 选择最小负载的分片
 */
int shard_coordinator_select_least_load(shard_coordinator_t *coord,
                                         const int *candidate_shards,
                                         int count);

/**
 * @brief 获取分片路由器
 */
shard_router_t *shard_coordinator_get_router(shard_coordinator_t *coord);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_COORDINATOR_H */
