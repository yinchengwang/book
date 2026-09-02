// engineering/src/db/sharding/shard_balance.c
#include "db/sharding/shard_balance.h"
#include <stdlib.h>
#include <string.h>

shard_balance_config_t *shard_balance_config_create(void) {
    shard_balance_config_t *cfg = (shard_balance_config_t *)calloc(1,
        sizeof(shard_balance_config_t));
    if (!cfg) return NULL;

    cfg->skew_threshold = DEFAULT_SKEW_THRESHOLD;
    cfg->max_shard_size = DEFAULT_MAX_SHARD_SIZE;
    cfg->check_interval_ms = DEFAULT_CHECK_INTERVAL_MS;
    cfg->strategy = MIGRATE_INCREMENTAL;  // 默认增量迁移
    cfg->auto_rebalance = true;

    return cfg;
}

void shard_balance_config_destroy(shard_balance_config_t *cfg) {
    free(cfg);
}

migrate_strategy_t migrate_strategy_from_string(const char *str) {
    if (!str) return MIGRATE_INCREMENTAL;
    if (strcmp(str, "virtual-node") == 0 || strcmp(str, "vnode") == 0) {
        return MIGRATE_VIRTUAL_NODE;
    }
    return MIGRATE_INCREMENTAL;
}

const char *migrate_strategy_to_string(migrate_strategy_t s) {
    switch (s) {
        case MIGRATE_VIRTUAL_NODE: return "virtual-node";
        case MIGRATE_INCREMENTAL:
        default: return "incremental";
    }
}