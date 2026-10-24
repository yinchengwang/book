/**
 * @file include/db/bgworker.h
 * @brief 后台任务调度器公共 API
 */
#ifndef DB_BGWORKER_H
#define DB_BGWORKER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct db_bg_task db_bg_task_t;
typedef struct db_bg_task_context db_bg_task_context_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 启动后台任务调度器
 * @return 0 成功，-1 失败
 */
int db_bgworker_start(void);

/**
 * @brief 停止后台任务调度器
 * @return 0 成功，-1 失败
 */
int db_bgworker_stop(void);

/* ============================================================
 * 任务管理
 * ============================================================ */

/**
 * @brief 注册后台任务
 * @param task 任务定义
 * @param slot_out 输出分配的任务槽位（可为 NULL）
 * @return 0 成功，-1 失败
 */
int db_bgworker_register(db_bg_task_t *task, int *slot_out);

/**
 * @brief 注销后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_unregister(const char *name);

/**
 * @brief 暂停后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_pause(const char *name);

/**
 * @brief 恢复后台任务
 * @param name 任务名称
 * @return 0 成功，-1 未找到
 */
int db_bgworker_resume(const char *name);

/* ============================================================
 * 状态查询
 * ============================================================ */

/** 任务状态（与 task_iface.h 同步） */
typedef enum db_bg_task_status_e {
    DB_BG_TASK_STATUS_IDLE       = 0,
    DB_BG_TASK_STATUS_CHECKING   = 1,
    DB_BG_TASK_STATUS_EXECUTING  = 2,
    DB_BG_TASK_STATUS_PAUSED     = 3,
    DB_BG_TASK_STATUS_STOPPED    = 4
} db_bg_task_status_t;

/**
 * @brief 获取任务状态
 * @param name 任务名称
 * @param status_out 输出状态
 * @return 0 成功，-1 未找到
 */
int db_bgworker_get_status(const char *name, unsigned int *status_out);

/**
 * @brief 调度器统计信息
 */
typedef struct db_bgworker_stats {
    uint32_t  task_count;         /**< 已注册任务数 */
    uint64_t  running_time_ms;    /**< 运行时间（毫秒） */
    uint64_t  total_cycles;       /**< 总检查周期数 */
    uint64_t  total_executions;   /**< 总执行次数 */
} db_bgworker_stats_t;

/**
 * @brief 获取调度器统计信息
 * @param stats_out 输出统计信息
 * @return 0 成功
 */
int db_bgworker_get_stats(db_bgworker_stats_t *stats_out);

#ifdef __cplusplus
}
#endif

#endif /* DB_BGWORKER_H */
