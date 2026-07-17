/**
 * @file task_iface.h
 * @brief 后台任务接口定义
 */
#ifndef DB_BGWORKER_TASK_IFACE_H
#define DB_BGWORKER_TASK_IFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 任务状态 */
typedef enum db_bg_task_status_e {
    DB_BG_TASK_STATUS_IDLE       = 0,  /**< 空闲 */
    DB_BG_TASK_STATUS_CHECKING   = 1,  /**< 检查中 */
    DB_BG_TASK_STATUS_EXECUTING  = 2,  /**< 执行中 */
    DB_BG_TASK_STATUS_PAUSED     = 3,  /**< 暂停 */
    DB_BG_TASK_STATUS_STOPPED    = 4   /**< 已停止 */
} db_bg_task_status_t;

/**
 * @brief 后台任务上下文
 */
typedef struct db_bg_task_context {
    void          *user_data;           /**< 用户自定义数据 */
    uint64_t       last_check_time;     /**< 上次检查时间 (ms) */
    uint32_t       consecutive_trigger; /**< 连续触发计数 */
    atomic_uint    status;              /**< 任务状态 */
} db_bg_task_context_t;

/**
 * @brief 任务初始化回调
 * @param ctx 任务上下文
 * @return 0 成功，-1 失败
 */
typedef int (*db_bg_task_init_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 任务检查回调
 * @param ctx 任务上下文
 * @return > 0 满足触发条件，= 0 不满足条件，< 0 错误
 */
typedef int (*db_bg_task_check_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 任务执行回调
 * @param ctx 任务上下文
 * @return 0 成功，-1 失败
 */
typedef int (*db_bg_task_execute_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 任务清理回调
 * @param ctx 任务上下文
 */
typedef void (*db_bg_task_cleanup_fn)(db_bg_task_context_t *ctx);

/**
 * @brief 后台任务定义
 */
typedef struct db_bg_task {
    const char               *name;     /**< 任务名称（唯一标识） */
    uint32_t                  interval_ms;  /**< 检查间隔（毫秒） */
    db_bg_task_init_fn        init;     /**< 初始化回调 */
    db_bg_task_check_fn       check;    /**< 检查回调 */
    db_bg_task_execute_fn     execute;  /**< 执行回调 */
    db_bg_task_cleanup_fn     cleanup;  /**< 清理回调 */
    db_bg_task_context_t     *ctx;      /**< 任务上下文（内部填充） */
} db_bg_task_t;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 获取任务当前状态
 * @param ctx 任务上下文
 * @return 任务状态
 */
static inline db_bg_task_status_t db_bg_task_get_status(const db_bg_task_context_t *ctx) {
    return (db_bg_task_status_t)atomic_load_explicit(&ctx->status, memory_order_acquire);
}

/**
 * @brief 设置任务状态
 * @param ctx 任务上下文
 * @param status 新状态
 */
static inline void db_bg_task_set_status(db_bg_task_context_t *ctx, db_bg_task_status_t status) {
    atomic_store_explicit(&ctx->status, status, memory_order_release);
}

#ifdef __cplusplus
}
#endif

#endif /* DB_BGWORKER_TASK_IFACE_H */
