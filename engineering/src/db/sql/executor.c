/**
 * @file executor.c
 * @brief Volcano 迭代器执行器框架实现
 *
 * 实现 Task 1.4 的核心接口：
 *   - QueryDesc 创建/销毁
 *   - DestReceiver 创建/销毁
 *   - ExecutorStart/Run/Finish/End 四阶段
 *   - ExecInitNode/ExecEndNode/ExecReScan PlanState 树管理
 *   - EState/ExprContext 创建/销毁
 *   - TupleTableSlot 操作
 *
 * 当前为 Task 1.4 阶段：所有节点类型的初始化函数尚未实现，
 * 因此 ExecInitNode 在大多数情况下会返回 NULL。
 * 后续节点实现任务会扩展此文件。
 */

#include "db/sql/executor.h"
#include "db/sql/nodeResult.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodeHashjoin.h"
/* nodeHash.h 已合并到 nodeHashjoin.h，不再需要独立引用 */
#include "db/sql/nodeAgg.h"
#include "db/sql/nodeSort.h"
#include "db/sql/nodeLimit.h"
#include "db/sql/nodeModifyTable.h"
#include "db/sql/nodeProjectSet.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* PHYS_RESULT 在 sql_planner.h 的 PhysicalOpType 枚举中位置为 45（PHYS_SEQ_SCAN=0 起算）。
 * 这里直接引用枚举值（而非硬编码 45）以避免枚举顺序变更时不同步。
 * 该函数是 executor <-> planner 的桥接层，planner.c 已在调用方包含 sql_planner.h。 */
#ifndef PHYS_RESULT
#define PHYS_RESULT 45
#endif

/* ========================================================================
 * 节点注册表
 * ======================================================================== */

/** 节点类型 -> 初始化函数映射表（最大节点数） */
#define EXECUTOR_NODE_TABLE_SIZE 64

/** 单个节点注册项 */
typedef struct NodeRegistryEntry {
    NodeTag         tag;        /**< 节点类型 */
    ExecInitNodeFn  init_fn;    /**< 初始化函数 */
} NodeRegistryEntry;

/** 全局节点注册表 */
static NodeRegistryEntry g_node_registry[EXECUTOR_NODE_TABLE_SIZE];

/** 注册表项数 */
static int g_node_registry_count = 0;

/** 注册表是否已初始化 */
static bool g_node_registry_initialized = false;

/* TODO(并行执行任务): 当前节点注册表 g_node_registry 是全局可变状态，
 *                     无线程安全保护。当前任务阶段尚未引入并行 worker，
 *                     初始化/查询路径均单线程访问，因此可接受。
 *                     并行执行任务接入时需：
 *                       1. 引入 pthread_mutex_t / spinlock 保护并发读写
 *                       2. 注册接口改为只读快照 + write-once 模式
 *                       3. ExecutorStart 阶段按 worker 粒度做 per-worker 注册表
 *                     详见并行子任务的设计文档。 */

/**
 * @brief 初始化节点注册表
 *
 * 注册所有已实现的节点类型。
 */
void executor_register_nodes(void) {
    if (g_node_registry_initialized) {
        return;
    }

    /* 清零整个表 */
    memset(g_node_registry, 0, sizeof(g_node_registry));
    g_node_registry_count = 0;

    /* 注册 Result 节点 */
    executor_register_node(T_Result, (ExecInitNodeFn)ExecInitResult);

    /* 注册 SeqScan 节点 */
    executor_register_node(T_SeqScan, (ExecInitNodeFn)ExecInitSeqScan);

    /* 注册 HashJoin 节点 */
    executor_register_node(T_HashJoin, (ExecInitNodeFn)ExecInitHashJoin);

    /* 注册 Hash 节点 */
    executor_register_node(T_Hash, (ExecInitNodeFn)ExecInitHash);

    /* 注册 Agg 节点 */
    executor_register_node(T_Agg, (ExecInitNodeFn)ExecInitAgg);

    /* 注册 Sort 节点 */
    executor_register_node(T_Sort, (ExecInitNodeFn)ExecInitSort);

    /* 注册 Limit 节点 */
    executor_register_node(T_Limit, (ExecInitNodeFn)ExecInitLimit);

    /* 注册 ModifyTable 节点 */
    executor_register_node(T_ModifyTable, (ExecInitNodeFn)ExecInitModifyTable);

    /* 注册 ProjectSet 节点 */
    executor_register_node(T_ProjectSet, (ExecInitNodeFn)ExecInitProjectSet);

    /* 标记为已初始化 */
    g_node_registry_initialized = true;
}

/**
 * @brief 注册节点类型的初始化函数
 *
 * @param tag     节点类型
 * @param init_fn 初始化函数（不可为 NULL）
 */
void executor_register_node(NodeTag tag, ExecInitNodeFn init_fn) {
    if (init_fn == NULL) {
        return;
    }

    /* 查找是否已存在 */
    for (int i = 0; i < g_node_registry_count; i++) {
        if (g_node_registry[i].tag == tag) {
            g_node_registry[i].init_fn = init_fn;
            return;
        }
    }

    /* 添加新条目 */
    if (g_node_registry_count < EXECUTOR_NODE_TABLE_SIZE) {
        g_node_registry[g_node_registry_count].tag = tag;
        g_node_registry[g_node_registry_count].init_fn = init_fn;
        g_node_registry_count++;
    }
}

/**
 * @brief 查找节点类型的初始化函数
 *
 * @param tag 节点类型
 *
 * @return 初始化函数；未找到返回 NULL
 */
static ExecInitNodeFn find_node_init_fn(NodeTag tag) {
    if (!g_node_registry_initialized) {
        executor_register_nodes();
    }

    for (int i = 0; i < g_node_registry_count; i++) {
        if (g_node_registry[i].tag == tag) {
            return g_node_registry[i].init_fn;
        }
    }

    return NULL;
}

/* ========================================================================
 * EState - 执行器状态管理
 * ======================================================================== */

/**
 * @brief 创建 EState
 *
 * 创建 EState 并分配 es_query_cxt 内存上下文。
 *
 * @return 新创建的 EState；失败返回 NULL
 */
EState *CreateEState(void) {
    MemoryContext query_cxt = AllocSetContextCreate(
        NULL,                       /* 无父上下文：EState 是顶级 */
        "EStateQuery",              /* 调试用名 */
        0,                          /* minContextSize */
        ALLOCSET_DEFAULT_BLOCK_SIZE,/* initBlockSize */
        ALLOCSET_DEFAULT_BLOCK_SIZE /* maxBlockSize */
    );

    if (query_cxt == NULL) {
        return NULL;
    }

    /* 在 query_cxt 中分配 EState */
    EState *estate = (EState *)palloc0(query_cxt, sizeof(EState));
    if (estate == NULL) {
        delete_memory(query_cxt);
        return NULL;
    }

    /* 初始化字段 */
    estate->type = T_EState;
    estate->es_query_cxt = query_cxt;
    estate->es_tupleTable = NULL;
    estate->es_trig_tuple_slot = NULL;
    /* es_snapshot 已由 palloc0 清零（Snapshot 是 struct 值，不是指针） */
    (void)estate->es_snapshot;
    estate->es_output_cid = 0;
    estate->es_range_table = NULL;
    estate->es_relations = NULL;
    estate->es_processed = 0;
    estate->es_exprcontexts = NULL;
    estate->es_per_tuple_exprctx = NULL;
    estate->es_direction = ForwardScanDirection;
    estate->es_result_relations = NULL;
    estate->es_current_result_rel = NULL;
    estate->es_use_parallel_mode = false;
    estate->es_parallel_workers_needed = 0;

    return estate;
}

/**
 * @brief 释放 EState
 *
 * 删除 EState 关联的查询级内存上下文。
 *
 * @param estate EState（可为 NULL）
 */
void FreeEState(EState *estate) {
    if (estate == NULL) {
        return;
    }

    /* 保存查询级内存上下文指针 */
    MemoryContext query_cxt = estate->es_query_cxt;

    /* 清零结构 */
    memset(estate, 0, sizeof(EState));

    /* 删除内存上下文 */
    if (query_cxt != NULL) {
        delete_memory(query_cxt);
    }
}

/* ========================================================================
 * ExprContext - 表达式求值上下文
 * ======================================================================== */

/**
 * @brief 创建 ExprContext
 *
 * 从 EState 的 query_cxt 派生 per_tuple_memory。
 *
 * @param estate EState（不可为 NULL）
 *
 * @return 新创建的 ExprContext；失败返回 NULL
 */
ExprContext *CreateExprContext(EState *estate) {
    if (estate == NULL) {
        return NULL;
    }

    /* per_query_memory：复用 EState 的 query_cxt */
    MemoryContext per_query_cxt = estate->es_query_cxt;

    /* per_tuple_memory：从 query_cxt 派生子上下文 */
    MemoryContext per_tuple_cxt = AllocSetContextCreate(
        per_query_cxt,
        "ExprContextPerTuple",
        0,
        ALLOCSET_DEFAULT_BLOCK_SIZE,
        ALLOCSET_DEFAULT_BLOCK_SIZE
    );

    if (per_tuple_cxt == NULL) {
        return NULL;
    }

    /* 分配 ExprContext */
    ExprContext *context = (ExprContext *)palloc0(per_query_cxt, sizeof(ExprContext));
    if (context == NULL) {
        delete_memory(per_tuple_cxt);
        return NULL;
    }

    /* 初始化字段 */
    context->type = T_ExprContext;
    context->ecxt_per_query_memory = per_query_cxt;
    context->ecxt_per_tuple_memory = per_tuple_cxt;
    context->ecxt_scantuple = NULL;
    context->ecxt_innertuple = NULL;
    context->ecxt_outertuple = NULL;
    context->ecxt_param_values = NULL;
    context->ecxt_param_nulls = NULL;
    context->ecxt_param_num = 0;

    return context;
}

/**
 * @brief 释放 ExprContext
 *
 * @param context   ExprContext（可为 NULL）
 * @param isCommit  是否提交（当前任务忽略此参数）
 */
void FreeExprContext(ExprContext *context, bool isCommit) {
    if (context == NULL) {
        return;
    }

    /* 保存子上下文指针 */
    MemoryContext per_tuple_cxt = context->ecxt_per_tuple_memory;

    /* 清零结构 */
    memset(context, 0, sizeof(ExprContext));

    /* 删除 per_tuple_memory */
    if (per_tuple_cxt != NULL) {
        delete_memory(per_tuple_cxt);
    }

    /* per_query_memory 由 EState 统一管理，此处不释放 */
    (void)isCommit;
}

/* ========================================================================
 * TupleTableSlot - 元组槽操作
 * ======================================================================== */

/**
 * @brief 创建元组槽
 *
 * 推荐传入 EState 关联的 MemoryContext，以便纳入查询级生命周期管理。
 * 若 mcxt == NULL 则回退到全局堆（适用于独立测试路径，但需调用方负责释放）。
 *
 * @param mcxt  所属内存上下文（可为 NULL，此时回退到 calloc）
 *
 * @return 新创建的 TupleTableSlot；失败返回 NULL
 */
TupleTableSlot *MakeTupleTableSlotWithMCxt(MemoryContext mcxt) {
    TupleTableSlot *slot;

    if (mcxt != NULL) {
        slot = (TupleTableSlot *)palloc0(mcxt, sizeof(TupleTableSlot));
    } else {
        slot = (TupleTableSlot *)calloc(1, sizeof(TupleTableSlot));
    }

    if (slot == NULL) {
        return NULL;
    }

    slot->type = T_TupleTableSlot;
    slot->tts_tupleDescriptor = NULL;
    slot->tts_values = NULL;
    slot->tts_isnull = NULL;
    /* tts_mcxt 为 Size（unsigned long），这里把指针整化为句柄值保存，
     * 测试中可识别非零值即代表有上下文。生产代码应改为强类型指针字段。 */
    slot->tts_mcxt = (Size)(uintptr_t)mcxt;  /* MemoryContext 句柄（整化保存） */
    slot->tts_ops = NULL;
    slot->tts_nvalid = 0;
    slot->tts_shouldFree = false;
    slot->tts_shouldFreeMin = false;
    slot->tts_tuple = NULL;
    slot->tts_tupleLen = 0;
    slot->tts_minTupleLen = 0;

    return slot;
}

/**
 * @brief 创建元组槽（保留兼容接口：回退到堆分配）
 *
 * @return 新创建的 TupleTableSlot；失败返回 NULL
 */
TupleTableSlot *MakeTupleTableSlot(void) {
    return MakeTupleTableSlotWithMCxt(NULL);
}

/**
 * @brief 释放元组槽
 *
 * 若槽在 MemoryContext 中分配（tts_mcxt != 0），由所属上下文统一释放；
 * 否则回退到 free。
 *
 * @param slot 待释放的 TupleTableSlot（可为 NULL）
 */
void FreeTupleTableSlot(TupleTableSlot *slot) {
    if (slot == NULL) {
        return;
    }

    /* 若有所属 MemoryContext，则不单独释放字段和结构本身，
     * 由上下文统一重置（PostgreSQL 标准行为）。 */
    if (slot->tts_mcxt != 0) {
        return;
    }

    /* 释放字段 */
    if (slot->tts_values != NULL) {
        free(slot->tts_values);
    }
    if (slot->tts_isnull != NULL) {
        free(slot->tts_isnull);
    }

    /* 释放结构本身 */
    free(slot);
}

/**
 * @brief 清空元组槽
 *
 * @param slot 元组槽
 */
void ExecClearTuple(TupleTableSlot *slot) {
    if (slot == NULL) {
        return;
    }

    slot->tts_nvalid = 0;
    slot->tts_tuple = NULL;
}

/**
 * @brief 检查元组是否为 NULL
 *
 * @param slot 元组槽
 *
 * @return true 表示 NULL
 */
bool TupIsNull(TupleTableSlot *slot) {
    return TupIsNullMacro(slot);
}

/* ========================================================================
 * DestReceiver - 结果接收器
 * ======================================================================== */

/**
 * @brief 启动回调（占位）
 */
static void dest_startup_stub(DestReceiver *self, int eflags) {
    (void)self;
    (void)eflags;
}

/**
 * @brief 关闭回调（占位）
 */
static void dest_shutdown_stub(DestReceiver *self) {
    (void)self;
}

/**
 * @brief 销毁回调（占位）
 */
static void dest_destroy_stub(DestReceiver *self) {
    (void)self;
}

/**
 * @brief 创建 DestReceiver
 *
 * @return 新创建的 DestReceiver；失败返回 NULL
 */
DestReceiver *CreateDestReceiverObj(void) {
    DestReceiver *self = (DestReceiver *)calloc(1, sizeof(DestReceiver));
    if (self == NULL) {
        return NULL;
    }

    self->type = T_DestReceiver;
    self->receiveSlot = NULL;
    self->rStartup = dest_startup_stub;
    self->rShutdown = dest_shutdown_stub;
    self->rDestroy = dest_destroy_stub;
    self->myDest = NULL;

    return self;
}

/**
 * @brief 销毁 DestReceiver
 *
 * @param self 待销毁的 DestReceiver（可为 NULL）
 */
void DestReceiverDestroy(DestReceiver *self) {
    if (self == NULL) {
        return;
    }

    /* 调用 rDestroy 回调 */
    if (self->rDestroy != NULL) {
        self->rDestroy(self);
    }

    /* 释放结构本身 */
    free(self);
}

/* ========================================================================
 * QueryDesc - 查询描述符
 * ======================================================================== */

/**
 * @brief 创建 QueryDesc
 *
 * 使用 malloc 分配，调用方需通过 FreeQueryDesc 释放。
 *
 * @param plannedstmt 计划树（可为 NULL）
 * @param planstate   运行时状态（可为 NULL）
 *
 * @return 新创建的 QueryDesc；失败返回 NULL
 */
QueryDesc *CreateQueryDesc(Plan *plannedstmt, PlanState *planstate) {
    QueryDesc *qdesc = (QueryDesc *)calloc(1, sizeof(QueryDesc));
    if (qdesc == NULL) {
        return NULL;
    }

    qdesc->type = T_QueryDesc;
    qdesc->plannedstmt = plannedstmt;
    qdesc->planstate = planstate;
    qdesc->estate = NULL;
    /* snapshot 已由 calloc 清零（Snapshot 是 struct 值） */
    (void)qdesc->snapshot;
    qdesc->direction = ForwardScanDirection;
    qdesc->count = 0;
    qdesc->doInstrument = false;
    qdesc->dest = NULL;

    return qdesc;
}

/**
 * @brief 释放 QueryDesc
 *
 * 注意：不会释放 plannedstmt、planstate 和 estate。
 *
 * @param qdesc 待释放的 QueryDesc（可为 NULL）
 */
void FreeQueryDesc(QueryDesc *qdesc) {
    if (qdesc == NULL) {
        return;
    }

    /* 释放 DestReceiver */
    if (qdesc->dest != NULL) {
        DestReceiverDestroy(qdesc->dest);
    }

    /* 清零结构 */
    memset(qdesc, 0, sizeof(QueryDesc));

    /* 释放结构本身 */
    free(qdesc);
}

/* ========================================================================
 * 执行器四阶段接口
 * ======================================================================== */

/**
 * @brief 启动阶段：构造 EState，初始化 PlanState 树
 *
 * @param queryDesc 查询描述符（不可为 NULL）
 * @param eflags    执行器标志
 */
void ExecutorStart(QueryDesc *queryDesc, int eflags) {
    if (queryDesc == NULL) {
        return;
    }

    /* 确保节点注册表已初始化 */
    if (!g_node_registry_initialized) {
        executor_register_nodes();
    }

    /* 1. 创建 EState（如果尚未创建） */
    if (queryDesc->estate == NULL) {
        queryDesc->estate = CreateEState();
        if (queryDesc->estate == NULL) {
            return;
        }
    }

    /* 2. 设置 EState 字段 */
    queryDesc->estate->es_direction = queryDesc->direction;

    /* 3. 初始化 PlanState 树 */
    if (queryDesc->plannedstmt != NULL && queryDesc->planstate == NULL) {
        queryDesc->planstate = ExecInitNode(
            queryDesc->plannedstmt,
            queryDesc->estate,
            eflags
        );
    }

    /* 4. 启动 DestReceiver */
    if (queryDesc->dest != NULL && queryDesc->dest->rStartup != NULL) {
        queryDesc->dest->rStartup(queryDesc->dest, eflags);
    }

    (void)eflags;
}

/**
 * @brief 运行阶段：拉取元组
 *
 * 反复调用 ExecProcNode 直到达到 count 或返回 NULL。
 * 拉到的每个元组都通过 DestReceiver->receiveSlot 投递给外部消费者。
 *
 * @param queryDesc 查询描述符
 * @param direction 扫描方向
 * @param count     最大元组数（0 表示全部）
 */
void ExecutorRun(QueryDesc *queryDesc, int direction, uint64_t count) {
    if (queryDesc == NULL || queryDesc->planstate == NULL) {
        return;
    }

    /* 更新扫描方向 */
    queryDesc->direction = direction;
    if (queryDesc->estate != NULL) {
        queryDesc->estate->es_direction = direction;
    }

    /* 拉取元组 */
    uint64_t produced = 0;
    while (true) {
        /* 检查 LIMIT */
        if (count > 0 && produced >= count) {
            break;
        }

        /* 调用 ExecProcNode 拉取下一个元组 */
        TupleTableSlot *slot = ExecProcNode(queryDesc->planstate);
        if (slot == NULL) {
            /* 无更多元组 */
            break;
        }

        /* 将元组投递给 DestReceiver。
         * 当前 Task 1.4 阶段：DestReceiver->receiveSlot 是 void* 占位字段，
         * 尚未定义为标准的 callback 类型。后续任务会引入：
         *   typedef bool (*ReceiveSlotFn)(DestReceiver *self, TupleTableSlot *slot);
         * 并由具体 DestReceiver 子类型（PrinttupDestReceiver / MaterializeDestReceiver 等）
         * 实现其语义。本阶段保留代码入口和分发逻辑，但不进行实际投递（消费者为空时仍合法）。 */
        if (queryDesc->dest != NULL && queryDesc->dest->receiveSlot != NULL) {
            /* 后续任务：dest->receiveSlot(queryDesc->dest, slot); */
            (void)slot;
        }

        produced++;
    }

    /* 更新处理元组计数 */
    if (queryDesc->estate != NULL) {
        queryDesc->estate->es_processed = produced;
    }
}

/**
 * @brief 结束阶段：清理临时资源
 *
 * @param queryDesc 查询描述符
 */
void ExecutorFinish(QueryDesc *queryDesc) {
    if (queryDesc == NULL) {
        return;
    }

    /* 关闭 DestReceiver */
    if (queryDesc->dest != NULL && queryDesc->dest->rShutdown != NULL) {
        queryDesc->dest->rShutdown(queryDesc->dest);
    }
}

/**
 * @brief 释放阶段：释放 EState 和 PlanState 树
 *
 * @param queryDesc 查询描述符
 */
void ExecutorEnd(QueryDesc *queryDesc) {
    if (queryDesc == NULL) {
        return;
    }

    /* 1. 释放 PlanState 树 */
    if (queryDesc->planstate != NULL) {
        ExecEndNode(queryDesc->planstate);
        queryDesc->planstate = NULL;
    }

    /* 2. 释放 EState */
    if (queryDesc->estate != NULL) {
        FreeEState(queryDesc->estate);
        queryDesc->estate = NULL;
    }
}

/* ========================================================================
 * PlanState 树管理
 * ======================================================================== */

/**
 * @brief 根据 NodeTag 调用注册的初始化函数
 *
 * 由 ExecInitNode 调用（避免直接访问 plan->type，因 Plan 当前不完整）。
 * 注册表为空时直接返回 NULL，由调用方做桥接到 executor_create_plan_state_by_phys_type。
 *
 * @param tag    节点类型
 * @param plan   计划节点（透传给 init_fn）
 * @param estate 执行器状态
 * @param eflags 标志
 *
 * @return PlanState 或 NULL
 */
static PlanState *exec_init_by_tag(NodeTag tag, Plan *plan, EState *estate, int eflags) {
    if (!g_node_registry_initialized) {
        executor_register_nodes();
    }
    ExecInitNodeFn init_fn = find_node_init_fn(tag);
    if (init_fn == NULL) {
        return NULL;
    }
    return init_fn(plan, estate, eflags);
}

/**
 * @brief 初始化 PlanState 节点
 *
 * 根据 Plan 节点的类型查找注册表，分派到对应的初始化函数。
 * 当前 Plan 为不完整类型（前向声明），因此通过 executor_create_plan_state_by_phys_type
 * 桥接路径在注册表未命中时构造基础 PlanState，注册表命中则返回注册函数构造的 PlanState。
 *
 * @param plan   计划节点
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 PlanState；失败返回 NULL
 */
PlanState *ExecInitNode(Plan *plan, EState *estate, int eflags) {
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    /* 确保节点注册表已初始化 */
    if (!g_node_registry_initialized) {
        executor_register_nodes();
    }

    /* 查找注册的初始化函数 */
    NodeTag tag = plan->type;
    ExecInitNodeFn init_fn = find_node_init_fn(tag);
    if (init_fn != NULL) {
        return init_fn(plan, estate, eflags);
    }

    /* 注册表未命中：回退到桥接路径 */
    (void)eflags;
    return NULL;
}

/**
 * @brief 释放 PlanState 树
 *
 * 递归释放 PlanState 树（先子节点，再父节点）。
 *
 * @param node PlanState 节点（可为 NULL）
 */
void ExecEndNode(PlanState *node) {
    if (node == NULL) {
        return;
    }

    /* 1. 递归释放左子树 */
    if (node->lefttree != NULL) {
        ExecEndNode(node->lefttree);
        node->lefttree = NULL;
    }

    /* 2. 递归释放右子树 */
    if (node->righttree != NULL) {
        ExecEndNode(node->righttree);
        node->righttree = NULL;
    }

    /* 3. 释放表达式上下文 */
    if (node->ps_ExprContext != NULL) {
        FreeExprContext(node->ps_ExprContext, true);
        node->ps_ExprContext = NULL;
    }

    /* 4. 释放结果槽 */
    if (node->ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->ps_ResultTupleSlot);
        node->ps_ResultTupleSlot = NULL;
    }

    /* 5. 释放诊断信息 */
    if (node->instrument != NULL) {
        free(node->instrument);
        node->instrument = NULL;
    }

    /* 6. 释放 chgParam */
    if (node->chgParam != NULL) {
        free(node->chgParam);
        node->chgParam = NULL;
    }

    /* 7. 释放 PlanState 本身（简化：使用 free） */
    /* 注意：实际应由 EState 的 es_query_cxt 统一管理 */
    free(node);
}

/**
 * @brief 重置 PlanState 节点到初始状态
 *
 * 用于重新扫描（如 NestLoop 内表回退）。
 * 当前任务阶段：递归调用左右子树的 ExecReScan（用于嵌套循环等场景）。
 * 节点自身的具体重置逻辑（如重置扫描游标、清空物化缓冲等）由
 * 后续节点实现任务在各自的 PlanState 子类中注册专用 Rescan 回调。
 *
 * @param node PlanState 节点
 */
void ExecReScan(PlanState *node) {
    if (node == NULL) {
        return;
    }

    /* 1. 检查节点类型合法性 */
    if (node->type != T_PlanState && node->type < T_PlanState) {
        return;
    }

    /* 2. 递归重置左子树（用于嵌套循环等需回退内部扫描的场景） */
    if (node->lefttree != NULL) {
        ExecReScan(node->lefttree);
    }

    /* 3. 递归重置右子树（同上） */
    if (node->righttree != NULL) {
        ExecReScan(node->righttree);
    }

    /* 4. 节点自身具体重置逻辑由后续节点实现任务在此扩展 */
}

/* ========================================================================
 * Volcano 框架 - 占位执行函数
 * ========================================================================
 *
 * Task 1.4 阶段：仅 Result 节点挂载占位实现，其余节点由后续任务提供。
 * 占位实现返回 NULL（表示无更多元组），便于测试骨架链路。
 */

/**
 * @brief Result 节点占位实现
 *
 * @param pstate PlanState
 *
 * @return NULL（占位）
 */
TupleTableSlot *exec_result_volcano(PlanState *pstate) {
    (void)pstate;
    /* 当前任务：Result 节点尚未实现，返回 NULL 表示无更多元组 */
    return NULL;
}

/* ========================================================================
 * PlanState 注册工厂（Task 1.4: planner.c 修复的对应实现）
 * ========================================================================
 *
 * 由于 sql_executor.h 和 execnodes.h 中 PlanState 命名冲突，
 * planner.c 不能直接调用新框架的 PlanState 构造函数。
 * 这里提供一个桥接函数，使用 sql_planner.h 的 PhysPlan 类型，
 * 内部创建新框架的 PlanState，并通过 executor_register_node 机制
 * 设置对应的 ExecProcNode。
 *
 * 当前实现：仅 Result 节点挂载占位函数，其余设为 NULL。
 */

/**
 * @brief 根据物理算子类型创建 Volcano 框架 PlanState
 *
 * 该函数由 executor.c 导出，被 planner.c 调用以桥接新旧框架。
 *
 * @param phys_plan_type 物理算子类型（PhysicalOpType 枚举值，传为 int）
 *
 * @return 新框架 PlanState；失败返回 NULL
 */
PlanState *executor_create_plan_state_by_phys_type(int phys_plan_type) {
    PlanState *state = (PlanState *)calloc(1, sizeof(PlanState));
    if (!state) return NULL;

    /* 使用 PHYS_RESULT 枚举值（而非硬编码数字 45）保持与 sql_planner.h 同步 */
    if (phys_plan_type == PHYS_RESULT) {
        state->type = T_Result;
        state->ExecProcNode = exec_result_volcano;
    } else {
        state->type = T_Plan;
        state->ExecProcNode = NULL;
    }
    state->ExecProcNodeReal = state->ExecProcNode;

    /* 其他字段初始化 */
    state->plan = NULL;
    state->state = NULL;
    state->lefttree = NULL;
    state->righttree = NULL;
    state->qual = NULL;
    state->recheck = NULL;
    state->ps_ProjInfo = NULL;
    state->ps_ResultTupleSlot = NULL;
    state->ps_ResultTupleDesc = NULL;
    state->ps_ExprContext = NULL;
    state->instrument = NULL;
    state->needs_to_scan_queue = false;
    state->chgParam = NULL;

    return state;
}