/**
 * @file merge_scheduler.h
 * @brief 后台合并调度器 - 管理增量缓冲区到主索引的异步合并
 *
 * 设计原则：
 * 1. 后台线程：合并操作在独立线程中执行，不阻塞插入
 * 2. 任务队列：合并任务按 FIFO 顺序执行
 * 3. 触发策略：支持定时触发和阈值触发
 * 4. 进度回调：支持合并进度的实时回调
 */

#ifndef MERGE_SCHEDULER_H
#define MERGE_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct merge_scheduler merge_scheduler_t;
typedef struct vector_index vector_index_t;

/* ========================================================================
 * 合并任务
 * ======================================================================== */

/**
 * @brief 合并任务类型
 */
typedef enum merge_task_type {
    MERGE_TASK_FLUSH,       /* 刷新缓冲区到主索引 */
    MERGE_TASK_REBUILD,     /* 重建索引 */
    MERGE_TASK_OPTIMIZE     /* 图优化 */
} merge_task_type_t;

/**
 * @brief 合并任务
 */
typedef struct merge_task {
    merge_task_type_t type;
    void *data;             /* 任务数据 */
    int32_t vector_count;   /* 要合并的向量数 */
} merge_task_t;

/* ========================================================================
 * 合并配置
 * ======================================================================== */

/**
 * @brief 合并调度配置
 */
typedef struct merge_scheduler_config {
    int32_t flush_interval_ms;    /* 定时刷新间隔（毫秒） */
    int32_t max_pending_tasks;    /* 最大待处理任务数 */
    int32_t merge_batch_size;     /* 每次合并的向量数 */
    bool auto_flush;              /* 是否自动刷新 */
} merge_scheduler_config_t;

/**
 * @brief 默认配置
 */
#define MERGE_SCHEDULER_DEFAULT_INTERVAL_MS 1000
#define MERGE_SCHEDULER_DEFAULT_MAX_PENDING 10
#define MERGE_SCHEDULER_DEFAULT_BATCH_SIZE 1000

/**
 * @brief 创建默认配置
 */
merge_scheduler_config_t merge_scheduler_config_default(void);

/* ========================================================================
 * 进度回调
 * ======================================================================== */

/**
 * @brief 合并进度回调函数类型
 * @param processed 已处理向量数
 * @param total 总向量数
 * @param task_type 任务类型
 * @param ctx 用户上下文
 */
typedef void (*merge_progress_callback_t)(
    int32_t processed,
    int32_t total,
    merge_task_type_t task_type,
    void *ctx);

/* ========================================================================
 * 调度器操作
 * ======================================================================== */

/**
 * @brief 创建合并调度器
 * @param index 主索引（合并目标）
 * @param config 配置（NULL 使用默认配置）
 * @return 调度器句柄，失败返回 NULL
 */
merge_scheduler_t *merge_scheduler_create(
    vector_index_t *index,
    const merge_scheduler_config_t *config);

/**
 * @brief 销毁合并调度器
 *
 * 会等待后台线程完成当前任务。
 *
 * @param sched 调度器句柄
 */
void merge_scheduler_destroy(merge_scheduler_t *sched);

/**
 * @brief 启动后台线程
 * @param sched 调度器句柄
 * @return 0 成功，-1 失败
 */
int32_t merge_scheduler_start(merge_scheduler_t *sched);

/**
 * @brief 停止后台线程
 *
 * 会等待当前任务完成。
 *
 * @param sched 调度器句柄
 * @return 0 成功，-1 失败
 */
int32_t merge_scheduler_stop(merge_scheduler_t *sched);

/**
 * @brief 触发合并
 *
 * 将合并任务加入队列，后台线程会自动执行。
 *
 * @param sched 调度器句柄
 * @param task 合并任务
 * @return 0 成功加入队列，-1 失败
 */
int32_t merge_scheduler_trigger(merge_scheduler_t *sched, const merge_task_t *task);

/**
 * @brief 刷新缓冲区并合并
 *
 * 便捷函数：将缓冲区刷新并触发合并。
 *
 * @param sched 调度器句柄
 * @param buffer 缓冲区
 * @param vectors 缓冲区数据（预分配）
 * @param max_n 最大向量数
 * @return 0 成功，-1 失败
 */
int32_t merge_scheduler_flush(merge_scheduler_t *sched,
                               void *buffer,
                               float *vectors,
                               int32_t max_n);

/**
 * @brief 等待所有任务完成
 * @param sched 调度器句柄
 * @return 0 成功，-1 失败或超时
 */
int32_t merge_scheduler_wait(merge_scheduler_t *sched);

/**
 * @brief 取消所有待处理任务
 * @param sched 调度器句柄
 */
void merge_scheduler_cancel_pending(merge_scheduler_t *sched);

/* ========================================================================
 * 配置更新
 * ======================================================================== */

/**
 * @brief 设置刷新间隔
 * @param sched 调度器句柄
 * @param interval_ms 间隔（毫秒）
 * @return 0 成功，-1 失败
 */
int32_t merge_scheduler_set_interval(merge_scheduler_t *sched, int32_t interval_ms);

/**
 * @brief 设置进度回调
 * @param sched 调度器句柄
 * @param callback 回调函数
 * @param ctx 用户上下文
 */
void merge_scheduler_set_callback(merge_scheduler_t *sched,
                                   merge_progress_callback_t callback,
                                   void *ctx);

/* ========================================================================
 * 状态查询
 * ======================================================================== */

/**
 * @brief 合并调度器状态
 */
typedef struct merge_scheduler_stats {
    bool running;               /* 后台线程是否运行 */
    int32_t pending_tasks;      /* 待处理任务数 */
    int32_t completed_tasks;    /* 已完成任务数 */
    int64_t total_vectors_merged; /* 累计合并向量数 */
    int32_t current_task_type;  /* 当前任务类型 */
    int32_t current_progress;   /* 当前进度 */
    int32_t current_total;      /* 当前总数 */
} merge_scheduler_stats_t;

/**
 * @brief 获取调度器状态
 * @param sched 调度器句柄
 * @param stats 输出状态
 */
void merge_scheduler_get_stats(const merge_scheduler_t *sched,
                                merge_scheduler_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* MERGE_SCHEDULER_H */
