/**
 * @file nodeGather.c
 * @brief Gather/GatherMerge 节点实现
 */

#include "db/sql/nodeGather.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
#include "db/sql/worker_pool.h"
#include "db/sql/tuple_queue.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Worker 函数类型
 * ======================================================================== */

/**
 * @brief Gather Worker 任务参数
 */
typedef struct GatherWorkerArg {
    PlanState   *planstate;     /**< 子计划状态 */
    TupleQueue  *output_q;      /**< 输出队列 */
} GatherWorkerArg;

/* ========================================================================
 * Worker 线程函数
 * ======================================================================== */

/**
 * @brief Gather Worker 线程入口函数
 *
 * 反复执行子计划，将结果发送到输出队列。
 */
static void gather_worker_main(int worker_id, void *arg, TupleQueue *output_q) {
    GatherWorkerArg *warg = (GatherWorkerArg *)arg;
    TupleTableSlot *slot;

    (void)worker_id;  /* 暂时未使用 */

    if (!warg || !warg->planstate) {
        return;
    }

    /* 反复执行子计划 */
    for (;;) {
        slot = ExecProcNode(warg->planstate);
        if (TupIsNull(slot)) {
            break;
        }

        /* 发送到输出队列 */
        if (TupleQueueSend(output_q, slot) != 0) {
            /* 队列已关闭 */
            break;
        }
    }
}

/* ========================================================================
 * Gather 节点实现
 * ======================================================================== */

GatherState *ExecInitGather(Gather *node, EState *estate, int eflags) {
    GatherState *state;

    /* 分配状态结构 */
    state = (GatherState *)palloc0(estate->es_query_cxt, sizeof(GatherState));
    if (!state) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_GatherState;
    state->ps.plan = (Plan *)node;
    state->ps.state = estate;
    PlanStateSetExecProc(&state->ps, (ExecProcNodeMtd)ExecGather);

    /* 初始化字段 */
    state->num_workers = node->num_workers;
    state->initialized = false;
    state->need_to_scan_locally = false;
    state->reader = 0;

    /* 创建并行上下文 */
    state->pcxt = CreateParallelContext(node->num_workers);

    /* 初始化子计划 */
    if (node->plan.lefttree) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
    }

    /* 创建结果槽 */
    state->result_slot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);
    state->ps.ps_ResultTupleSlot = state->result_slot;
    state->ps.ps_ExprContext = estate->es_per_tuple_exprctx;

    /* 创建 Worker 线程池 */
    state->worker_pool = WorkerPoolCreate(node->num_workers);
    if (!state->worker_pool) {
        /* 线程池创建失败，回退到串行模式 */
        state->num_workers = 0;
    }

    /* 创建输出队列 */
    state->output_queue = TupleQueueCreate(1024);
    if (!state->output_queue) {
        /* 队列创建失败，回退到串行模式 */
        WorkerPoolDestroy(state->worker_pool);
        state->worker_pool = NULL;
        state->num_workers = 0;
    }

    return state;
}

/**
 * @brief Gather 真实并行初始化
 *
 * 启动 worker 线程池执行子计划。
 */
static void gather_init_real(GatherState *state) {
    WorkerTask *tasks;
    GatherWorkerArg *args;
    int i;

    if (!state->worker_pool || state->num_workers <= 0) {
        return;
    }

    /* 分配任务数组 */
    tasks = (WorkerTask *)calloc(state->num_workers, sizeof(WorkerTask));
    if (!tasks) {
        return;
    }

    /* 分配参数数组 */
    args = (GatherWorkerArg *)calloc(state->num_workers, sizeof(GatherWorkerArg));
    if (!args) {
        free(tasks);
        return;
    }

    /* 初始化任务参数（所有 worker 共享同一个子计划） */
    for (i = 0; i < state->num_workers; i++) {
        args[i].planstate = state->ps.lefttree;
        args[i].output_q = state->output_queue;

        tasks[i].func = gather_worker_main;
        tasks[i].arg = &args[i];
        tasks[i].output_q = state->output_queue;
    }

    /* 启动 worker */
    if (WorkerPoolStart(state->worker_pool, tasks) <= 0) {
        /* 启动失败，回退到串行模式 */
        free(tasks);
        free(args);
        return;
    }

    state->need_to_scan_locally = false;

    free(tasks);
    free(args);
}

/**
 * @brief Gather 懒初始化
 *
 * 在首次执行时启动并行 worker。
 */
static void gather_init(GatherState *state) {
    if (state->initialized) {
        return;
    }

    /* 启动并行 worker */
    if (state->pcxt) {
        LaunchParallelWorkers(state->pcxt);
    }

    /* 真实并行初始化 */
    if (state->worker_pool && state->num_workers > 0) {
        gather_init_real(state);
    }

    state->initialized = true;
}

/**
 * @brief 从输出队列获取下一个元组
 */
static TupleTableSlot *gather_getnext_parallel(GatherState *state) {
    TupleTableSlot *slot;

    if (!state->output_queue) {
        return NULL;
    }

    /* 从队列接收元组 */
    slot = (TupleTableSlot *)TupleQueueReceive(state->output_queue);
    return slot;
}

/**
 * @brief 获取下一个元组（串行模式）
 */
static TupleTableSlot *gather_getnext(GatherState *state) {
    TupleTableSlot *slot;

    /* 从子计划获取元组 */
    if (state->ps.lefttree) {
        slot = ExecProcNode(state->ps.lefttree);
        if (!TupIsNull(slot)) {
            return slot;
        }
    }

    /* 所有 worker 完成 */
    return NULL;
}

TupleTableSlot *ExecGather(GatherState *node) {
    if (!node) {
        return NULL;
    }

    /* 懒初始化 */
    if (!node->initialized) {
        gather_init(node);
    }

    /* 真实并行模式 */
    if (node->worker_pool && node->num_workers > 0) {
        return gather_getnext_parallel(node);
    }

    /* 串行模式 */
    return gather_getnext(node);
}

void ExecEndGather(GatherState *node) {
    if (!node) {
        return;
    }

    /* 等待 worker 完成 */
    if (node->worker_pool) {
        WorkerPoolWait(node->worker_pool);
        WorkerPoolDestroy(node->worker_pool);
        node->worker_pool = NULL;
    }

    /* 销毁输出队列 */
    if (node->output_queue) {
        TupleQueueDestroy(node->output_queue);
        node->output_queue = NULL;
    }

    /* 结束子计划 */
    if (node->ps.lefttree) {
        ExecEndNode(node->ps.lefttree);
    }

    /* 释放并行上下文 */
    if (node->pcxt) {
        DestroyParallelContext(node->pcxt);
    }

    /* 释放结果槽 */
    if (node->ps.ps_ResultTupleSlot) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
    }
}

void ExecReScanGather(GatherState *node) {
    if (!node) {
        return;
    }

    /* 重置状态 */
    node->initialized = false;
    node->reader = 0;

    /* 重置子计划 */
    if (node->ps.lefttree) {
        ExecReScan(node->ps.lefttree);
    }
}

/* ========================================================================
 * GatherMerge 节点实现
 * ======================================================================== */

GatherMergeState *ExecInitGatherMerge(GatherMerge *node, EState *estate, int eflags) {
    GatherMergeState *state;

    state = (GatherMergeState *)palloc0(estate->es_query_cxt, sizeof(GatherMergeState));
    if (!state) {
        return NULL;
    }

    state->ps.type = T_GatherMergeState;
    state->ps.plan = (Plan *)node;
    state->ps.state = estate;
    PlanStateSetExecProc(&state->ps, (ExecProcNodeMtd)ExecGatherMerge);

    state->num_workers = node->num_workers;
    state->pcxt = CreateParallelContext(node->num_workers);
    state->gm_slots = NULL;
    state->gm_heap = NULL;
    state->gm_heap_count = 0;

    /* 初始化子计划 */
    if (node->plan.lefttree) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
    }

    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);

    return state;
}

TupleTableSlot *ExecGatherMerge(GatherMergeState *node) {
    /* 简化实现：委托给子计划 */
    if (!node) {
        return NULL;
    }

    if (node->ps.lefttree) {
        return ExecProcNode(node->ps.lefttree);
    }

    return NULL;
}

void ExecEndGatherMerge(GatherMergeState *node) {
    if (!node) {
        return;
    }

    if (node->ps.lefttree) {
        ExecEndNode(node->ps.lefttree);
    }

    if (node->pcxt) {
        DestroyParallelContext(node->pcxt);
    }

    if (node->ps.ps_ResultTupleSlot) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
    }
}