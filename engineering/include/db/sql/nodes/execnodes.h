/**
 * @file execnodes.h
 * @brief 执行器节点数据结构定义
 *
 * 定义火山模型（Volcano-style）执行器所需的核心数据结构：
 *   - PlanState:        计划节点运行时状态基类
 *   - EState:           每个执行的执行器状态
 *   - ExprContext:      表达式求值上下文
 *   - TupleTableSlot:   元组槽（接口）
 *   - Bitmapset:        简化位图集合（参数表达式追踪等）
 *   - Instrumentation:  节点级性能/诊断信息
 *   - TupleDescData:    元组描述符
 *   - ProjectionInfo:   投影表达式求值信息
 *   - TupleTableSlotOps: 槽操作虚表
 *
 * 本文件只定义数据结构（不做任何内存分配与初始化），
 * 真正的创建/销毁/初始化逻辑在后续 Executor 子任务中提供。
 *
 * 设计准则：
 *   - 每个结构体首字段都是 NodeTag type，便于运行时类型分发
 *   - 使用前向 struct 声明避免循环依赖
 *   - 与 Task 1.1 的 MemoryContext 集成：所有长期存活的指针放在 EState/PlanState 中
 *   - 与 nodetags.h 中的 NodeTag 完整对应（包括所有 Plan/State/Expression 标签）
 */

#ifndef DB_SQL_NODES_EXECNODES_H
#define DB_SQL_NODES_EXECNODES_H

#include <stdint.h>
#include <stdbool.h>

#include "db/sql/nodes/nodetags.h"
#include "db/sql/memctx.h"
#include "db/sql/sql_types.h"  /* 共享类型定义 */

/* ========================================================================
 * TupleTableSlot 值设置宏
 * ======================================================================== */

/**
 * @brief 设置 TupleTableSlot 中的整数值
 * @param slot 元组槽
 * @param attno 属性号（从 0 开始）
 * @param value 整数值
 *
 * 用法示例：
 *   TTS_VALUES_SET_INT(slot, 0, 42);
 *   TTS_VALUES_SET_INT(slot, 1, count);
 */
#define TTS_VALUES_SET_INT(slot, attno, value) \
    do { \
        (slot)->tts_values[(attno)] = (void *)(uintptr_t)(uint64_t)(value); \
        (slot)->tts_isnull[(attno)] = false; \
    } while (0)

/**
 * @brief 设置 TupleTableSlot 中的 64 位整数值
 * @param slot 元组槽
 * @param attno 属性号
 * @param value int64_t 值
 */
#define TTS_VALUES_SET_INT64(slot, attno, value) \
    do { \
        (slot)->tts_values[(attno)] = (void *)(uintptr_t)(uint64_t)(value); \
        (slot)->tts_isnull[(attno)] = false; \
    } while (0)

/**
 * @brief 设置 TupleTableSlot 中的指针值
 * @param slot 元组槽
 * @param attno 属性号
 * @param ptr 指针值
 */
#define TTS_VALUES_SET_PTR(slot, attno, ptr) \
    do { \
        (slot)->tts_values[(attno)] = (void *)(ptr); \
        (slot)->tts_isnull[(attno)] = false; \
    } while (0)

/**
 * @brief 设置 TupleTableSlot 中的 double 值（使用位拷贝）
 * @param slot 元组槽
 * @param attno 属性号
 * @param value double 值
 *
 * 注意：使用位拷贝将 double 的 IEEE 754 表示存储为 uint64_t，
 * 这是必须的，因为 Datum 是指针大小的整数类型
 *
 * 用法示例：
 *   double dist = 1.5;
 *   TTS_VALUES_SET_DOUBLE(slot, 1, dist);
 */
#define TTS_VALUES_SET_DOUBLE(slot, attno, value) \
    do { \
        union { double d; uint64_t u; } _u; \
        _u.d = (value); \
        (slot)->tts_values[(attno)] = (void *)(uintptr_t)_u.u; \
        (slot)->tts_isnull[(attno)] = false; \
    } while (0)

/**
 * @brief 从 TupleTableSlot 提取 double 值
 * @param slot 元组槽
 * @param attno 属性号
 * @return double 值
 */
#define TTS_VALUES_GET_DOUBLE(slot, attno) \
    ({ \
        union { double d; uint64_t u; } _u; \
        _u.u = (uint64_t)(uintptr_t)((slot)->tts_values[(attno)]); \
        _u.d; \
    })

/**
 * @brief 设置 TupleTableSlot 中的浮点值
 * @param slot 元组槽
 * @param attno 属性号
 * @param value float 值
 */
#define TTS_VALUES_SET_FLOAT(slot, attno, value) \
    TTS_VALUES_SET_DOUBLE((slot), (attno), (double)(value))

/**
 * @brief 设置 TupleTableSlot 中的 NULL 值
 * @param slot 元组槽
 * @param attno 属性号
 */
#define TTS_VALUES_SET_NULL(slot, attno) \
    do { \
        (slot)->tts_values[(attno)] = NULL; \
        (slot)->tts_isnull[(attno)] = true; \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

/* 计划树（不定义 Expr，使用 sql_planner.h 中的定义） */
#ifndef PLANSTATE_DEFINED
#define PLANSTATE_DEFINED
typedef struct Plan          Plan;
typedef struct ExprState     ExprState;
typedef struct PlanState     PlanState;
typedef struct EState        EState;
typedef struct ExprContext   ExprContext;
typedef struct TupleTableSlot        TupleTableSlot;
typedef struct TupleTableSlotOps     TupleTableSlotOps;
typedef struct TupleDescData         TupleDescData;
typedef struct ProjectionInfo        ProjectionInfo;
typedef struct ResultRelInfo         ResultRelInfo;
typedef struct Instrumentation       Instrumentation;
typedef struct SnapshotData          Snapshot;
typedef struct Bitmapset             Bitmapset;
typedef struct List                  List;
typedef struct Relation              Relation;
#endif

/* 解析期类型（Oid 和 CommandId 在 sql_types.h 中已定义） */

/* Plan 结构体定义（简化版，供 Result 等节点使用） */
struct Plan {
    NodeTag         type;           /**< 节点类型标签 */
    struct Plan    *lefttree;       /**< 左子计划树 */
    struct Plan    *righttree;      /**< 右子计划树 */
    /* 后续任务会补充完整字段 */
};

/* ========================================================================
 * 通用小型数据结构
 * ======================================================================== */

/**
 * @brief Bitmapset 简化位图集合
 *
 * 用于参数表达式追踪（chgParam）、依赖管理等场景。
 * 实现为 words[1] 形式的柔性数组，实际 word 数 = nwords。
 */
typedef struct Bitmapset {
    int             nwords;        /**< word 数 */
    unsigned long   words[1];      /**< 柔性数组：位图数据 */
} Bitmapset;

/**
 * @brief 节点级性能/诊断信息
 *
 * 由 instrumentation.c 在每个节点初始化时分配，shutdown 时汇总输出。
 */
typedef struct Instrumentation {
    double          startup_time;  /**< 初始化时间（秒） */
    double          total_time;    /**< 总执行时间（秒） */
    uint64_t        tuples_out;    /**< 输出元组数 */
    uint64_t        buf_hit;       /**< Buffer 命中次数 */
    uint64_t        buf_read;      /**< Buffer 物理读次数 */
} Instrumentation;

/**
 * @brief 快照（MVCC 可见性判定用）
 *
 * 本任务先以占位结构体的形式存在，由后续事务子系统任务补充实现。
 */
typedef struct SnapshotData {
    NodeTag         type;
    /* 详细字段由 Task 4（事务与 MVCC）补充 */
    uint64_t        xmin;          /**< 活跃事务下界 */
    uint64_t        xmax;          /**< 活跃事务上界 */
} SnapshotData;

/* ========================================================================
 * TupleTableSlot 接口
 * ======================================================================== */

/**
 * @brief 元组槽操作虚表
 *
 * TupleTableSlot 是抽象基类（不同实现：HeapTupleTableSlot / VirtualTupleTableSlot
 * 等），操作通过函数指针分派。后续任务会提供具体实现并填充本表。
 *
 * 当前任务先以占位签名定义接口，方法体留待实现任务提供。
 */
typedef struct TupleTableSlotOps {
    /* 释放槽内部数据 */
    void (*release)(TupleTableSlot *slot);
    /* 清空槽（保留结构与描述符） */
    void (*clear)(TupleTableSlot *slot);
    /* 取槽内 materialized 元组（只读） */
    void *(*get_heap_tuple)(TupleTableSlot *slot);
} TupleTableSlotOps;

/**
 * @brief TupleTableSlot - 元组槽
 *
 * 在算子之间传递结果的容器。可能持有 materialized 元组指针
 * 或一组 Datum 数组形式的"虚拟"元组。
 */
struct TupleTableSlot {
    NodeTag                 type;           /**< T_TupleTableSlot 或子类 */
    struct TupleDescData   *tts_tupleDescriptor;  /**< 元组描述符 */
    Datum                  *tts_values;     /**< Datum[] 每个字段的取值 */
    bool                   *tts_isnull;     /**< bool[] 每个字段的 NULL 标志 */
    Size                    tts_mcxt;       /**< 关联内存上下文（占位：先存 pointer 化值） */
    const TupleTableSlotOps *tts_ops;       /**< 操作虚表 */
    int                     tts_nvalid;     /**< tts_values 中已填充的字段数 */
    bool                    tts_shouldFree; /**< 释放时是否需要 free 底层元组 */
    bool                    tts_shouldFreeMin; /**< 释放时是否需要 free 最小元组 */
    void                   *tts_tuple;      /**< materialized 元组指针 */
    unsigned short          tts_tupleLen;
    unsigned short          tts_minTupleLen;
};

/* ========================================================================
 * TupleDesc / ProjectionInfo
 * ======================================================================== */

/**
 * @brief TupleDescData - 元组描述符
 *
 * 描述字段数、类型、约束等信息。本任务仅占位，详细字段由 catalog 任务补充。
 */
typedef struct AttrNumber {
    int     attnum;
    const char *attname;
    Oid      atttype;
    int      atttypmod;
    int      attdim;
    bool     attbyval;
    bool     attnotnull;
} AttrNumber;

struct TupleDescData {
    NodeTag         type;       /**< T_TupleDesc */
    int             natts;      /**< 字段个数 */
    AttrNumber     *attrs;      /**< 字段元数据数组 */
    Size            tds_mcxt;   /**< 关联内存上下文（占位） */
};

/**
 * @brief ProjectionInfo - 投影表达式求值信息
 *
 * 存储计划节点的"输出"表达式列表及对应的执行期状态。
 */
struct ProjectionInfo {
    NodeTag             type;       /**< T_ProjectionInfo */
    struct ExprState   *pi_expr;    /**< 投影表达式（ExprState 树） */
    List               *pi_varSlot; /**< Var -> 源槽映射（占位） */
    bool                pi_isVarList;
};

/**
 * @brief ResultRelInfo - INSERT/UPDATE/DELETE 目标表信息
 *
 * 本任务先以占位形式存在，详细字段由 DML 执行器任务补充。
 */
struct ResultRelInfo {
    NodeTag             type;       /**< T_ResultRelInfo */
    Relation           *ri_RelationDesc;  /**< 目标关系（占位） */
    int                 ri_RelationDescRef;  /**< 引用计数占位 */
    /* 后续任务补充：ri_TrigDesc / ri_ConstraintExprs 等 */
};

/* ========================================================================
 * ExprContext - 表达式求值上下文
 * ======================================================================== */

/**
 * @brief ExprContext - 表达式求值上下文
 *
 * 携带在执行期间会被频繁读写的"上下文状态"，例如当前扫描元组、
 * Join 的内外元组、参数值数组等。它本身只是一个缓冲区，不持有
 * 长期资源——所有释放由 EState 的 ecxt_per_query_memory / ecxt_per_tuple_memory
 * 负责。
 */
struct ExprContext {
    NodeTag         type;                       /**< T_ExprContext */
    MemoryContext   ecxt_per_query_memory;      /**< 整个查询周期内分配的内存 */
    MemoryContext   ecxt_per_tuple_memory;      /**< 每个元组处理结束即 reset 的临时内存 */
    TupleTableSlot *ecxt_scantuple;             /**< 当前扫描节点产生的元组 */
    TupleTableSlot *ecxt_innertuple;            /**< Join 内表元组 */
    TupleTableSlot *ecxt_outertuple;            /**< Join 外表元组 */
    Datum          *ecxt_param_values;          /**< 参数值数组 */
    bool           *ecxt_param_nulls;           /**< 参数 NULL 标记数组 */
    int             ecxt_param_num;             /**< 参数个数 */
};

/* ========================================================================
 * EState - 执行器状态
 * ======================================================================== */

/**
 * @brief EState - 执行器状态（per-query）
 *
 * 每个执行的顶层查询都有一个 EState，承载：
 *   - 长生命周期内存上下文（es_query_cxt）
 *   - 当前打开的 Relation 列表
 *   - MVCC 快照
 *   - 结果元组槽表
 *   - 表达式上下文表
 *   - 处理元组计数、扫描方向等运行时信息
 *
 * 所有 PlanState 通过 state 字段指回其所属 EState。
 */
struct EState {
    NodeTag                  type;                       /**< T_EState */
    MemoryContext            es_query_cxt;               /**< 查询级内存上下文 */
    List                    *es_tupleTable;              /**< 所有 TupleTableSlot 列表 */
    TupleTableSlot          *es_trig_tuple_slot;         /**< 触发器专用元组槽 */
    Snapshot                 es_snapshot;                /**< MVCC 快照 */
    CommandId                es_output_cid;              /**< 输出命令 ID */
    List                    *es_range_table;             /**< FROM 子句（解析期 RangeVar 列表） */
    List                    *es_relations;               /**< 当前打开的 Relation 列表 */
    uint64_t                 es_processed;               /**< 已处理的元组数 */
    List                    *es_exprcontexts;            /**< 表达式上下文列表 */
    struct ExprContext      *es_per_tuple_exprctx;       /**< 每个元组复用的表达式上下文 */
    int                      es_direction;               /**< 扫描方向（ForwardScanDirection） */
    List                    *es_result_relations;        /**< INSERT/UPDATE/DELETE 目标表列表 */
    struct ResultRelInfo    *es_current_result_rel;      /**< 当前 ResultRelInfo */
    bool                     es_use_parallel_mode;       /**< 是否使用并行 */
    int                      es_parallel_workers_needed; /**< 期望的并行 worker 数 */
};

/* ========================================================================
 * PlanState - 计划节点运行时状态基类
 *
 * 这是火山模型里所有 Plan 节点的运行时状态基类。
 * 各类 Plan 节点对应一个 PlanState 子结构（SeqScanState / HashJoinState 等），
 *   将本结构作为首字段。
 * ======================================================================== */

/**
 * @brief ExecProcNodeMtd - 虚函数：取下一个元组
 *
 * 返回值：NULL 表示无更多元组；否则返回当前元组所在的 TupleTableSlot。
 */
typedef TupleTableSlot *(*ExecProcNodeMtd)(PlanState *pstate);

struct PlanState {
    NodeTag                  type;                       /**< T_PlanState 或子类（如 T_SeqScanState） */
    struct Plan             *plan;                       /**< 对应的计划树节点 */
    struct EState           *state;                      /**< 所属执行器状态 */
    ExecProcNodeMtd          ExecProcNode;               /**< 取下一个元组的虚函数 */
    ExecProcNodeMtd          ExecProcNodeReal;           /**< 真实函数指针（被 ExecProcNode 包装） */

    /* 子树 */
    PlanState               *lefttree;
    PlanState               *righttree;

    /* 条件 */
    struct ExprState        *qual;                       /**< 过滤条件 */
    struct ExprState        *recheck;                    /**< EPQ recheck 条件 */

    /* 投影与输出 */
    struct ProjectionInfo   *ps_ProjInfo;                /**< 投影表达式信息 */
    TupleTableSlot          *ps_ResultTupleSlot;         /**< 结果元组槽 */
    struct TupleDescData    *ps_ResultTupleDesc;         /**< 结果元组描述符 */

    /* 表达式上下文（每个节点持有自己的 ecxt，内含内/外/扫描元组） */
    struct ExprContext      *ps_ExprContext;

    /* 诊断 / 参数追踪 */
    struct Instrumentation  *instrument;                 /**< 性能计数 */
    bool                     needs_to_scan_queue;       /**< 是否需要扫描队列（占位） */
    Bitmapset               *chgParam;                   /**< 参数表达式依赖集 */
};

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODES_EXECNODES_H */
