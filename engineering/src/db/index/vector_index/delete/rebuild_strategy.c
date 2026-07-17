/*
 * 向量索引重建策略实现
 */

#include <stdlib.h>
#include <string.h>

#include <db/index/vector_index/delete/rebuild_strategy.h>

struct rebuild_strategy {
    rebuild_strategy_config_t config;
};

void rebuild_strategy_config_init_default(rebuild_strategy_config_t *config)
{
    if (!config) return;

    /* 默认：删除比例超过 30% 或删除数量超过 10000 时建议重建 */
    config->deleted_ratio_threshold = 30;
    config->min_deleted_count = 10000;
    config->prefer_rebuild = false;
}

rebuild_strategy_t *rebuild_strategy_create(const rebuild_strategy_config_t *config)
{
    rebuild_strategy_t *strategy = (rebuild_strategy_t *)malloc(
        sizeof(rebuild_strategy_t));
    if (!strategy) {
        return NULL;
    }

    if (config) {
        strategy->config = *config;
    } else {
        rebuild_strategy_config_init_default(&strategy->config);
    }

    return strategy;
}

void rebuild_strategy_destroy(rebuild_strategy_t *strategy)
{
    if (strategy) {
        free(strategy);
    }
}

float rebuild_strategy_deleted_ratio(int32_t deleted_count, int32_t total_count)
{
    if (total_count <= 0) {
        return 0.0f;
    }
    return (float)deleted_count / (float)total_count;
}

rebuild_decision_t rebuild_strategy_decide(rebuild_strategy_t *strategy,
                                          int32_t deleted_count,
                                          int32_t total_count)
{
    if (!strategy) {
        /* 默认策略 */
        if (total_count <= 0 || deleted_count <= 0) {
            return REBUILD_DECISION_NONE;
        }

        float ratio = rebuild_strategy_deleted_ratio(deleted_count, total_count);
        if (ratio >= 0.30f || deleted_count >= 10000) {
            return REBUILD_DECISION_REBUILD;
        }

        return REBUILD_DECISION_REPAIR;
    }

    const rebuild_strategy_config_t *cfg = &strategy->config;

    /* 无需重建 */
    if (deleted_count <= 0 || total_count <= 0) {
        return REBUILD_DECISION_NONE;
    }

    float ratio = rebuild_strategy_deleted_ratio(deleted_count, total_count);

    bool ratio_triggered = (ratio * 100.0f) >= cfg->deleted_ratio_threshold;
    bool count_triggered = deleted_count >= cfg->min_deleted_count;

    /* S9：任一条件触发即建议重建（向 Repair 升级而非仅 Repair） */
    /* 优先级：双触发 > 单触发 > 默认 REPAIR */
    if (ratio_triggered || count_triggered) {
        return REBUILD_DECISION_REBUILD;
    }

    /* 都不触发：若用户偏好重建且存在非零删除，按 Repair 升级为 Rebuild */
    if (cfg->prefer_rebuild) {
        return REBUILD_DECISION_REBUILD;
    }

    /* 删除比例低，使用边修复即可 */
    return REBUILD_DECISION_REPAIR;
}

void rebuild_strategy_update_config(rebuild_strategy_t *strategy,
                                   const rebuild_strategy_config_t *config)
{
    if (!strategy || !config) {
        return;
    }

    strategy->config = *config;
}
