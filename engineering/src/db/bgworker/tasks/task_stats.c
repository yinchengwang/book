/**
 * @file task_stats.c
 * @brief 统计信息收集后台任务
 */

#include "task_iface.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 任务上下文
 * ============================================================ */

typedef struct stats_ctx {
    uint64_t last_total_operations;  /**< 上次总操作数 */
    double   last_check_time;        /**< 上次检查时间 */
} stats_ctx_t;

/* ============================================================
 * 任务回调实现
 * ============================================================ */

/**
 * @brief 初始化回调
 */
static int stats_init(db_bg_task_context_t *ctx) {
    stats_ctx_t *stats_ctx = (stats_ctx_t *)malloc(sizeof(stats_ctx_t));
    if (!stats_ctx) {
        return -1;
    }
    memset(stats_ctx, 0, sizeof(stats_ctx_t));
    ctx->user_data = stats_ctx;
    LOG_INFO("%s", "统计收集任务初始化完成");
    return 0;
}

/**
 * @brief 检查回调
 * @note 统计收集任务不需要条件触发，定期执行
 */
static int stats_check(db_bg_task_context_t *ctx) {
    (void)ctx;
    return 1;  /* 总是满足条件 */
}

/**
 * @brief 执行回调：收集统计信息
 */
static int stats_execute(db_bg_task_context_t *ctx) {
    stats_ctx_t *stats_ctx = (stats_ctx_t *)ctx->user_data;

    /* 收集各种统计信息：
     * - 缓冲区命中率
     * - 索引使用情况
     * - 查询延迟分布
     * - 内存使用情况
     */

    /* 这里是占位实现，实际应该从各子系统获取统计信息 */
    LOG_DEBUG("%s", "统计信息收集: 执行一次统计采样");

    (void)stats_ctx;
    return 0;
}

/**
 * @brief 清理回调
 */
static void stats_cleanup(db_bg_task_context_t *ctx) {
    if (ctx && ctx->user_data) {
        free(ctx->user_data);
        ctx->user_data = NULL;
    }
    LOG_INFO("%s", "统计收集任务清理完成");
}

/* ============================================================
 * 任务定义
 * ============================================================ */

/**
 * @brief 统计收集任务定义
 */
db_bg_task_t g_stats_task = {
    .name         = "stats_collector",
    .interval_ms  = 60000,         /* 默认 1 分钟收集一次 */
    .init         = stats_init,
    .check        = stats_check,
    .execute      = stats_execute,
    .cleanup      = stats_cleanup,
    .ctx          = NULL
};
