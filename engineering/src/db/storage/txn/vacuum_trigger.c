/**
 * @file vacuum_trigger.c
 * @brief 自动 vacuum 阈值触发（C2-1 T8）
 */
#include "db/vacuum_trigger.h"
#include "db/txn.h"
#include "db/core/log.h"

#include <stdatomic.h>

#define TRIGGER_THRESHOLD 1000  /* 累计事务数超过此值触发一次 autovacuum */

static atomic_int s_since_last_vacuum = 0;

void vacuum_trigger_check(void) {
    /* 自增累计；达到阈值后触发 autovacuum 并重置 */
    int n = atomic_fetch_add(&s_since_last_vacuum, 1) + 1;
    if (n >= TRIGGER_THRESHOLD) {
        VacuumParams params = {
            .vacuum_cost_delay = VACUUM_DEFAULT_COST_DELAY,
            .vacuum_cost_limit = VACUUM_DEFAULT_COST_LIMIT,
            .vacuum_page_speed = 1000,
            .freeze_min_age    = VACUUM_DEFAULT_FREEZE_MIN_AGE,
            .freeze_max_age    = VACUUM_DEFAULT_FREEZE_MAX_AGE,
        };
        vacuum_autovacuum(&params);
        atomic_store(&s_since_last_vacuum, 0);
        LOG_INFO("vacuum_trigger_check: 触发 autovacuum（累计 %d 事务）", n);
    }
}