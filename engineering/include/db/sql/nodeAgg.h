/**
 * @file nodeAgg.h
 * @brief Agg 聚合执行器节点
 *
 * Agg 节点用于聚合查询，如：
 *   - SELECT COUNT(*) FROM table
 *   - SELECT dept, AVG(salary) FROM emp GROUP BY dept
 *   - SELECT SUM(amount) FROM orders
 *
 * 执行行为：
 *   - AGG_PLAIN：无 GROUP BY，单次扫描计算聚合值
 *   - AGG_SORTED：GROUP BY 列已排序，按组计算
 *   - AGG_HASHED：构建哈希表进行分组聚合
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的 Task 2.4。
 */

#ifndef DB_SQL_NODE_AGG_H
#define DB_SQL_NODE_AGG_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 聚合策略
 * ======================================================================== */

/**
 * @brief 聚合策略枚举
 */
typedef enum AggStrategy {
    AGG_PLAIN = 0,      /**< 无 GROUP BY（标量聚合） */
    AGG_SORTED = 1,     /**< GROUP BY 列已排序 */
    AGG_HASHED = 2      /**< GROUP BY 使用哈希表 */
} AggStrategy;

/* ========================================================================
 * 聚合函数种类与聚合表达式描述
 * ======================================================================== */

/**
 * @brief 聚合函数种类
 *
 * 驱动层（sql_driver.c）把 SQL 里的聚合函数名映射到该枚举，
 * 执行层（nodeAgg.c）据此选择累加子程序。
 */
typedef enum AggFuncKind {
    AGG_FUNC_COUNT = 0, /**< COUNT(*)/COUNT(col) */
    AGG_FUNC_SUM,       /**< SUM(col) */
    AGG_FUNC_MIN,       /**< MIN(col) */
    AGG_FUNC_MAX,       /**< MAX(col) */
    AGG_FUNC_AVG        /**< AVG(col)（当前驱动层拒绝，无浮点类型） */
} AggFuncKind;

/**
 * @brief 单个聚合表达式的描述
 *
 * 由驱动层在构建 Agg 计划节点时分配，挂在 Agg.aggrefs 链表上；
 * 执行层遍历该链表逐项累加。
 */
typedef struct AggrefDef {
    AggFuncKind fn;         /**< 聚合函数种类 */
    int         arg_attno;  /**< 参数列号；COUNT(*) 为 -1 */
    Oid         arg_type;   /**< 参数列类型 */
    Oid         out_type;   /**< 输出列类型 */
    char       *out_name;   /**< 输出列名（可为 NULL） */
} AggrefDef;

/* ========================================================================
 * Agg 计划节点
 * ======================================================================== */

/**
 * @brief Agg 计划节点
 *
 * 用于聚合查询。
 *
 * 设计说明：Agg 结构体嵌入 Plan 作为第一个字段，使得 Agg* 可以安全转换为 Plan*。
 */
typedef struct Agg {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    AggStrategy  aggstrategy;        /**< 聚合策略 */
    int          numCols;            /**< GROUP BY 列数 */
    int         *grpColIdx;          /**< GROUP BY 列索引数组 */
    Oid         *grpColTypes;        /**< GROUP BY 列类型数组 */
    List        *aggrefs;            /**< 聚合表达式列表（元素为 AggrefDef*） */
    int          out_natts;          /**< 输出列数 = GROUP BY 列数 + 聚合项数 */
    struct TupleDescData *out_desc;  /**< 输出元组描述符（可为 NULL，由驱动填充） */
} Agg;

/* ========================================================================
 * AggState - Agg 执行状态
 * ======================================================================== */

/**
 * @brief Agg 执行状态
 *
 * 维护 Agg 节点的运行时状态。
 */
typedef struct AggState {
    PlanState    ps;                 /**< 基类：计划状态 */
    AggStrategy  aggstrategy;        /**< 聚合策略 */
    int          numCols;            /**< GROUP BY 列数 */
    int         *grpColIdx;          /**< GROUP BY 列索引 */
    Oid         *grpColTypes;        /**< GROUP BY 列类型 */
    List        *aggrefs;            /**< 聚合表达式列表（元素为 AggrefDef*） */
    int          out_natts;          /**< 输出列数 */
    struct TupleDescData *out_desc;  /**< 输出元组描述符 */
    TupleTableSlot *agg_slot;        /**< 聚合结果槽 */
    bool         agg_done;           /**< 聚合完成标志 */
    struct AggPerGroupData *pergroup; /**< 每组聚合状态 */
    void        *hash_table;         /**< AGG_HASHED 的哈希表（内部类型 AggHashTable*） */
    int          hash_ngroups;       /**< 已产生的分组数 / 输出游标 */
    void        *plain_acc;          /**< AGG_PLAIN 的累加器（内部类型 AggPlainAccum*） */
} AggState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Agg 节点
 *
 * 分配 AggState 并设置 ExecProcNode 函数指针。
 * 同时初始化子节点。
 *
 * @param plan   计划节点（实际类型为 Agg*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 AggState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitAgg(Plan *plan, EState *estate, int eflags);

/**
 * @brief Agg 节点执行函数
 *
 * 执行聚合计算。
 * 当前为框架实现，返回 NULL。
 *
 * @param pstate PlanState（实际类型为 AggState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecAgg(PlanState *pstate);

/**
 * @brief 结束 Agg 节点
 *
 * 释放 AggState 关联的资源。
 *
 * @param node AggState（可为 NULL）
 */
void ExecEndAgg(AggState *node);

/**
 * @brief 重置 Agg 节点（用于重新扫描）
 *
 * 重置聚合状态，允许重新计算。
 *
 * @param node AggState
 */
void ExecReScanAgg(AggState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_AGG_H */