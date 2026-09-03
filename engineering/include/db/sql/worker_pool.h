/**
 * @file worker_pool.h
 * @brief Worker 线程池
 *
 * 实现并行执行的工作线程池：
 *   - WorkerPoolCreate 创建指定数量的工作线程
 *   - WorkerPoolStart 启动所有工作线程执行任务
 *   - WorkerPoolWait 等待所有工作线程完成
 *   - WorkerPoolDestroy 销毁线程池
 *
 * 用于 Gather 节点并行执行子计划。
 */

#ifndef DB_SQL_WORKER_POOL_H
#define DB_SQL_WORKER_POOL_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct WorkerPool WorkerPool;
typedef struct TupleQueue TupleQueue;
typedef struct PlanState PlanState;

/* ========================================================================
 * Worker 任务函数类型
 * ======================================================================== */

/**
 * @brief Worker 任务函数
 *
 * @param worker_id  Worker ID（0 开始）
 * @param arg        用户参数
 * @param output_q   输出队列（可为 NULL）
 */
typedef void (*WorkerTaskFn)(int worker_id, void *arg, TupleQueue *output_q);

/* ========================================================================
 * WorkerTask 结构
 * ======================================================================== */

/**
 * @brief Worker 任务
 */
typedef struct WorkerTask {
    WorkerTaskFn   func;        /**< 任务函数 */
    void          *arg;         /**< 任务参数 */
    TupleQueue    *output_q;    /**< 输出队列（可为 NULL） */
} WorkerTask;

/* ========================================================================
 * WorkerPool API
 * ======================================================================== */

/**
 * @brief 创建 Worker 线程池
 *
 * @param nworkers Worker 数量（至少 1）
 * @return 新创建的 WorkerPool；失败返回 NULL
 */
WorkerPool *WorkerPoolCreate(int nworkers);

/**
 * @brief 销毁 Worker 线程池
 *
 * 等待所有 worker 完成后释放资源。
 *
 * @param pool WorkerPool（可为 NULL）
 */
void WorkerPoolDestroy(WorkerPool *pool);

/**
 * @brief 启动所有 Worker 执行任务
 *
 * 每个 worker 执行对应的 tasks[i] 任务。
 * 调用者需确保 tasks 数组长度 >= nworkers。
 *
 * @param pool  WorkerPool
 * @param tasks 任务数组（长度 >= nworkers）
 *
 * @return 成功启动的 worker 数量；失败返回 -1
 */
int WorkerPoolStart(WorkerPool *pool, WorkerTask *tasks);

/**
 * @brief 等待所有 Worker 完成
 *
 * 阻塞等待所有 worker 线程执行完毕。
 *
 * @param pool WorkerPool
 */
void WorkerPoolWait(WorkerPool *pool);

/**
 * @brief 获取 Worker 数量
 *
 * @param pool WorkerPool
 * @return Worker 数量
 */
int WorkerPoolGetNWorkers(WorkerPool *pool);

/**
 * @brief 检查线程池是否已关闭
 *
 * @param pool WorkerPool
 * @return true 表示已关闭
 */
bool WorkerPoolIsShutdown(WorkerPool *pool);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_WORKER_POOL_H */
