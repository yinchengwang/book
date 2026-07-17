/**
 * @file cross_model_optimizer.h
 * @brief 跨模型查询优化器头文件
 *
 * 实现跨模型执行计划、异构模型 JOIN、Exchange 算子、谓词下推等
 */
#ifndef DB_CROSS_MODEL_OPTIMIZER_H
#define DB_CROSS_MODEL_OPTIMIZER_H

#include "db/sql/sql_planner.h"
#include "db/storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 跨模型算子类型
 * ======================================================================== */

/**
 * @brief 跨模型算子类型
 */
typedef enum CrossModelOpType_e {
    CROSS_MODEL_SCAN,            /**< 跨模型扫描 */
    CROSS_MODEL_JOIN,            /**< 跨模型连接 */
    CROSS_MODEL_UNION,           /**< 跨模型联合 */
    CROSS_MODEL_EXCHANGE,        /**< 数据交换 */
    CROSS_MODEL_BROADCAST,       /**< 广播 */
    CROSS_MODEL_REPARTITION,     /**< 重分区 */
    CROSS_MODEL_COLLECT,         /**< 收集 */
    CROSS_MODEL_GATHER,          /**< 聚集 */
} CrossModelOpType;

/**
 * @brief 模型类型
 */
typedef enum CrossModelSource_e {
    CROSS_MODEL_RELATIONAL = MODEL_RELATIONAL,  /**< 关系模型 */
    CROSS_MODEL_KV = MODEL_KV,                  /**< KV 模型 */
    CROSS_MODEL_GRAPH = MODEL_GRAPH,            /**< 图模型 */
    CROSS_MODEL_VECTOR = MODEL_VECTOR,          /**< 向量模型 */
    CROSS_MODEL_TIMESERIES = MODEL_TIMESERIES,  /**< 时序模型 */
    CROSS_MODEL_DOCUMENT = MODEL_DOCUMENT,      /**< 文档模型 */
    CROSS_MODEL_SPATIAL = MODEL_SPATIAL,        /**< 空间模型 */
    CROSS_MODEL_TREE = MODEL_TREE,              /**< 树模型 */
} CrossModelSource;

/* ========================================================================
 * 跨模型执行计划结构
 * ======================================================================== */

/** 跨模型扫描信息 */
typedef struct CrossModelScanInfo_s {
    int relid;                   /**< Relation ID */
    CrossModelSource model;      /**< 数据模型 */
    char *collection_name;       /**< 集合/表名 */
    Expr *filter;                /**< 过滤条件 */
    Expr **pushdown_preds;       /**< 下推谓词数组 */
    size_t num_preds;            /**< 谓词数量 */
    List *required_cols;         /**< 需要的列 */
} CrossModelScanInfo;

/** 跨模型 JOIN 信息 */
typedef struct CrossModelJoinInfo_s {
    CrossModelOpType join_type;  /**< JOIN 类型 */
    Expr *join_condition;        /**< 连接条件 */
    CrossModelScanInfo *left;    /**< 左表信息 */
    CrossModelScanInfo *right;   /**< 右表信息 */
    bool is_heterogeneous;       /**< 是否异构模型 */
    CrossModelSource left_model; /**< 左表模型 */
    CrossModelSource right_model;/**< 右表模型 */
} CrossModelJoinInfo;

/** 跨模型算子节点 */
typedef struct CrossModelPlanNode_s {
    CrossModelOpType type;       /**< 算子类型 */
    int node_id;                 /**< 节点 ID */
    double estimated_cost;       /**< 估计代价 */
    double estimated_rows;       /**< 估计行数 */
    union {
        CrossModelScanInfo scan;     /**< 扫描信息 */
        CrossModelJoinInfo join;     /**< JOIN 信息 */
        struct {
            CrossModelOpType exchange_type;
            int num_partitions;
            int *partition_keys;
        } exchange;
    } info;
    struct CrossModelPlanNode_s *children[4]; /**< 子节点 */
} CrossModelPlanNode;

/** 跨模型执行计划 */
typedef struct CrossModelPlan_s {
    CrossModelPlanNode *root;    /**< 根节点 */
    int num_partitions;          /**< 分区数 */
    List *scan_nodes;           /**< 扫描节点列表 */
    List *join_nodes;           /**< JOIN 节点列表 */
    double total_cost;           /**< 总代价 */
} CrossModelPlan;

/* ========================================================================
 * Exchange 算子
 * ======================================================================== */

/** Exchange 类型 */
typedef enum ExchangeType_e {
    EXCHANGE_BROADCAST = 0,      /**< 广播 */
    EXCHANGE_HASH_PARTITION,     /**< Hash 分区 */
    EXCHANGE_RANGE_PARTITION,    /**< 范围分区 */
    EXCHANGE_ROUND_ROBIN,        /**< 轮询分区 */
    EXCHANGE_GATHER,             /**< 收集到单节点 */
    EXCHANGE_ALL_GATHER,         /**< 全收集 */
} ExchangeType;

/** Exchange 节点配置 */
typedef struct ExchangeConfig_s {
    ExchangeType type;           /**< Exchange 类型 */
    int num_output_partitions;   /**< 输出分区数 */
    int *partition_columns;      /**< 分区列索引 */
    int num_partition_cols;      /**< 分区列数 */
    bool preserve_order;         /**< 保持顺序 */
    double selectivity;          /**< 选择率 */
} ExchangeConfig;

/**
 * @brief 创建跨模型计划
 */
CrossModelPlan *cross_model_plan_create(void);

/**
 * @brief 销毁跨模型计划
 */
void cross_model_plan_destroy(CrossModelPlan *plan);

/**
 * @brief 添加扫描节点
 */
CrossModelPlanNode *cross_model_add_scan(CrossModelPlan *plan,
                                         CrossModelSource model,
                                         const char *collection,
                                         Expr *filter);

/**
 * @brief 添加 JOIN 节点
 */
CrossModelPlanNode *cross_model_add_join(CrossModelPlan *plan,
                                         CrossModelPlanNode *left,
                                         CrossModelPlanNode *right,
                                         Expr *condition,
                                         CrossModelOpType join_type);

/**
 * @brief 添加 Exchange 节点
 */
CrossModelPlanNode *cross_model_add_exchange(CrossModelPlan *plan,
                                             CrossModelPlanNode *child,
                                             const ExchangeConfig *config);

/**
 * @brief 添加广播节点
 */
CrossModelPlanNode *cross_model_add_broadcast(CrossModelPlan *plan,
                                              CrossModelPlanNode *child,
                                              int num_replicas);

/* ========================================================================
 * 跨模型优化规则
 * ======================================================================== */

/** 优化选项 */
typedef struct CrossModelOptOptions_s {
    bool enable_predicate_pushdown;     /**< 启用谓词下推 */
    bool enable_column_pruning;         /**< 启用列裁剪 */
    bool enable_broadcast_small_table;  /**< 启用小表广播 */
    bool enable_join_reordering;        /**< 启用连接重排序 */
    double small_table_threshold;       /**< 小表阈值（行数） */
} CrossModelOptOptions;

/**
 * @brief 创建优化选项
 */
CrossModelOptOptions *cross_model_opt_options_create(void);

/**
 * @brief 设置默认优化选项
 */
void cross_model_set_default_options(CrossModelOptOptions *opts);

/**
 * @brief 优化跨模型计划
 * @param plan 原始计划
 * @param opts 优化选项
 * @return 优化后的计划
 */
CrossModelPlan *cross_model_optimize(CrossModelPlan *plan,
                                     const CrossModelOptOptions *opts);

/**
 * @brief 谓词下推到扫描节点
 */
bool cross_model_pushdown_predicate(CrossModelPlan *plan,
                                   CrossModelPlanNode *scan_node,
                                   Expr *predicate);

/**
 * @brief 分析异构模型 JOIN
 */
bool cross_model_analyze_heterogeneous_join(CrossModelJoinInfo *join_info,
                                            CrossModelPlanNode **optimized_join);

/**
 * @brief 检查是否可以下推谓词到指定模型
 */
bool cross_model_can_pushdown_to_model(Expr *predicate, CrossModelSource model);

/**
 * @brief 获取可下推谓词
 */
Expr **cross_model_get_pushdown_preds(Expr *predicates, size_t *out_count,
                                      CrossModelSource model);

/**
 * @brief 重排序 JOIN 顺序
 */
CrossModelPlan *cross_model_reorder_joins(CrossModelPlan *plan);

/**
 * @brief 优化小表广播
 */
CrossModelPlan *cross_model_optimize_broadcast(CrossModelPlan *plan,
                                               double threshold);

/**
 * @brief 裁剪不需要的列
 */
CrossModelPlan *cross_model_prune_columns(CrossModelPlan *plan,
                                         List *required_cols);

/* ========================================================================
 * 代价估算
 * ======================================================================== */

/**
 * @brief 估算扫描代价
 */
double cross_model_estimate_scan_cost(CrossModelScanInfo *scan,
                                      double num_rows);

/**
 * @brief 估算 JOIN 代价
 */
double cross_model_estimate_join_cost(CrossModelJoinInfo *join,
                                     double left_rows,
                                     double right_rows);

/**
 * @brief 估算 Exchange 代价
 */
double cross_model_estimate_exchange_cost(ExchangeConfig *config,
                                          double num_rows,
                                          double row_size);

/**
 * @brief 估算总代价
 */
double cross_model_estimate_total_cost(CrossModelPlan *plan);

/**
 * @brief 选择最优计划
 * @param plans 候选计划数组
 * @param num_plans 计划数量
 * @return 最优计划索引
 */
int cross_model_select_best_plan(CrossModelPlan **plans, size_t num_plans);

/* ========================================================================
 * 计划执行
 * ======================================================================== */

/** 执行器上下文 */
typedef struct CrossModelExecContext_s {
    void *relational_exec;       /**< 关系模型执行器 */
    void *kv_exec;               /**< KV 执行器 */
    void *vector_exec;           /**< 向量执行器 */
    void *timeseries_exec;       /**< 时序执行器 */
    void *graph_exec;            /**< 图执行器 */
    void *doc_exec;              /**< 文档执行器 */
    void *spatial_exec;          /**< 空间执行器 */
    void *tree_exec;             /**< 树执行器 */
} CrossModelExecContext;

/**
 * @brief 创建跨模型执行上下文
 */
CrossModelExecContext *cross_model_exec_create(void);

/**
 * @brief 销毁跨模型执行上下文
 */
void cross_model_exec_destroy(CrossModelExecContext *ctx);

/**
 * @brief 执行跨模型计划
 * @param ctx 执行上下文
 * @param plan 执行计划
 * @param output 结果输出槽
 * @return 下一行是否可用
 */
bool cross_model_exec_next(CrossModelExecContext *ctx,
                          CrossModelPlan *plan,
                          TupleTableSlot *output);

/**
 * @brief 初始化执行上下文
 */
int cross_model_exec_init(CrossModelExecContext *ctx, CrossModelPlan *plan);

/**
 * @brief 清理执行上下文
 */
void cross_model_exec_cleanup(CrossModelExecContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_CROSS_MODEL_OPTIMIZER_H */
