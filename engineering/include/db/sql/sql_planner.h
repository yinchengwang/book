/**
 * @file sql_planner.h
 * @brief 查询计划器头文件
 *
 * 实现查询计划器：
 * 1. 逻辑算子（LogicalScan/Join/Agg/Project）
 * 2. 物理算子（PhysSeqScan/IndexScan/HashJoin）
 * 3. 代价估算
 * 4. 优化规则
 */
#ifndef DB_SQL_PLANNER_H
#define DB_SQL_PLANNER_H

#include <stdint.h>
#include <stdbool.h>

/* 首先定义 ExprType_DEFINED 标记，避免与 expr.h 冲突 */
#ifndef EXPR_TYPE_DEFINED
#define EXPR_TYPE_DEFINED
#endif

/* 引入 parsenodes.h 以复用真实的 List 定义（而非空宏） */
#include "db/parser/sql/parsenodes.h"

/* 引入共享类型定义（包含 Oid） */
#include "db/sql/sql_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 解析期类型定义（Oid 在 sql_types.h 中定义为 uint64_t） */

/* ========================================================================
 * 逻辑算子类型
 * ======================================================================== */

/** 逻辑算子类型 */
typedef enum LogicalOpType_e {
    /* 扫描算子 */
    LOGICAL_SCAN,           /**< 全表扫描 */
    LOGICAL_INDEX_SCAN,     /**< 索引扫描 */
    LOGICAL_BITMAP_SCAN,    /**< 位图扫描 */
    LOGICAL_VECTOR_SCAN,    /**< 向量扫描 */

    /* 连接算子 */
    LOGICAL_JOIN,           /**< 连接 */
    LOGICAL_NESTLOOP,       /**< 嵌套循环连接 */
    LOGICAL_HASHJOIN,       /**< Hash 连接 */
    LOGICAL_MERGEJOIN,      /**< 归并连接 */

    /* 聚合算子 */
    LOGICAL_AGG,            /**< 聚合 */
    LOGICAL_GROUPAGG,       /**< 分组聚合 */
    LOGICAL_SORTAGG,       /**< 排序聚合 */
    LOGICAL_HASHAGG,       /**< Hash 聚合 */

    /* 修饰算子 */
    LOGICAL_PROJECT,        /**< 投影 */
    LOGICAL_FILTER,         /**< 过滤 */
    LOGICAL_SORT,           /**< 排序 */
    LOGICAL_LIMIT,          /**< 限制行数 */
    LOGICAL_OFFSET,         /**< 跳过行数 */
    LOGICAL_DISTINCT,       /**< 去重 */
    LOGICAL_WINDOW,         /**< 窗口函数 */
    LOGICAL_SETOP,          /**< 集合操作 (UNION/INTERSECT/EXCEPT) */

    /* 修改算子 */
    LOGICAL_INSERT,         /**< 插入 */
    LOGICAL_UPDATE,         /**< 更新 */
    LOGICAL_DELETE,         /**< 删除 */
    LOGICAL_MODIFY,         /**< 修改 */

    /* 结果算子 */
    LOGICAL_RESULT,         /**< 结果 */
    LOGICAL_VALUES,         /**< VALUES 子句 */
    LOGICAL_CTE_SCAN,       /**< CTE 扫描 */
    LOGICAL_MATERIAL,       /**< 物化 */
    LOGICAL_MVIEW_SCAN,     /**< 物化视图扫描 */
    LOGICAL_MVIEW_REFRESH   /**< 物化视图刷新 */
} LogicalOpType;

/* ========================================================================
 * 物理算子类型
 * ======================================================================== */

/** 物理算子类型 */
typedef enum PhysicalOpType_e {
    /* 扫描算子 */
    PHYS_SEQ_SCAN,          /**< 顺序扫描 */
    PHYS_INDEX_SCAN,        /**< 索引扫描 */
    PHYS_INDEX_ONLY_SCAN,   /**< 仅索引扫描 */
    PHYS_BITMAP_SCAN,       /**< 位图扫描 */
    PHYS_BITMAP_HEAP_SCAN,  /**< 位图堆扫描 */
    PHYS_TID_SCAN,          /**< TID 扫描 */
    PHYS_SUBQUERY_SCAN,     /**< 子查询扫描 */
    PHYS_FUNCTION_SCAN,     /**< 函数扫描 */
    PHYS_VALUES_SCAN,       /**< VALUES 扫描 */
    PHYS_CTE_SCAN,          /**< CTE 扫描 */
    PHYS_NAMEDTUPLESTORE_SCAN, /**< 命名元组存储扫描 */
    PHYS_WORKTABLEScan,      /**< 工作表扫描 */
    PHYS_MATERIAL_SCAN,     /**< 物化扫描 */
    PHYS_MVIEW_SCAN,        /**< 物化视图扫描 */
    PHYS_VECTOR_SCAN,       /**< 向量扫描 */
    PHYS_HNSW_SCAN,         /**< HNSW 索引扫描 */
    PHYS_IVF_SCAN,          /**< IVF 索引扫描 */
    PHYS_DISKANN_SCAN,      /**< DiskANN 扫描 */
    PHYS_BM25_SCAN,         /**< BM25 全文扫描 */
    PHYS_TIMESERIES_SCAN,   /**< 时序扫描 */
    PHYS_GRAPH_SCAN,        /**< 图扫描 */
    PHYS_DOCUMENT_SCAN,     /**< 文档扫描 */
    PHYS_SPATIAL_SCAN,      /**< 空间扫描 */

    /* 并行算子 */
    PHYS_GATHER,            /**< Gather 并行汇聚 */
    PHYS_GATHER_MERGE,      /**< GatherMerge 有序并行汇聚 */
    PHYS_PARALLEL_SCAN,     /**< 并行扫描 */

    /* 连接算子 */
    PHYS_NESTLOOP,          /**< 嵌套循环连接 */
    PHYS_NESTLOOP_PARAM,    /**< 带参数的嵌套循环 */
    PHYS_MERGEJOIN,         /**< 归并连接 */
    PHYS_HASHJOIN,          /**< Hash 连接 */
    PHYS_HASH,              /**< Hash 构建 */

    /* 聚合算子 */
    PHYS_SORT,              /**< 排序 */
    PHYS_GROUPAGG,          /**< 分组聚合 */
    PHYS_HASHAGG,          /**< Hash 聚合 */
    PHYS_SORTAGG,          /**< 排序聚合 */
    PHYS_UNIQUE,            /**< 去重 */
    PHYS_SETOP,             /**< 集合操作 */
    PHYS_LIMIT,             /**< 限制 */
    PHYS_WINDOWAGG,         /**< 窗口聚合 */
    PHYS_MATERIALIZE,       /**< 物化 */
    PHYS_SORTREVERSE,       /**< 逆序排序 */

    /* 修改算子 */
    PHYS_INSERT,            /**< 插入 */
    PHYS_UPDATE,            /**< 更新 */
    PHYS_DELETE,            /**< 删除 */
    PHYS_MODIFYTABLE,       /**< 修改表 */
    PHYS_VACUUM,            /**< 清理 */
    PHYS_ANALYZE,           /**< 分析 */
    PHYS_REFRESH_MVIEW,     /**< 刷新物化视图 */

    /* 结果算子 */
    PHYS_RESULT,            /**< 结果 */
    PHYS_EXTERNAL_SCAN,     /**< 外部扫描 */
    PHYS_FAKE_CUTUPLES,     /**< 假元组 */
    PHYS_RECURSIVEUNION,   /**< 递归联合 */
    PHYS_WORKTABLE,         /**< 工作表 */
    PHYS_CTE_WORKTABLE,     /**< CTE 工作表 */
    PHYS_SHAREINPUTSCAN,    /**< 共享输入扫描 */

    /* 控制算子 */
    PHYS_RETURN,            /**< 返回 */
    PHYS_CALLFUNC,          /**< 调用函数 */
    PHYS_EXECUTE,           /**< 执行 */
    PHYS_EXPLAIN            /**< EXPLAIN */
} PhysicalOpType;

/* ========================================================================
 * 表达式结构
 * ======================================================================== */

/* 使用共享的 SqlExprType 定义（避免与 expr.h 冲突） */
#include "db/sql/sql_types.h"

/** 表达式（planner 版本，与 expr.h 的 Expr 结构保持字段名一致） */
typedef struct Expr_s {
    NodeTag         type;           /**< 节点类型标签 T_Expr */
    SqlExprType     expr_type;      /**< 表达式类型 */
    Oid             result_type;    /**< 结果类型 OID */
    int             result_len;     /**< 结果长度 */
    int             result_by_val;  /**< 是否按值传递 */
    union {
        struct {
            int value;      /**< 常量值 */
            int isnull;     /**< 是否为 NULL */
        } const_val;
        struct {
            int varattno;  /**< 属性编号 */
            int varno;     /**< 变量编号 */
        } var;
        int paramno;       /**< 参数编号 */
        struct {
            int opno;      /**< 操作符 OID */
            struct Expr_s *lexpr;
            struct Expr_s *rexpr;
        } op;
        struct {
            int funcid;    /**< 函数 OID */
            struct Expr_s **args;
            int nargs;
        } func;
    } val;
} Expr;

/** 目标列表达式 */
typedef struct TargetEntry_s {
    int resno;              /**< 结果编号 */
    Expr *expr;             /**< 表达式 */
    char *resname;          /**< 结果名称 */
    int ressortgroupref;   /**< 排序组引用 */
    bool resjunk;           /**< 是否为垃圾列 */
} TargetEntry;

/* ========================================================================
 * 逻辑计划节点
 * ======================================================================== */

/** 逻辑扫描节点 */
typedef struct LogicalScan_s {
    LogicalOpType type;     /**< 节点类型 */
    int scan_relid;         /**< 扫描的 Relation ID */
    char *rel_name;        /**< 表名 */
    Expr *qual;            /**< 过滤条件 */
    List *targetlist;      /**< 目标列 */
    List *baserestrictinfo;/**< 基础限制信息 */
} LogicalScan;

/** 逻辑连接节点 */
typedef struct LogicalJoin_s {
    LogicalOpType type;     /**< 节点类型 */
    LogicalOpType join_type;/**< 连接类型 */
    int jointype;           /**< 连接类型（INNER/LEFT/RIGHT/FULL） */
    Expr *joinqual;         /**< 连接条件 */
    struct LogicalPlan_s *left;
    struct LogicalPlan_s *right;
} LogicalJoin;

/** 逻辑聚合节点 */
typedef struct LogicalAgg_s {
    LogicalOpType type;     /**< 节点类型 */
    int aggstrategy;        /**< 聚合策略 */
    int num_group_cols;     /**< 分组列数 */
    int *group_col_idx;    /**< 分组列索引 */
    int num_aggs;           /**< 聚合函数数 */
    List *aggs;            /**< 聚合函数列表 */
    List *groupClause;     /**< GROUP BY 子句 */
    bool hasAggs;          /**< 是否有聚合函数 */
} LogicalAgg;

/** 逻辑投影节点 */
typedef struct LogicalProject_s {
    LogicalOpType type;     /**< 节点类型 */
    List *targetlist;      /**< 目标列 */
    Expr *qual;            /**< 过滤条件（可选） */
} LogicalProject;

/** 逻辑计划节点（联合结构） */
typedef struct LogicalPlan_s {
    LogicalOpType type;     /**< 节点类型 */
    int node_id;            /**< 节点 ID */
    double total_cost;     /**< 总代价 */
    double startup_cost;   /**< 启动代价 */
    double rows;            /**< 估计行数 */
    int width;              /**< 行宽度 */
    List *targetlist;       /**< 目标列 */
    Expr *qual;            /**< 过滤条件 */
    struct LogicalPlan_s *left;   /**< 左子节点 */
    struct LogicalPlan_s *right;  /**< 右子节点 */
    List *initplan;        /**< 初始化计划 */
    void *extra;          /**< 额外信息 */
} LogicalPlan;

/* ========================================================================
 * 物理计划节点
 * ======================================================================== */

/** 物理扫描节点 */
typedef struct PhysScan_s {
    PhysicalOpType type;    /**< 节点类型 */
    int scan_relid;        /**< 扫描的 Relation ID */
    int indexid;           /**< 索引 ID（用于索引扫描） */
    Expr *qual;            /**< 过滤条件 */
    List *targetlist;      /**< 目标列 */
    List *recheck;         /**< 重新检查条件 */
    int nbatch;            /**< 批次数 */
    int nbuckets;          /**< 桶数 */
    double startup_cost;    /**< 启动代价 */
    double total_cost;     /**< 总代价 */
} PhysScan;

/** 物理连接节点 */
typedef struct PhysJoin_s {
    PhysicalOpType type;    /**< 节点类型 */
    Expr *joinqual;         /**< 连接条件 */
    Expr *hashclauses;     /**< Hash 条件 */
    struct PhysPlan_s *left;
    struct PhysPlan_s *right;
    double startup_cost;    /**< 启动代价 */
    double total_cost;     /**< 总代价 */
} PhysJoin;

/** 物理聚合节点 */
typedef struct PhysAgg_s {
    PhysicalOpType type;    /**< 节点类型 */
    int aggstrategy;       /**< 聚合策略 */
    int num_group_cols;    /**< 分组列数 */
    int *group_col_idx;   /**< 分组列索引 */
    Expr **aggs;           /**< 聚合表达式 */
    int num_aggs;          /**< 聚合数 */
    bool streamable;       /**< 是否可流式 */
    double startup_cost;    /**< 启动代价 */
    double total_cost;     /**< 总代价 */
} PhysAgg;

/** 物理计划节点 */
typedef struct PhysPlan_s {
    PhysicalOpType type;    /**< 节点类型 */
    int node_id;            /**< 节点 ID */
    double total_cost;     /**< 总代价 */
    double startup_cost;    /**< 启动代价 */
    double rows;            /**< 估计行数 */
    int width;              /**< 行宽度 */
    List *targetlist;       /**< 目标列 */
    Expr *qual;            /**< 过滤条件 */
    List *lefttree;         /**< 左子树 */
    List *righttree;       /**< 右子树 */
    void *extra;           /**< 额外信息 */
} PhysPlan;

/* 列表定义已移至 parsenodes.h（通过上面的 #include 引入） */

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/** 表统计信息 */
typedef struct TableStats_s {
    double nrows;           /**< 行数 */
    double nbytes;          /**< 字节数 */
    double density;        /**< 密度 */
    int ndistinct;         /**< 不同值数量 */
    double ndistinct_ratio;/**< 不同值比例 */
    double null_frac;       /**< NULL 比例 */
    double width;           /**< 平均宽度 */
    int relpages;           /**< 页面数 */
    int reltuples;          /**< 元组数 */
    double correlation;    /**< 物理/逻辑顺序相关性 */
} TableStats;

/** 列统计信息 */
typedef struct ColumnStats_s {
    double ndistinct;       /**< 不同值数量 */
    double null_frac;       /**< NULL 比例 */
    double width;           /**< 平均宽度 */
    double low_value;       /**< 最小值 */
    double high_value;      /**< 最大值 */
    int nbuckets;           /**< 桶数（直方图） */
    double *histogram;     /**< 直方图边界 */
    double *probabilities;  /**< 频率 */
} ColumnStats;

/** 索引统计信息 */
typedef struct IndexStats_s {
    double ntuples;         /**< 元组数 */
    double ndistinct;       /**< 不同值数量 */
    int nkeycols;          /**< 键列数 */
    double indexrelpages;  /**< 索引页面数 */
    double indexreltuples;/**< 索引元组数 */
    double heapfetchtuples;/**< 需要获取堆的元组数 */
} IndexStats;

/* ========================================================================
 * 计划器上下文
 * ======================================================================== */

/** 计划器配置 */
typedef struct PlannerConfig_s {
    double planner_rows;   /**< 估计行数 */
    int planner_eliminate_join;/**< 是否消除连接 */
    int join_collapse_limit;/**< 连接折叠限制 */
    int from_collapse_limit;/**< FROM 折叠限制 */
    bool enable_hashjoin;  /**< 启用 Hash Join */
    bool enable_nestloop;  /**< 启用嵌套循环 */
    bool enable_seqscan;   /**< 启用顺序扫描 */
    bool enable_indexscan; /**< 启用索引扫描 */
    bool enable_sort;      /**< 启用排序 */
    bool enable_hashagg;   /**< 启用 Hash 聚合 */
    bool enable_vector_scan;/**< 启用向量扫描 */
    bool enable_mview_scan;/**< 启用物化视图扫描 */
    double vector_search_k;/**< 向量搜索 top-k */
} PlannerConfig;

/** 计划器上下文 */
typedef struct PlannerContext_s {
    void *parser_ctx;      /**< 解析器上下文 */
    void *sem_ctx;        /**< 语义分析上下文 */
    void *catalog;         /**< 目录 */
    PlannerConfig config; /**< 配置 */
    List *active_dynamic_indexes;/**< 活跃动态索引 */
    List *rowMarks;       /**< 行标记 */
    List *subroots;       /**< 子计划根 */
    double tuple_fraction;/**< 元组比例 */
    double limit_offset;  /**< OFFSET 值 */
    double limit_count;   /**< LIMIT 值 */
} PlannerContext;

/* ========================================================================
 * 代价模型
 * ======================================================================== */

/** 代价参数 */
typedef struct CostParams_s {
    double seq_page_cost;     /**< 顺序页面代价 */
    double random_page_cost; /**< 随机页面代价 */
    double cpu_tuple_cost;   /**< CPU 元组代价 */
    double cpu_index_tuple_cost; /**< CPU 索引元组代价 */
    double cpu_operator_cost;/**< CPU 操作符代价 */
    double parallel_setup_cost;/**< 并行设置代价 */
    double parallel_tuple_cost;/**< 并行元组代价 */
    double vector_search_cost;/**< 向量搜索代价 */
    double vector_reorder_cost;/**< 向量重排序代价 */
} CostParams;

/** 默认代价参数 */
#define DEFAULT_COST_PARAMS { \
    .seq_page_cost = 1.0, \
    .random_page_cost = 4.0, \
    .cpu_tuple_cost = 0.01, \
    .cpu_index_tuple_cost = 0.005, \
    .cpu_operator_cost = 0.0025, \
    .parallel_setup_cost = 1000.0, \
    .parallel_tuple_cost = 0.1, \
    .vector_search_cost = 1.0, \
    .vector_reorder_cost = 0.1 \
}

/**
 * @brief 计算顺序扫描代价
 */
void cost_seqscan(PhysScan *node, PlannerContext *root,
                  double rows, int width);

/**
 * @brief 计算索引扫描代价
 */
void cost_index(PhysScan *node, PlannerContext *root,
                double rows, int width);

/**
 * @brief 计算 Hash Join 代价
 */
void cost_hashjoin(PhysJoin *node, PlannerContext *root,
                   double inner_rows, double outer_rows, int width);

/**
 * @brief 计算 Nested Loop 代价
 */
void cost_nestloop(PhysJoin *node, PlannerContext *root,
                   double inner_rows, double outer_rows);

/**
 * @brief 计算 Merge Join 代价
 */
void cost_mergejoin(PhysJoin *node, PlannerContext *root,
                    double inner_rows, double outer_rows);

/**
 * @brief 计算聚合代价
 */
void cost_agg(PhysAgg *node, PlannerContext *root,
              double rows, int width, int numGroupCols);

/**
 * @brief 计算排序代价
 */
void cost_sort(PhysPlan *node, PlannerContext *root,
               double rows, int width);

/**
 * @brief 计算向量扫描代价
 */
void cost_vector_scan(PhysScan *node, PlannerContext *root,
                      double rows, int vec_dim);

/* ========================================================================
 * 计划器 API
 * ======================================================================== */

/**
 * @brief 创建计划器上下文
 */
PlannerContext *planner_create(void);

/**
 * @brief 销毁计划器上下文
 */
void planner_destroy(PlannerContext *ctx);

/**
 * @brief 创建计划器配置
 */
PlannerConfig *planner_config_create(void);

/**
 * @brief 设置计划器配置
 */
void planner_set_config(PlannerContext *ctx, const PlannerConfig *config);

/**
 * @brief 从 AST 生成逻辑计划
 *
 * @param ctx 计划器上下文
 * @param parse_result 解析结果
 * @return 逻辑计划
 */
LogicalPlan *planner_logical_plan(PlannerContext *ctx, void *parse_result);

/**
 * @brief 从逻辑计划生成物理计划
 *
 * @param ctx 计划器上下文
 * @param logical 逻辑计划
 * @return 物理计划
 */
PhysPlan *planner_physical_plan(PlannerContext *ctx, LogicalPlan *logical);

/**
 * @brief 完整计划生成（逻辑 + 物理）
 */
PhysPlan *planner_plan(PlannerContext *ctx, void *parse_result);

/**
 * @brief 获取表统计信息
 */
TableStats *planner_get_table_stats(PlannerContext *ctx, int relid);

/**
 * @brief 获取列统计信息
 */
ColumnStats *planner_get_column_stats(PlannerContext *ctx, int relid, int attno);

/**
 * @brief 添加统计信息
 */
void planner_add_stats(PlannerContext *ctx, int relid,
                      const TableStats *stats);

/**
 * @brief 估计计划代价
 */
double planner_estimate_cost(PlannerContext *ctx, PhysPlan *plan);

/**
 * @brief 设置代价参数
 */
void planner_set_cost_params(PlannerContext *ctx, const CostParams *params);

/* ========================================================================
 * 优化规则
 * ======================================================================== */

/** 优化规则类型 */
typedef enum OptRuleType_e {
    RULE_PREDICATE_PUSHDOWN,    /**< 谓词下推 */
    RULE_COLUMN_PRUNING,        /**< 列裁剪 */
    RULE_JOIN_REORDERING,       /**< 连接重排序 */
    RULE_JOIN_ELIMINATION,      /**< 连接消除 */
    RULE_AGG_PUSHDOWN,          /**< 聚合下推 */
    RULE_SUBQUERY_FLATTENING,   /**< 子查询扁平化 */
    RULE_WINDOW_PUSHDOWN,       /**< 窗口函数下推 */
    RULE_DISTINCT_PUSHDOWN,     /**< DISTINCT 下推 */
    RULE_LIMIT_PUSHDOWN,        /**< LIMIT 下推 */
    RULE_CONSTANT_FOLDING,      /**< 常量折叠 */
    RULE_MATERIALIZATION,       /**< 物化 */
    RULE_MVIEW_REWRITE          /**< 物化视图改写 */
} OptRuleType;

/**
 * @brief 应用优化规则
 */
void planner_apply_rule(PlannerContext *ctx, LogicalPlan *plan, OptRuleType rule);

/**
 * @brief 应用所有优化规则
 */
void planner_optimize(PlannerContext *ctx, LogicalPlan *plan);

/**
 * @brief 检查是否可以下推谓词
 */
bool planner_can_pushdown_pred(Expr *pred, LogicalPlan *node);

/**
 * @brief 下推谓词
 */
LogicalPlan *planner_pushdown_pred(PlannerContext *ctx, LogicalPlan *plan, Expr *pred);

/**
 * @brief 裁剪不需要的列
 */
LogicalPlan *planner_prune_columns(PlannerContext *ctx, LogicalPlan *plan,
                                   List *required_cols);

/**
 * @brief 重排序连接
 */
LogicalPlan *planner_reorder_joins(PlannerContext *ctx, LogicalPlan *plan);

/**
 * @brief 检查物化视图是否可以改写查询
 */
bool planner_check_mview_rewrite(PlannerContext *ctx, LogicalPlan *plan,
                                 int mview_id);

/**
 * @brief 尝试使用物化视图改写查询
 */
LogicalPlan *planner_rewrite_with_mview(PlannerContext *ctx, LogicalPlan *plan);

/**
 * @brief 检查向量索引是否可以下推
 */
bool planner_can_vector_index(PlannerContext *ctx, LogicalPlan *plan, Expr *pred);

/**
 * @brief 添加向量索引扫描
 */
LogicalPlan *planner_add_vector_index_scan(PlannerContext *ctx, LogicalPlan *plan,
                                          Expr *pred);

/**
 * @brief 分区裁剪优化
 *
 * 检查 WHERE 条件中的分区键，只扫描匹配的分区。
 * 如果表不是分区表，则不做任何操作。
 *
 * @param ctx  PlannerContext
 * @param plan 逻辑计划
 */
void planner_partition_prune(PlannerContext *ctx, LogicalPlan *plan);

/* ========================================================================
 * PlanState 桥接函数
 * ======================================================================== */

/* 使用 execnodes.h 中的 PlanState 定义 */
#ifndef PLANSTATE_DEFINED
#define PLANSTATE_DEFINED
struct PlanState_s;
typedef struct PlanState_s PlanState;
#endif
typedef struct PhysPlan_s PhysPlan;

/**
 * @brief 将物理计划转换为执行器状态树
 */
PlanState *planner_create_plan_state(PhysPlan *plan);

/**
 * @brief 销毁执行器状态树
 */
void planner_destroy_plan_state(PlanState *state);

/* ========================================================================
 * 调试和工具
 * ======================================================================== */

/**
 * @brief 释放逻辑计划
 */
void planner_free_logical_plan(LogicalPlan *plan);

/**
 * @brief 释放物理计划
 */
void planner_free_physical_plan(PhysPlan *plan);

/**
 * @brief 打印逻辑计划
 */
void planner_dump_logical(LogicalPlan *plan, int indent);

/**
 * @brief 打印物理计划
 */
void planner_dump_physical(PhysPlan *plan, int indent);

/**
 * @brief 获取物理算子名称
 */
const char *planner_physical_op_name(PhysicalOpType type);

/**
 * @brief 获取逻辑算子名称
 */
const char *planner_logical_op_name(LogicalOpType type);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_PLANNER_H */
