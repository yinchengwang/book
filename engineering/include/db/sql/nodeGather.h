/**
 * @file nodeGather.h
 * @brief Gather/GatherMerge 节点
 *
 * Gather 节点协调多个并行 worker 执行计划子树：
 *   - 启动并行 worker
 *   - 收集所有 worker 的结果
 *   - 合并输出到父节点
 *
 * GatherMerge 用于有序结果合并（类似归并排序）。
 */

#ifndef DB_SQL_NODEGATHER_H
#define DB_SQL_NODEGATHER_H

#include "nodes/execnodes.h"
#include "db/sql/parallel.h"
#include "db/sql/worker_pool.h"
#include "db/sql/tuple_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Gather 计划节点
 * ======================================================================== */

/**
 * @brief Gather 计划节点
 *
 * 协调多个并行 worker 执行子计划。
 */
typedef struct Gather {
    Plan        plan;               /**< 基类 */
    int         num_workers;        /**< worker 数量 */
    bool        single_copy;        /**< 是否单拷贝模式 */
    bool        invisible;          /**< 是否隐藏 */
    int         rescan_param;       /**< ReScan 参数 */
} Gather;

/* ========================================================================
 * GatherMerge 计划节点
 * ======================================================================== */

/**
 * @brief GatherMerge 计划节点
 *
 * 有序合并多个 worker 的结果。
 */
typedef struct GatherMerge {
    Plan        plan;               /**< 基类 */
    int         num_workers;        /**< worker 数量 */
    int         *collations;        /**< 排序列 */
    int         ncollations;        /**< 排序列数量 */
} GatherMerge;

/* ========================================================================
 * GatherState 执行状态
 * ======================================================================== */

/**
 * @brief GatherState 执行状态
 */
typedef struct GatherState {
    PlanState       ps;             /**< 基类 */
    int             num_workers;    /**< worker 数量 */
    bool            initialized;    /**< 是否已初始化 */
    bool            need_to_scan_locally; /**< 是否本地扫描 */
    ParallelContext *pcxt;          /**< 并行上下文 */
    TupleTableSlot  *result_slot;   /**< 结果槽 */
    int             reader;         /**< 当前读取的 worker */

    /* 真实并行执行支持 */
    WorkerPool      *worker_pool;   /**< Worker 线程池 */
    TupleQueue      *output_queue;  /**< 输出队列 */
} GatherState;

/* ========================================================================
 * GatherMergeState 执行状态
 * ======================================================================== */

/**
 * @brief GatherMergeState 执行状态
 */
typedef struct GatherMergeState {
    PlanState       ps;             /**< 基类 */
    int             num_workers;    /**< worker 数量 */
    ParallelContext *pcxt;          /**< 并行上下文 */
    void            *gm_slots;      /**< worker 结果槽 */
    int             *gm_heap;       /**< 归并堆 */
    int             gm_heap_count;  /**< 堆元素数量 */
} GatherMergeState;

/* ========================================================================
 * Gather API
 * ======================================================================== */

/**
 * @brief 初始化 Gather 节点
 *
 * @param node Gather 计划节点
 * @param estate 执行器状态
 * @param eflags 执行器标志
 * @return GatherState
 */
GatherState *ExecInitGather(Gather *node, EState *estate, int eflags);

/**
 * @brief 执行 Gather 节点
 *
 * @param node GatherState
 * @return 下一个元组槽；NULL 表示结束
 */
TupleTableSlot *ExecGather(GatherState *node);

/**
 * @brief 结束 Gather 节点
 *
 * @param node GatherState
 */
void ExecEndGather(GatherState *node);

/**
 * @brief 重置 Gather 节点
 *
 * @param node GatherState
 */
void ExecReScanGather(GatherState *node);

/* ========================================================================
 * GatherMerge API
 * ======================================================================== */

/**
 * @brief 初始化 GatherMerge 节点
 *
 * @param node GatherMerge 计划节点
 * @param estate 执行器状态
 * @param eflags 执行器标志
 * @return GatherMergeState
 */
GatherMergeState *ExecInitGatherMerge(GatherMerge *node, EState *estate, int eflags);

/**
 * @brief 执行 GatherMerge 节点
 *
 * @param node GatherMergeState
 * @return 下一个元组槽；NULL 表示结束
 */
TupleTableSlot *ExecGatherMerge(GatherMergeState *node);

/**
 * @brief 结束 GatherMerge 节点
 *
 * @param node GatherMergeState
 */
void ExecEndGatherMerge(GatherMergeState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODEGATHER_H */