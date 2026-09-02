/*
 * shard_balance.h - 分片平衡配置与迁移任务
 */

#ifndef DB_SHARDING_BALANCE_H
#define DB_SHARDING_BALANCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 迁移策略 */
typedef enum {
    MIGRATE_INCREMENTAL = 0,     /* 增量迁移（Range/List 分片） */
    MIGRATE_VIRTUAL_NODE         /* 虚拟节点迁移（Hash 分片） */
} migrate_strategy_t;

/* 迁移状态 */
typedef enum {
    MIGRATE_STATUS_PENDING = 0,
    MIGRATE_STATUS_RUNNING,
    MIGRATE_STATUS_COMPLETED,
    MIGRATE_STATUS_FAILED
} migrate_status_t;

/* 平衡配置 */
typedef struct shard_balance_config {
    double skew_threshold;        /* 倾斜阈值（默认 1.5） */
    int64_t max_shard_size;      /* 最大分片大小（默认 10GB） */
    int check_interval_ms;       /* 检查间隔（默认 60000ms） */
    migrate_strategy_t strategy; /* 默认迁移策略 */
    bool auto_rebalance;         /* 自动再平衡开关（默认 true） */
} shard_balance_config_t;

/* 迁移任务 */
typedef struct migrate_task {
    int task_id;
    int source_shard;
    int target_shard;
    migrate_strategy_t strategy;
    void *key_range;
    double progress;              /* 0.0-1.0 */
    migrate_status_t status;
} migrate_task_t;

/* 配置默认值 */
#define DEFAULT_SKEW_THRESHOLD 1.5
#define DEFAULT_MAX_SHARD_SIZE (10ULL * 1024 * 1024 * 1024)
#define DEFAULT_CHECK_INTERVAL_MS 60000

/**
 * @brief 创建默认平衡配置
 */
shard_balance_config_t *shard_balance_config_create(void);

/**
 * @brief 销毁平衡配置
 */
void shard_balance_config_destroy(shard_balance_config_t *config);

/**
 * @brief 从配置字符串解析迁移策略
 */
migrate_strategy_t migrate_strategy_from_string(const char *str);

/**
 * @brief 获取迁移策略名称
 */
const char *migrate_strategy_to_string(migrate_strategy_t strategy);

#ifdef __cplusplus
}
#endif

#endif /* DB_SHARDING_BALANCE_H */
