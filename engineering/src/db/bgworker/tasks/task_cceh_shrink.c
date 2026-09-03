/**
 * @file task_cceh_shrink.c
 * @brief CCEH 索引缩容后台任务
 */

#include "task_iface.h"
#include "db/index/hash/cceh.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认负载因子阈值 */
#define DEFAULT_LOAD_FACTOR_THRESHOLD 0.25

/** 默认时间窗口大小 */
#define DEFAULT_TIME_WINDOW 3

/** 默认最小全局深度 */
#define DEFAULT_MIN_GLOBAL_DEPTH 1

/* ============================================================
 * 任务上下文
 * ============================================================ */

typedef struct cceh_shrink_ctx {
    cceh_index_t *index;              /**< CCEH 索引实例 */
    double        load_factor_threshold; /**< 负载因子阈值 */
    uint32_t      min_global_depth;   /**< 最小全局深度 */
} cceh_shrink_ctx_t;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 计算当前负载因子
 */
static double calculate_load_factor(const cceh_shrink_ctx_t *ctx) {
    if (!ctx || !ctx->index) {
        return 1.0;
    }

    uint32_t n_total = cceh_index_size(ctx->index);
    uint32_t segment_count = cceh_index_segment_count(ctx->index);

    if (segment_count == 0) {
        return 0.0;
    }

    /* 获取 segment 容量（通过全局变量或计算） */
    /* 这里简化处理，实际应该从索引获取 */
    uint32_t capacity = 8;  /* CCEH_DEFAULT_SEGMENT_CAPACITY */

    uint32_t max_records = segment_count * capacity;
    if (max_records == 0) {
        return 0.0;
    }

    return (double)n_total / max_records;
}

/* ============================================================
 * 任务回调实现
 * ============================================================ */

/**
 * @brief 初始化回调
 */
static int cceh_shrink_init(db_bg_task_context_t *base_ctx) {
    cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)malloc(sizeof(cceh_shrink_ctx_t));
    if (!ctx) {
        return -1;
    }
    memset(ctx, 0, sizeof(cceh_shrink_ctx_t));

    ctx->load_factor_threshold = DEFAULT_LOAD_FACTOR_THRESHOLD;
    ctx->min_global_depth = DEFAULT_MIN_GLOBAL_DEPTH;
    ctx->index = NULL;  /* 需要外部设置 */

    base_ctx->user_data = ctx;
    LOG_INFO("CCEH 缩容任务初始化完成");
    return 0;
}

/**
 * @brief 检查回调
 * @return > 0 满足缩容条件，= 0 不满足，< 0 错误
 */
static int cceh_shrink_check(db_bg_task_context_t *base_ctx) {
    cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)base_ctx->user_data;
    if (!ctx || !ctx->index) {
        return 0;
    }

    /* 检查全局深度是否允许缩容 */
    uint32_t global_depth = cceh_index_global_depth(ctx->index);
    if (global_depth <= ctx->min_global_depth) {
        return 0;
    }

    /* 计算负载因子 */
    double load_factor = calculate_load_factor(ctx);

    LOG_DEBUG("CCEH 缩容检查: load_factor=%.3f, threshold=%.3f, global_depth=%u",
              load_factor, ctx->load_factor_threshold, global_depth);

    if (load_factor < ctx->load_factor_threshold) {
        return 1;  /* 满足条件 */
    }

    return 0;
}

/**
 * @brief 执行回调：执行分片缩容
 * @return 0 成功，-1 失败
 */
static int cceh_shrink_execute(db_bg_task_context_t *base_ctx) {
    cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)base_ctx->user_data;
    if (!ctx || !ctx->index) {
        return -1;
    }

    uint32_t global_depth = cceh_index_global_depth(ctx->index);
    uint32_t segment_count = cceh_index_segment_count(ctx->index);

    LOG_INFO("CCEH 缩容执行: global_depth=%u, segment_count=%u",
             global_depth, segment_count);

    /* 分片缩容逻辑：每次只合并一对相邻 segment
     * 由于 CCEH 当前实现没有 shrink 接口，这里预留扩展点
     * 未来实现时需要：
     * 1. 找到可合并的一对 segment（local_depth 相等且指向同一 parent）
     * 2. 原子性地更新目录指针
     * 3. 标记旧 segment 为 retired
     * 4. epoch 回收
     */

    /* 暂时记录日志，实际缩容逻辑待实现 */
    LOG_INFO("CCEH 分片缩容: 合并一对相邻 segment（待实现）");

    return 0;
}

/**
 * @brief 清理回调
 */
static void cceh_shrink_cleanup(db_bg_task_context_t *base_ctx) {
    if (base_ctx && base_ctx->user_data) {
        free(base_ctx->user_data);
        base_ctx->user_data = NULL;
    }
    LOG_INFO("CCEH 缩容任务清理完成");
}

/* ============================================================
 * 任务定义
 * ============================================================ */

/**
 * @brief CCEH 缩容任务定义
 * @note 需要外部设置 ctx->index 才能生效
 */
db_bg_task_t g_cceh_shrink_task = {
    .name         = "cceh_shrink",
    .interval_ms  = 5000,          /* 默认 5 秒检查一次 */
    .init         = cceh_shrink_init,
    .check        = cceh_shrink_check,
    .execute      = cceh_shrink_execute,
    .cleanup      = cceh_shrink_cleanup,
    .ctx          = NULL
};

/* ============================================================
 * 辅助 API
 * ============================================================ */

/**
 * @brief 设置缩容任务的目标索引
 * @param index CCEH 索引实例
 */
void db_bgworker_cceh_shrink_set_index(cceh_index_t *index) {
    /* 通过 GUC 或直接修改任务上下文 */
    /* 这里简化处理，实际应该在注册时传递 */
    if (g_cceh_shrink_task.ctx) {
        cceh_shrink_ctx_t *ctx = (cceh_shrink_ctx_t *)g_cceh_shrink_task.ctx->user_data;
        if (ctx) {
            ctx->index = index;
        }
    }
}