/**
 * @file executor.h
 * @brief Volcano 迭代器执行器框架
 *
 * 实现 PostgreSQL 风格的执行器四阶段接口：
 *   1. ExecutorStart - 启动阶段：构造 EState，初始化 PlanState 树
 *   2. ExecutorRun   - 运行阶段：调 ExecProcNode 拉取元组
 *   3. ExecutorFinish- 结束阶段：清理临时资源
 *   4. ExecutorEnd   - 释放阶段：释放 PlanState 树和 EState
 *
 * 核心数据结构：
 *   - QueryDesc     查询描述符（连接 Plan 与 EState）
 *   - DestReceiver  结果接收器（统一输出接口）
 *   - EState        执行器状态（per-query）
 *   - ExprContext   表达式求值上下文
 *   - TupleTableSlot 元组槽
 *
 * 节点调度机制：
 *   - 通过 PlanState->ExecProcNode 虚函数指针实现多态分派
 *   - 节点注册表 executor_register_node() 维护类型到初始化函数的映射
 *
 * 本文件是 SQL 执行引擎 Phase 1 基础设施层的第四个任务（Task 1.4）。
 */

#ifndef DB_SQL_EXECUTOR_H
#define DB_SQL_EXECUTOR_H

#include <stdint.h>
#include <stdbool.h>

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"
#include "db/sql/memctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Var 节点类型常量（占位常量）
 * ======================================================================== */

/** Var 节点类型常量（已在 nodetags.h 定义，此处保留用于兼容性） */
#ifndef INNER_VAR
#define INNER_VAR  1
#endif

#ifndef OUTER_VAR
#define OUTER_VAR  2
#endif

#ifndef SCAN_VAR
#define SCAN_VAR   3
#endif

/* ========================================================================
 * 扫描方向常量
 * ======================================================================== */

#ifndef ForwardScanDirection
#define ForwardScanDirection   0
#endif

#ifndef BackwardScanDirection
#define BackwardScanDirection  1
#endif

/* ========================================================================
 * 执行器状态标志
 * ======================================================================== */

/** 无标志 */
#define EXEC_FLAG_NONE              0

/** 跳过元组计数（性能优化） */
#define EXEC_FLAG_SKIP_TRIGGERS     (1 << 0)

/** WITH OIDS 模式 */
#define EXEC_FLAG_WITH_OIDS         (1 << 1)

/** 强制参数化扫描 */
#define EXEC_FLAG_FORCE_PARAM       (1 << 2)

/** 不向 DestReceiver 输出（仅收集统计） */
#define EXEC_FLAG_NO_OUTPUT         (1 << 3)

/* ========================================================================
 * ExecProcNode 函数指针类型
 * ======================================================================== */

/** ExecInitNode 函数指针：初始化节点为 PlanState */
typedef PlanState *(*ExecInitNodeFn)(Plan *plan, EState *estate, int eflags);

/** ExecEndNode 函数指针：释放节点 */
typedef void (*ExecEndNodeFn)(PlanState *node);

/** ExecReScan 函数指针：重置节点 */
typedef void (*ExecReScanFn)(PlanState *node);

/* ========================================================================
 * Plan / DestReceiver 前向声明（必须在使用前声明）
 * ======================================================================== */

/* Plan 已由 execnodes.h 前向声明为 typedef struct Plan Plan;
 * 这里是占位/兼容性 typedef，详细定义由后续任务提供。 */

/* DestReceiver 类型前置声明（详细定义见下方） */
typedef struct DestReceiver DestReceiver;

/* ========================================================================
 * QueryDesc - 查询描述符
 * ======================================================================== */

/**
 * @brief 查询描述符
 *
 * 封装一次完整查询执行所需的所有状态：
 *   - plannedstmt: 已生成的执行计划
 *   - planstate:   初始化后的 PlanState 树
 *   - estate:      执行器状态
 *   - snapshot:    MVCC 快照
 *   - direction:   扫描方向
 *   - count:       LIMIT 元组数
 *   - dest:        结果接收器
 */
typedef struct QueryDesc {
    NodeTag         type;                /**< T_QueryDesc */
    Plan           *plannedstmt;         /**< 计划树 */
    PlanState      *planstate;           /**< 运行时状态树 */
    EState         *estate;              /**< 执行器状态 */
    Snapshot        snapshot;            /**< MVCC 快照 */
    int             direction;           /**< ForwardScanDirection / BackwardScanDirection */
    uint64_t        count;               /**< LIMIT 元组数（0 表示无 LIMIT） */
    bool            doInstrument;        /**< 是否收集诊断信息 */
    DestReceiver   *dest;                /**< 结果接收器 */
} QueryDesc;

/**
 * @brief 创建 QueryDesc
 *
 * @param plannedstmt 计划树（可为 NULL）
 * @param planstate   运行时状态（可为 NULL）
 *
 * @return 新创建的 QueryDesc；失败返回 NULL
 */
QueryDesc *CreateQueryDesc(Plan *plannedstmt, PlanState *planstate);

/**
 * @brief 释放 QueryDesc
 *
 * 注意：不会释放 plannedstmt 和 planstate，需要调用方自行管理。
 *
 * @param qdesc 待释放的 QueryDesc
 */
void FreeQueryDesc(QueryDesc *qdesc);

/* ========================================================================
 * DestReceiver - 结果接收器
 * ======================================================================== */

/**
 * @brief DestReceiver 回调函数类型
 */
typedef void (*DestStartupFn)(DestReceiver *self, int eflags);
typedef void (*DestShutdownFn)(DestReceiver *self);
typedef void (*DestDestroyFn)(DestReceiver *self);

/**
 * @brief DestReceiver - 结果接收器
 *
 * 抽象基类：通过函数指针实现多态分派。
 * 当前任务仅定义结构，具体实现由后续任务提供。
 */
typedef struct DestReceiver {
    NodeTag         type;                /**< T_DestReceiver */
    void           *receiveSlot;         /**< 接收元组回调（占位） */
    DestStartupFn   rStartup;            /**< 启动回调 */
    DestShutdownFn  rShutdown;           /**< 关闭回调 */
    DestDestroyFn   rDestroy;            /**< 销毁回调 */
    void           *myDest;              /**< 子类数据 */
} DestReceiver;

/**
 * @brief 创建 DestReceiver
 *
 * 当前任务为占位实现：仅分配结构并初始化函数指针。
 *
 * @return 新创建的 DestReceiver；失败返回 NULL
 */
DestReceiver *CreateDestReceiverObj(void);

/**
 * @brief 销毁 DestReceiver
 *
 * @param self 待销毁的 DestReceiver
 */
void DestReceiverDestroy(DestReceiver *self);

/* ========================================================================
 * 执行器四阶段接口
 * ======================================================================== */

/**
 * @brief 启动阶段：构造 EState，初始化 PlanState 树
 *
 * 主要工作：
 *   1. 创建 EState
 *   2. 创建内存上下文
 *   3. 初始化 PlanState 树（递归调用 ExecInitNode）
 *
 * @param queryDesc 查询描述符
 * @param eflags    执行器标志
 */
void ExecutorStart(QueryDesc *queryDesc, int eflags);

/**
 * @brief 运行阶段：拉取元组
 *
 * 反复调用 ExecProcNode 直到达到 count 或返回 NULL。
 *
 * @param queryDesc 查询描述符
 * @param direction 扫描方向
 * @param count     最大元组数（0 表示全部）
 */
void ExecutorRun(QueryDesc *queryDesc, int direction, uint64_t count);

/**
 * @brief 结束阶段：清理临时资源
 *
 * 主要工作：
 *   - AFTER 触发器
 *   - 清理临时内存上下文
 *
 * @param queryDesc 查询描述符
 */
void ExecutorFinish(QueryDesc *queryDesc);

/**
 * @brief 释放阶段：释放 EState 和 PlanState 树
 *
 * @param queryDesc 查询描述符
 */
void ExecutorEnd(QueryDesc *queryDesc);

/* ========================================================================
 * Volcano 迭代器核心
 * ======================================================================== */

/**
 * @brief 节点合法性检查
 */
#define CheckNodeIsValid(node) \
    do { \
        if ((node) == NULL) { \
            return NULL; \
        } \
        if ((node)->ExecProcNode == NULL) { \
            return NULL; \
        } \
    } while (0)

/**
 * @brief ExecProcNode 虚函数调用
 *
 * 通过 PlanState->ExecProcNode 虚函数指针获取下一个元组。
 *
 * @param node PlanState 节点
 *
 * @return 下一个元组槽；NULL 表示无更多元组
 */
static inline TupleTableSlot *
ExecProcNode(PlanState *node) {
    if (node == NULL || node->ExecProcNode == NULL) {
        return NULL;
    }
    return node->ExecProcNode(node);
}

/**
 * @brief 初始化 PlanState 树
 *
 * 根据 Plan 节点的类型分派到对应的 ExecInit<...> 实现。
 *
 * @param plan   计划节点
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 PlanState
 */
PlanState *ExecInitNode(Plan *plan, EState *estate, int eflags);

/**
 * @brief 释放 PlanState 树
 *
 * 递归释放 PlanState 树（先子节点，再父节点）。
 *
 * @param node PlanState 节点（可为 NULL）
 */
void ExecEndNode(PlanState *node);

/**
 * @brief 重置 PlanState 节点到初始状态
 *
 * 用于重新扫描（如 NestLoop 内表回退）。
 *
 * @param node PlanState 节点
 */
void ExecReScan(PlanState *node);

/**
 * @brief 节点注册表初始化（空操作）
 *
 * 当前任务所有节点类型都尚未实现，注册表为空。
 * 后续节点实现任务会填充具体映射。
 */
void executor_register_nodes(void);

/**
 * @brief 注册节点类型的初始化函数
 *
 * @param tag     节点类型
 * @param init_fn 初始化函数
 */
void executor_register_node(NodeTag tag, ExecInitNodeFn init_fn);

/**
 * @brief 根据物理算子类型创建 Volcano 框架 PlanState
 *
 * Task 1.4 桥接函数：planner.c 由于 PlanState 命名冲突不能直接
 * 调用新框架的构造函数。该函数由 executor.c 导出，供 planner.c
 * 调用以桥接新旧框架。
 *
 * @param phys_plan_type 物理算子类型（PhysicalOpType 枚举值）
 *
 * @return 新框架 PlanState；失败返回 NULL
 */
PlanState *executor_create_plan_state_by_phys_type(int phys_plan_type);

/* ========================================================================
 * EState / ExprContext 创建与销毁
 * ======================================================================== */

/**
 * @brief 创建 EState
 *
 * 创建 EState 并初始化查询级内存上下文。
 *
 * @return 新创建的 EState；失败返回 NULL
 */
EState *CreateEState(void);

/**
 * @brief 释放 EState
 *
 * 释放 EState 及关联的内存上下文。
 *
 * @param estate EState（可为 NULL）
 */
void FreeEState(EState *estate);

/**
 * @brief 创建 ExprContext
 *
 * @param estate EState
 *
 * @return 新创建的 ExprContext；失败返回 NULL
 */
ExprContext *CreateExprContext(EState *estate);

/**
 * @brief 释放 ExprContext
 *
 * @param context   ExprContext（可为 NULL）
 * @param isCommit  是否提交（当前任务忽略此参数）
 */
void FreeExprContext(ExprContext *context, bool isCommit);

/* ========================================================================
 * TupleTableSlot 操作
 * ======================================================================== */

/**
 * @brief 创建元组槽
 *
 * 推荐传入 EState 关联的 MemoryContext，以便纳入查询级生命周期管理。
 * 若 mcxt == NULL 则回退到全局堆（适用于独立测试路径，但需调用方负责释放）。
 *
 * @param mcxt 所属内存上下文（可为 NULL，此时回退到 calloc）
 *
 * @return 新创建的 TupleTableSlot；失败返回 NULL
 */
TupleTableSlot *MakeTupleTableSlotWithMCxt(MemoryContext mcxt);

/**
 * @brief 创建元组槽（保留兼容接口：回退到堆分配）
 *
 * @return 新创建的 TupleTableSlot；失败返回 NULL
 */
TupleTableSlot *MakeTupleTableSlot(void);

/**
 * @brief 释放元组槽
 *
 * @param slot 待释放的 TupleTableSlot（可为 NULL）
 */
void FreeTupleTableSlot(TupleTableSlot *slot);

/**
 * @brief 清空元组槽
 *
 * @param slot 元组槽
 */
void ExecClearTuple(TupleTableSlot *slot);

/**
 * @brief 检查元组槽是否为 NULL
 *
 * @param slot 元组槽
 *
 * @return true 表示 NULL
 */
bool TupIsNull(TupleTableSlot *slot);

/* ========================================================================
 * 宏
 * ======================================================================== */

/** 检查元组是否为 NULL */
#define TupIsNullMacro(slot) ((slot) == NULL || (slot)->tts_nvalid == 0)

/* ========================================================================
 * planner.c 兼容性：exec_proc 字段设置辅助宏
 * ======================================================================== */

/**
 * @brief 根据节点类型设置 exec_proc 函数指针
 *
 * 由于现有 planner.c 使用 exec_proc 字段名，而新 PlanState 使用 ExecProcNode，
 * 此宏用于在初始化时桥接两个字段。
 *
 * @param state PlanState
 * @param fn    执行函数指针
 */
#define PlanStateSetExecProc(state, fn) \
    do { \
        (state)->ExecProcNode = (fn); \
        (state)->ExecProcNodeReal = (fn); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_EXECUTOR_H */