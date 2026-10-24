/*
 * 向量索引重建策略
 *
 * 决定何时使用边修复 vs 完整重建。
 */

#ifndef VECTOR_REBUILD_STRATEGY_H
#define VECTOR_REBUILD_STRATEGY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 重建决策
 */
typedef enum rebuild_decision {
    REBUILD_DECISION_NONE,      /* 无需重建 */
    REBUILD_DECISION_REPAIR,    /* 边修复足够 */
    REBUILD_DECISION_REBUILD    /* 需要完整重建 */
} rebuild_decision_t;

/*
 * 重建策略配置
 */
typedef struct rebuild_strategy_config {
    int32_t deleted_ratio_threshold;   /* 删除比例阈值（百分比），超过则重建 */
    int32_t min_deleted_count;        /* 最小删除数量，超过则考虑重建 */
    bool    prefer_rebuild;           /* 是否优先选择重建 */
} rebuild_strategy_config_t;

/*
 * 重建策略上下文
 */
typedef struct rebuild_strategy rebuild_strategy_t;

/**
 * 初始化默认重建策略配置
 */
void rebuild_strategy_config_init_default(rebuild_strategy_config_t *config);

/**
 * 创建重建策略实例
 *
 * @param config 策略配置（可为 NULL）
 * @return 策略实例，失败返回 NULL
 */
rebuild_strategy_t *rebuild_strategy_create(const rebuild_strategy_config_t *config);

/**
 * 销毁重建策略实例
 */
void rebuild_strategy_destroy(rebuild_strategy_t *strategy);

/**
 * 分析并做出重建决策
 *
 * @param strategy        策略实例
 * @param deleted_count   已删除向量数量
 * @param total_count     向量总数
 * @return 重建决策
 */
rebuild_decision_t rebuild_strategy_decide(rebuild_strategy_t *strategy,
                                          int32_t deleted_count,
                                          int32_t total_count);

/**
 * 获取删除比例
 *
 * @param deleted_count 已删除数量
 * @param total_count   总数
 * @return 删除比例 [0.0, 1.0]
 */
float rebuild_strategy_deleted_ratio(int32_t deleted_count, int32_t total_count);

/**
 * 更新策略配置
 *
 * @param strategy 策略实例
 * @param config  新配置
 */
void rebuild_strategy_update_config(rebuild_strategy_t *strategy,
                                   const rebuild_strategy_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_REBUILD_STRATEGY_H */
