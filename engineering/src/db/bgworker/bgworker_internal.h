/**
 * @file bgworker_internal.h
 * @brief 后台任务调度器内部数据结构
 */
#ifndef DB_BGWORKER_INTERNAL_H
#define DB_BGWORKER_INTERNAL_H

#include "tasks/task_iface.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大任务槽位数 */
#define DB_BGWORKER_MAX_TASKS 16

/** 默认检查间隔（毫秒） */
#define DB_BGWORKER_DEFAULT_TICK_MS 1000

/** 默认停止超时（秒） */
#define DB_BGWORKER_STOP_TIMEOUT_SEC 30

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 任务槽位 */
typedef struct db_bg_task_slot {
    db_bg_task_t         *task;          /**< 注册的任务 */
    uint64_t              next_check_time; /**< 下次检查时间 (ms) */
    bool                  in_use;        /**< 是否在使用 */
} db_bg_task_slot_t;

/** 调度器统计信息 */
typedef struct db_bgworker_stats {
    uint32_t              task_count;    /**< 已注册任务数 */
    uint64_t              running_time_ms; /**< 运行时间（毫秒） */
    uint64_t              total_cycles;  /**< 总检查周期数 */
    uint64_t              total_executions; /**< 总执行次数 */
} db_bgworker_stats_t;

/** 后台任务调度器 */
typedef struct db_bg_worker {
    pthread_t             thread;        /**< 后台线程 */
    db_bg_task_slot_t     slots[DB_BGWORKER_MAX_TASKS]; /**< 任务槽位 */
    uint32_t              num_slots;     /**< 槽位数量 */
    atomic_bool           running;       /**< 是否运行中 */
    atomic_bool           stopping;      /**< 是否正在停止 */
    uint64_t              start_time_ms; /**< 启动时间 */
    uint32_t              tick_ms;       /**< 检查间隔 */
    pthread_mutex_t       mutex;         /**< 保护槽位访问 */
} db_bg_worker_t;

/* ============================================================
 * 全局调度器实例（供 API 和调度器主循环共享）
 * ============================================================ */
extern db_bg_worker_t g_worker;

/* ============================================================
 * 内部函数声明
 * ============================================================ */

/**
 * @brief 获取当前时间（毫秒）
 */
uint64_t _db_bgworker_now_ms(void);

/**
 * @brief 调度器主循环
 * @param arg 调度器指针
 * @return NULL
 */
void *_db_bgworker_main_loop(void *arg);

/**
 * @brief 初始化任务槽位
 * @param worker 调度器
 */
void _db_bgworker_init_slots(db_bg_worker_t *worker);

/**
 * @brief 清理任务槽位
 * @param worker 调度器
 */
void _db_bgworker_cleanup_slots(db_bg_worker_t *worker);

/**
 * @brief 查找任务槽位
 * @param worker 调度器
 * @param name 任务名称
 * @return 槽位索引，-1 未找到
 */
int _db_bgworker_find_slot(db_bg_worker_t *worker, const char *name);

/**
 * @brief 查找空闲槽位
 * @param worker 调度器
 * @return 槽位索引，-1 无空闲
 */
int _db_bgworker_find_free_slot(db_bg_worker_t *worker);

/**
 * @brief 分配任务上下文
 * @param task 任务定义
 * @return 上下文指针，NULL 失败
 */
db_bg_task_context_t *_db_bgworker_alloc_context(db_bg_task_t *task);

/**
 * @brief 释放任务上下文
 * @param ctx 上下文
 */
void _db_bgworker_free_context(db_bg_task_context_t *ctx);

/* ============================================================
 * 配置注册与访问
 * ============================================================ */

/**
 * @brief 注册后台任务调度器相关的 GUC 参数
 * @return 0 成功，-1 失败
 */
int db_bgworker_config_init(void);

/**
 * @brief 获取 CCEH 缩容任务是否启用
 * @return true 启用，false 禁用
 */
bool db_bgworker_cceh_shrink_is_enabled(void);

/**
 * @brief 获取 CCEH 缩容检查间隔（毫秒）
 * @return 间隔毫秒
 */
int db_bgworker_cceh_shrink_get_interval(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_BGWORKER_INTERNAL_H */
