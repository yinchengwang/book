/**
 * @file window.h
 * @brief 窗口函数接口定义
 *
 * 实现 PostgreSQL 风格的窗口函数执行器：
 *   - 聚合窗口函数: SUM, AVG, COUNT, MIN, MAX
 *   - 排名窗口函数: ROW_NUMBER, RANK, DENSE_RANK, NTILE
 *   - 偏移窗口函数: LAG, LEAD, FIRST_VALUE, LAST_VALUE
 *   - 帧定义: ROWS, RANGE 模式
 *
 * 本文件是 SQL 执行引擎 P1-3 窗口函数 + CTE 任务的核心接口。
 */

#ifndef DB_SQL_WINDOW_H
#define DB_SQL_WINDOW_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 窗口函数类型
 * ======================================================================== */

/**
 * @brief 窗口函数类型枚举
 */
typedef enum WindowFuncType {
    WINDOWFUNC_ROW_NUMBER = 0,   /**< ROW_NUMBER() */
    WINDOWFUNC_RANK,             /**< RANK() */
    WINDOWFUNC_DENSE_RANK,       /**< DENSE_RANK() */
    WINDOWFUNC_NTILE,            /**< NTILE(n) */
    WINDOWFUNC_PERCENT_RANK,     /**< PERCENT_RANK() */
    WINDOWFUNC_CUME_DIST,        /**< CUME_DIST() */
    WINDOWFUNC_LAG,              /**< LAG(expr, offset, default) */
    WINDOWFUNC_LEAD,             /**< LEAD(expr, offset, default) */
    WINDOWFUNC_FIRST_VALUE,      /**< FIRST_VALUE(expr) */
    WINDOWFUNC_LAST_VALUE,       /**< LAST_VALUE(expr) */
    WINDOWFUNC_NTH_VALUE,        /**< NTH_VALUE(expr, n) */
    WINDOWFUNC_COUNT,            /**< COUNT(*) OVER (...) */
    WINDOWFUNC_SUM,              /**< SUM(expr) OVER (...) */
    WINDOWFUNC_AVG,              /**< AVG(expr) OVER (...) */
    WINDOWFUNC_MIN,              /**< MIN(expr) OVER (...) */
    WINDOWFUNC_MAX               /**< MAX(expr) OVER (...) */
} WindowFuncType;

/* ========================================================================
 * 帧边界类型
 * ======================================================================== */

/**
 * @brief 帧边界类型
 */
typedef enum FrameBoundType {
    FRAME_BOUND_UNBOUNDED_PRECEDING = 0,  /**< UNBOUNDED PRECEDING */
    FRAME_BOUND_PRECEDING,                /**< n PRECEDING */
    FRAME_BOUND_CURRENT_ROW,              /**< CURRENT ROW */
    FRAME_BOUND_FOLLOWING,                /**< n FOLLOWING */
    FRAME_BOUND_UNBOUNDED_FOLLOWING       /**< UNBOUNDED FOLLOWING */
} FrameBoundType;

/* ========================================================================
 * 帧模式
 * ======================================================================== */

/**
 * @brief 帧模式
 */
typedef enum FrameMode {
    FRAME_MODE_ROWS = 0,   /**< ROWS 模式：按物理行数 */
    FRAME_MODE_RANGE       /**< RANGE 模式：按逻辑值范围 */
} FrameMode;

/* ========================================================================
 * 窗口帧定义
 * ======================================================================== */

/**
 * @brief 窗口帧定义
 *
 * 描述窗口函数的计算范围。
 */
typedef struct WindowFrame {
    FrameMode    frameMode;      /**< 帧模式 */
    FrameBoundType startBound;   /**< 起始边界 */
    int          startOffset;    /**< 起始偏移量（用于 PRECEDING/FOLLOWING） */
    FrameBoundType endBound;     /**< 结束边界 */
    int          endOffset;      /**< 结束偏移量 */
} WindowFrame;

/* ========================================================================
 * 窗口函数定义
 * ======================================================================== */

/**
 * @brief 窗口函数定义（解析期结构）
 *
 * 存储窗口函数的定义信息。
 */
typedef struct WindowFunc {
    NodeTag         type;           /**< T_WindowFunc */
    WindowFuncType  wintype;        /**< 窗口函数类型 */
    Node           *args;           /**< 函数参数 */
    int             argIndex;       /**< 参数索引（用于偏移函数） */
    int             winfnums;       /**< 窗口函数编号（Planner 分配） */
    int             winref;         /**< 窗口定义引用 */
    Node           *aggfilter;      /**< FILTER 子句 */
    bool            aggdistinct;    /**< DISTINCT */
    Node           *aggorder;       /**< 聚合内 ORDER BY */
} WindowFunc;

/* ========================================================================
 * 窗口定义（解析期结构）
 * ======================================================================== */

/**
 * @brief 窗口定义（对应 SQL 中的 WINDOW 子句）
 *
 * 定义窗口的分区和排序方式。
 */
typedef struct Window {
    NodeTag         type;           /**< T_Window */
    char           *winname;        /**< 窗口名 */
    char           *refname;        /**< 引用的窗口名 */
    List           *partitionClause;/**< PARTITION BY 列表 */
    List           *orderClause;    /**< ORDER BY 列表 */
    List           *frameOptions;   /**< 帧选项 */
    WindowFrame     *frame;         /**< 帧定义 */
} Window;

/* ========================================================================
 * WindowAgg 计划节点
 * ======================================================================== */

/**
 * @brief WindowAgg 计划节点
 *
 * 用于窗口函数计算。
 */
typedef struct WindowAgg {
    Plan           plan;            /**< 基类：计划节点 */
    int            numCols;         /**< 分区/排序列数 */
    int           *planColIdx;      /**< 列索引 */
    List           *windowFuncs;    /**< 窗口函数列表 */
    List           *partClause;     /**< PARTITION BY 子句 */
    List           *ordClause;      /**< ORDER BY 子句 */
    List           *frameOptions;   /**< 帧选项 */
    WindowFrame    *frame;          /**< 帧定义 */
    int             first;
    int             last;
    int             offset;
    int             defaultOffset;
} WindowAgg;

/* ========================================================================
 * 窗口函数执行状态
 * ======================================================================== */

/**
 * @brief 单个窗口函数的运行时状态
 */
typedef struct WindowFuncState {
    WindowFuncType  wintype;        /**< 函数类型 */
    int             argIndex;       /**< 参数索引 */
    int             argIndex2;      /**< 第二参数索引（用于 LAG/LEAD） */
    Datum           defaultValue;   /**< 默认值 */
    bool            defaultIsNull;  /**< 默认值是否为 NULL */

    /* 排名函数状态 */
    int64_t         rankValue;      /**< 当前排名值 */
    int64_t         rankOffset;     /**< 排名偏移（用于 DENSE_RANK） */

    /* 聚合状态（用于 SUM/AVG/COUNT/MIN/MAX） */
    bool            aggInitialized; /**< 是否已初始化 */
    int64_t         aggCount;       /**< COUNT 计数 */
    double          aggSum;         /**< SUM 求和 */
    double          aggMin;         /**< MIN 值 */
    double          aggMax;         /**< MAX 值 */

    /* NTILE 状态 */
    int             ntileBuckets;   /**< NTILE 分桶数 */
    int             ntileCount;     /**< 当前计数 */

    /* LAG/LEAD 状态 */
    int             lagOffset;      /**< 偏移量 */
    Datum          *lagBuffer;      /**< 值缓冲区 */
    bool           *lagBufferNull; /**< NULL 缓冲区 */
    int             lagBufferSize;  /**< 缓冲区大小 */
    int             lagBufferPos;   /**< 当前缓冲区位置 */
} WindowFuncState;

/* ========================================================================
 * WindowAggState - WindowAgg 执行状态
 * ======================================================================== */

/**
 * @brief WindowAgg 执行状态
 *
 * 维护窗口函数执行的运行时状态。
 */
typedef struct WindowAggState {
    PlanState        ps;                 /**< 基类：计划状态 */
    TupleTableSlot  *windowtuple;       /**< 窗口元组槽 */
    TupleTableSlot  *firstPartitionSlot;/**< 第一个分区元组槽 */
    bool             allDone;           /**< 是否全部完成 */
    bool             morePartitions;    /**< 是否有更多分区 */

    /* 分区信息 */
    int              numCols;           /**< 分区列数 */
    int             *partColIdx;        /**< 分区列索引 */

    /* 帧信息 */
    WindowFrame      frame;             /**< 帧定义 */
    int              frameHead;         /**< 帧头位置 */
    int              frameTail;         /**< 帧尾位置 */

    /* 元组缓冲区 */
    TupleTableSlot **partitionBuffers;  /**< 分区缓冲区 */
    int              partitionBufferSize; /**< 缓冲区大小 */
    int              partitionBufferUsed; /**< 已使用大小 */
    int              currentPos;        /**< 当前处理位置 */

    /* 窗口函数状态数组 */
    WindowFuncState *funcStates;        /**< 窗口函数状态数组 */
    int              numFuncs;          /**< 窗口函数数量 */

    /* 排序状态 */
    List            *ordClause;         /**< ORDER BY 子句 */
    bool             alreadySorted;     /**< 是否已排序 */
} WindowAggState;

/* ========================================================================
 * CTE 相关类型
 * ======================================================================== */

/**
 * @brief CTE 定义
 */
typedef struct CommonTableExpr {
    NodeTag         type;           /**< T_CommonTableExpr */
    char           *ctename;        /**< CTE 名称 */
    List           *aliascolnames;  /**< 列别名 */
    struct SelectStmt *ctequery;    /**< CTE 查询 */
    bool            recursive;      /**< 是否递归 */
    List           *searchClause;   /**< SEARCH 子句 */
    List           *cycleClause;    /**< CYCLE 子句 */
    int             location;       /**< 位置 */
} CommonTableExpr;

/**
 * @brief CTE 列表节点
 */
typedef struct CTEList {
    NodeTag         type;           /**< T_CTEList */
    List           *ctes;           /**< CTE 列表 */
} CTEList;

/**
 * @brief CTE 上下文
 */
typedef struct CTEContext {
    NodeTag         type;           /**< T_CTEContext */
    List           *ctes;           /**< CTE 定义列表 */
    PlanState      *ctePlanstates;  /**< CTE 执行计划状态数组 */
    int             numCTEs;        /**< CTE 数量 */
    MemoryContext   cteMemoryContext; /**< CTE 内存上下文 */
} CTEContext;

/* ========================================================================
 * 公共 API - WindowAgg
 * ======================================================================== */

/**
 * @brief 初始化 WindowAgg 节点
 *
 * @param plan   计划节点（实际类型为 WindowAgg*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 WindowAggState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitWindowAgg(Plan *plan, EState *estate, int eflags);

/**
 * @brief WindowAgg 节点执行函数
 *
 * @param pstate PlanState（实际类型为 WindowAggState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecWindowAgg(PlanState *pstate);

/**
 * @brief 结束 WindowAgg 节点
 *
 * @param node WindowAggState（可为 NULL）
 */
void ExecEndWindowAgg(WindowAggState *node);

/**
 * @brief 重置 WindowAgg 节点
 *
 * @param node WindowAggState
 */
void ExecReScanWindowAgg(WindowAggState *node);

/* ========================================================================
 * 公共 API - CTE
 * ======================================================================== */

/**
 * @brief 初始化 CTE 上下文
 *
 * @param estate 执行器状态
 * @param withClause WITH 子句列表
 *
 * @return CTE 上下文；失败返回 NULL
 */
CTEContext *ExecInitCTE(EState *estate, List *withClause);

/**
 * @brief 查找 CTE 定义
 *
 * @param cteCtx CTE 上下文
 * @param cteName CTE 名称
 *
 * @return CTE 定义；未找到返回 NULL
 */
CommonTableExpr *ExecFindCTE(CTEContext *cteCtx, const char *cteName);

/**
 * @brief 执行 CTE（用于递归 CTE 的递归步骤）
 *
 * @param cteCtx CTE 上下文
 * @param cteName CTE 名称
 * @param anchorTuple 锚点元组（用于递归）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecCTEScan(CTEContext *cteCtx, const char *cteName, TupleTableSlot *anchorTuple);

/**
 * @brief 释放 CTE 上下文
 *
 * @param cteCtx CTE 上下文（可为 NULL）
 */
void ExecEndCTE(CTEContext *cteCtx);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 比较两个元组在分区列上是否相等
 *
 * @param slot1 元组槽1
 * @param slot2 元组槽2
 * @param numCols 分区列数
 * @param colIdx 分区列索引数组
 *
 * @return true 表示相等
 */
bool window_tuples_match_partition(TupleTableSlot *slot1, TupleTableSlot *slot2,
                                   int numCols, int *colIdx);

/**
 * @brief 计算帧边界
 *
 * @param state WindowAggState
 * @param currentPos 当前行位置
 * @param totalRows 总行数
 * @param frame 帧定义
 * @param head 输出：帧头位置
 * @param tail 输出：帧尾位置
 */
void window_compute_frame_bounds(WindowAggState *state, int currentPos,
                                 int totalRows, WindowFrame *frame,
                                 int *head, int *tail);

/**
 * @brief 评估窗口函数
 *
 * @param funcState 窗口函数状态
 * @param args 参数值
 * @param argsNull NULL 标记数组
 * @param numArgs 参数数量
 *
 * @return 函数结果
 */
Datum window_func_eval(WindowFuncState *funcState, Datum *args, bool *argsNull, int numArgs);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_WINDOW_H */
